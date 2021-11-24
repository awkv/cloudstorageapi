// Copyright 2019 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/cloud_storage_client.h"
#include "cloudstorageapi/internal/algorithm.h"
#include "cloudstorageapi/internal/clients/curl_client_factory.h"
#include "cloudstorageapi/internal/log.h"
#include "cloudstorageapi/internal/utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace csa {
static_assert(std::is_copy_constructible<csa::CloudStorageClient>::value,
              "csa::CloudStorageClient must be constructible");
static_assert(std::is_copy_assignable<csa::CloudStorageClient>::value, "csa::CloudStorageClient must be assignable");

CloudStorageClient::CloudStorageClient(Options opts)
    : CloudStorageClient(CloudStorageClient::CreateDefaultRawClient(std::move(opts)))
{
}

std::shared_ptr<internal::RawClient> CloudStorageClient::CreateDefaultRawClient(Options options)
{
    if (!options.Has<ProviderOption>())
        return nullptr;
    options = internal::CreateDefaultOptionsWithCredentials(std::move(options));
    return CreateDefaultRawClient(options, internal::CurlClientFactory::CreateClient(options));
}

std::shared_ptr<internal::RawClient> CloudStorageClient::CreateDefaultRawClient(
    Options const& options, std::shared_ptr<internal::RawClient> client)
{
    auto contains_fn = [](auto& c, auto& v) {
        auto e = std::end(c);
        return e != std::find(std::begin(c), e, v);
    };
    auto const& tracing_opt = options.Get<TracingComponentsOption>();
    auto const enable_logging = contains_fn(tracing_opt, "raw-client") || contains_fn(tracing_opt, "http");
    if (enable_logging)
        client = std::make_shared<internal::LoggingClient>(std::move(client));

    return std::make_shared<internal::RetryClient>(std::move(client), options);
}

bool CloudStorageClient::UseSimpleUpload(std::string const& file_name, std::size_t& size) const
{
    auto status = std::filesystem::status(file_name);
    if (!std::filesystem::is_regular_file(status))
    {
        return false;
    }
    auto const fs = std::filesystem::file_size(file_name);
    if (fs <= m_RawClient->GetOptions().Get<MaximumSimpleUploadSizeOption>())
    {
        size = static_cast<std::size_t>(fs);
        return true;
    }
    return false;
}

StatusOrVal<FileMetadata> CloudStorageClient::UploadFileSimple(std::string const& fileName, std::size_t fileSize,
                                                               internal::InsertFileRequest request)
{
    auto uploadOffset = request.GetOption<UploadFromOffset>().ValueOr(0);
    if (fileSize < uploadOffset)
    {
        std::ostringstream os;
        os << __func__ << "(" << request << ", " << fileName << "): UploadFromOffset (" << uploadOffset
           << ") is bigger than the size of file source (" << fileSize << ")";
        return Status(StatusCode::InvalidArgument, std::move(os).str());
    }

    auto uploadSize =
        (std::min)(request.GetOption<UploadLimit>().ValueOr(fileSize - uploadOffset), fileSize - uploadOffset);

    std::ifstream is(fileName, std::ios::binary);
    if (!is.is_open())
    {
        std::ostringstream os;
        os << __func__ << "(" << request << ", " << fileName << "): cannot open upload file source";
        return Status(StatusCode::NotFound, std::move(os).str());
    }

    std::string payload(static_cast<std::size_t>(uploadSize), char{});
    is.seekg(uploadOffset, std::ios::beg);
    is.read(&payload[0], payload.size());
    if (static_cast<std::size_t>(is.gcount()) < payload.size())
    {
        std::ostringstream os;
        os << __func__ << "(" << request << ", " << fileName << "): Actual read (" << is.gcount()
           << ") is smaller than uploadSize (" << payload.size() << ")";
        return Status(StatusCode::Internal, std::move(os).str());
    }

    is.close();
    request.SetContent(std::move(payload));

    return m_RawClient->InsertFile(request);
}

StatusOrVal<FileMetadata> CloudStorageClient::UploadFileResumable(std::string const& fileName,
                                                                  internal::ResumableUploadRequest request)
{
    auto uploadOffset = request.GetOption<UploadFromOffset>().ValueOr(0);
    auto fileStatus = std::filesystem::status(fileName);
    if (!std::filesystem::is_regular_file(fileStatus))
    {
        CSA_LOG_WARNING(
            "Trying to upload {} which is not a regular file.\n"
            "This is often a problem because:\n"
            "  - Some non-regular files are infinite sources of data, and the load will never complete.\n"
            "  - Some non-regular files can only be read once, and UploadFile() may need to\n"
            "    read the file more than once to compute the checksum and hashes needed to\n"
            "    preserve data integrity.\n"
            "\n"
            "Consider using CloudStorageClient::WriteFile() instead.",
            fileName);
    }
    else
    {
        std::error_code sizeErr;
        auto fileSize = std::filesystem::file_size(fileName, sizeErr);
        if (sizeErr)
        {
            return Status(StatusCode::NotFound, sizeErr.message());
        }
        if (fileSize < uploadOffset)
        {
            std::ostringstream os;
            os << __func__ << "(" << request << ", " << fileName << "): UploadFromOffset (" << uploadOffset
               << ") is bigger than the size of file source (" << fileSize << ")";
            return Status(StatusCode::InvalidArgument, std::move(os).str());
        }

        auto uploadSize =
            (std::min)(request.GetOption<UploadLimit>().ValueOr(fileSize - uploadOffset), fileSize - uploadOffset);
        request.SetOption(UploadContentLength(uploadSize));
    }

    std::ifstream source(fileName, std::ios::binary);
    if (!source.is_open())
    {
        std::ostringstream os;
        os << __func__ << "(" << request << ", " << fileName << "): cannot open upload file source";
        return Status(StatusCode::NotFound, std::move(os).str());
    }

    // We set its offset before passing it to `UploadStreamResumable` so we don't
    // need to compute `UploadFromOffset` again.
    source.seekg(uploadOffset, std::ios::beg);

    return UploadStreamResumable(source, request);
}

StatusOrVal<FileMetadata> CloudStorageClient::UploadStreamResumable(std::istream& source,
                                                                    internal::ResumableUploadRequest const& request)
{
    StatusOrVal<std::unique_ptr<internal::ResumableUploadSession>> sessionStatus =
        m_RawClient->CreateResumableSession(request);
    if (!sessionStatus)
    {
        return std::move(sessionStatus).GetStatus();
    }

    auto session = std::move(*sessionStatus);

    // How many bytes of the local file are uploaded to the GCS server.
    auto serverSize = session->GetNextExpectedByte();
    auto uploadLimit = request.GetOption<UploadLimit>().ValueOr((std::numeric_limits<std::uint64_t>::max)());
    // If `serverSize == uploadLimit`, we will upload an empty string and
    // finalize the upload.
    if (serverSize > uploadLimit)
    {
        return Status(StatusCode::OutOfRange, "UploadLimit (" + std::to_string(uploadLimit) +
                                                  ") is not bigger than the uploaded size (" +
                                                  std::to_string(serverSize) + ") on cloud storage server");
    }
    source.seekg(serverSize, std::ios::cur);

    // Some cloud storages require chunks to be a multiple of some quantum in size.
    auto chunkSize = internal::RoundUpToQuantum(m_RawClient->GetOptions().Get<UploadBufferSizeOption>(),
                                                m_RawClient->GetFileChunkQuantum());

    StatusOrVal<internal::ResumableUploadResponse> uploadResponse(internal::ResumableUploadResponse{});

    // We iterate while `source` is good, the upload size does not reach the
    // `UploadLimit` and the retry policy has not been exhausted.
    bool reachUploadLimit = false;
    internal::ConstBufferSequence buffers(1);
    std::vector<char> buffer(chunkSize);
    while (!source.eof() && uploadResponse && !uploadResponse->m_payload.has_value() && !reachUploadLimit)
    {
        // Read a chunk of data from the source file.
        if (uploadLimit - serverSize <= chunkSize)
        {
            // We don't want the `source_size` to exceed `upload_limit`.
            chunkSize = static_cast<std::size_t>(uploadLimit - serverSize);
            reachUploadLimit = true;
        }
        source.read(buffer.data(), buffer.size());
        auto gcount = static_cast<std::size_t>(source.gcount());
        bool finalChunk = (gcount < buffer.size()) || reachUploadLimit;
        auto sourceSize = session->GetNextExpectedByte() + gcount;
        auto expected = sourceSize;
        buffers[0] = internal::ConstBuffer{buffer.data(), gcount};
        if (finalChunk)
        {
            uploadResponse = session->UploadFinalChunk(buffers, sourceSize);
        }
        else
        {
            uploadResponse = session->UploadChunk(buffers);
        }
        if (!uploadResponse)
        {
            return std::move(uploadResponse).GetStatus();
        }
        if (session->GetNextExpectedByte() != expected)
        {
            // Defensive programming: unless there is a bug, this should be dead code.
            return Status(StatusCode::Internal, "Unexpected last committed byte expected=" + std::to_string(expected) +
                                                    " got=" + std::to_string(session->GetNextExpectedByte()) +
                                                    ". This is a bug, please report it at "
                                                    "https://github.com/awkv/cloudstorageapi/issues/new");
        }
        // We only update `serverSize` when uploading is successful.
        serverSize = expected;
    }

    if (!uploadResponse)
    {
        return std::move(uploadResponse).GetStatus();
    }

    return *std::move(uploadResponse->m_payload);
}

FileWriteStream CloudStorageClient::WriteObjectImpl(internal::ResumableUploadRequest const& request)
{
    auto session = m_RawClient->CreateResumableSession(request);
    if (!session)
    {
        auto error = std::make_unique<internal ::ResumableUploadSessionError>(std::move(session).GetStatus());

        FileWriteStream errorStream(
            std::make_unique<internal::FileWriteStreambuf>(std::move(error), 0, AutoFinalizeConfig::Disabled));
        errorStream.setstate(std::ios::badbit | std::ios::eofbit);
        errorStream.Close();
        return errorStream;
    }

    return FileWriteStream(std::make_unique<internal::FileWriteStreambuf>(
        *std::move(session), m_RawClient->GetOptions().Get<UploadBufferSizeOption>(),
        request.GetOption<AutoFinalize>().ValueOr(AutoFinalizeConfig::Enabled)));
}

FileReadStream CloudStorageClient::ReadObjectImpl(internal::ReadFileRangeRequest const& request)
{
    auto source = m_RawClient->ReadFile(request);
    if (!source)
    {
        FileReadStream errorStream(
            std::make_unique<internal::FileReadStreambuf>(request, std::move(source).GetStatus()));
        errorStream.setstate(std::ios::badbit | std::ios::eofbit);
        return errorStream;
    }
    auto stream = FileReadStream(std::make_unique<internal::FileReadStreambuf>(
        request, *std::move(source), request.GetOption<ReadFromOffset>().ValueOr(0)));
    (void)stream.peek();

    // Without exceptions the streambuf cannot report errors, so we have to
    // manually update the status bits.
    if (!stream.GetStatus().Ok())
    {
        stream.setstate(std::ios::badbit | std::ios::eofbit);
    }

    return stream;
}

Status CloudStorageClient::DownloadFileImpl(internal::ReadFileRangeRequest const& request,
                                            std::string const& dstFileName)
{
    auto report_error = [&request, dstFileName](char const* func, char const* what, Status const& status) {
        std::ostringstream msg;
        msg << func << "(" << request << ", " << dstFileName << "): " << what
            << " - status.message=" << status.Message();
        return Status(status.Code(), std::move(msg).str());
    };

    auto stream = ReadObjectImpl(request);
    if (!stream.GetStatus().Ok())
    {
        return report_error(__func__, "cannot open download source object", stream.GetStatus());
    }

    // Open the destination file, and immediate raise an exception on failure.
    std::ofstream os(dstFileName, std::ios::binary);
    if (!os.is_open())
    {
        return report_error(__func__, "cannot open download destination file",
                            Status(StatusCode::InvalidArgument, "ofstream::open()"));
    }

    std::string buffer;
    buffer.resize(m_RawClient->GetOptions().Get<DownloadBufferSizeOption>(), '\0');
    do
    {
        stream.read(&buffer[0], buffer.size());
        os.write(buffer.data(), stream.gcount());
    } while (os.good() && stream.good());

    os.close();
    if (!os.good())
    {
        return report_error(__func__, "cannot close download destination file",
                            Status(StatusCode::Unknown, "ofstream::close()"));
    }
    if (!stream.GetStatus().Ok())
    {
        return report_error(__func__, "error reading download source object", stream.GetStatus());
    }
    return Status();
}

};  // namespace csa
