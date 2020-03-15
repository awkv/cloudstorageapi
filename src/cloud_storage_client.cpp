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
#include "cloudstorageapi/internal/clients/curl_client_factory.h"
#include "cloudstorageapi/internal/log.h"
#include "cloudstorageapi/internal/utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace csa {
static_assert(std::is_copy_constructible<csa::CloudStorageClient>::value,
    "csa::CloudStorageClient must be constructible");
static_assert(std::is_copy_assignable<csa::CloudStorageClient>::value,
    "csa::CloudStorageClient must be assignable");

StatusOrVal<CloudStorageClient> CloudStorageClient::CreateDefaultClient(EProvider provider)
{
    auto opts = ClientOptions::CreateDefaultClientOptions(provider);
    if (!opts)
    {
        return StatusOrVal<CloudStorageClient>(opts.GetStatus());
    }
    return StatusOrVal<CloudStorageClient>(CloudStorageClient(*opts));
}

std::shared_ptr<internal::RawClient> CloudStorageClient::CreateDefaultRawClient(ClientOptions options)
{
    return internal::CurlClientFactory::CreateClient(std::move(options));
}

bool CloudStorageClient::UseSimpleUpload(std::string const& file_name) const
{
    auto status = std::filesystem::status(file_name);
    if (!std::filesystem::is_regular_file(status))
    {
        return false;
    }
    auto size = std::filesystem::file_size(file_name);
    return size <= m_RawClient->GetClientOptions().GetMaximumSimpleUploadSize();
}

StatusOrVal<FileMetadata> CloudStorageClient::UploadFileSimple(
    std::string const& fileName, internal::InsertFileRequest request)
{
    std::ifstream is(fileName, std::ios::binary);
    if (!is.is_open())
    {
        std::ostringstream os;
        os << __func__ << "(" << request << ", " << fileName
            << "): cannot open upload file source";
        return Status(StatusCode::NotFound, std::move(os).str());
    }

    std::string payload(std::istreambuf_iterator<char>{is}, {});
    request.SetContent(std::move(payload));

    return m_RawClient->InsertFile(request);
}

StatusOrVal<FileMetadata> CloudStorageClient::UploadFileResumable(
    std::string const& fileName,
    internal::ResumableUploadRequest const& request)
{
    auto fileStatus = std::filesystem::status(fileName);
    if (!std::filesystem::is_regular_file(fileStatus))
    {
        CSA_LOG_WARNING("Trying to upload {} which is not a regular file.\n"
            "This is often a problem because:\n"
            "  - Some non-regular files are infinite sources of data, and the load will never complete.\n"
            "  - Some non-regular files can only be read once, and UploadFile() may need to\n"
            "    read the file more than once to compute the checksum and hashes needed to\n"
            "    preserve data integrity.\n"
            "\n"
            "Consider using CloudStorageClient::WriteFile() instead.", fileName);
    }

    std::ifstream source(fileName, std::ios::binary);
    if (!source.is_open())
    {
        std::ostringstream os;
        os << __func__ << "(" << request << ", " << fileName
            << "): cannot open upload file source";
        return Status(StatusCode::NotFound, std::move(os).str());
    }

    return UploadStreamResumable(source, request);
}

StatusOrVal<FileMetadata> CloudStorageClient::UploadStreamResumable(
    std::istream& source, internal::ResumableUploadRequest const& request)
{
    StatusOrVal<std::unique_ptr<internal::ResumableUploadSession>> sessionStatus =
        m_RawClient->CreateResumableSession(request);
    if (!sessionStatus)
    {
        return std::move(sessionStatus).GetStatus();
    }

    auto session = std::move(*sessionStatus);

    // Some cloud storages require chunks to be a multiple of some quantum in size.
    auto chunkSize = internal::RoundUpToQuantum(
        m_RawClient->GetClientOptions().GetUploadBufferSize(), m_RawClient->GetFileChunkQuantum());

    StatusOrVal<internal::ResumableUploadResponse> uploadResponse(
        internal::ResumableUploadResponse{});

    // We iterate while `source` is good and the retry policy has not been
    // exhausted.
    while (!source.eof() && uploadResponse &&
        !uploadResponse->m_payload.has_value())
    {
        // Read a chunk of data from the source file.
        std::string buffer(chunkSize, '\0');
        source.read(buffer.data(), buffer.size());
        auto gcount = static_cast<std::size_t>(source.gcount());
        bool finalChunk = (gcount < buffer.size());
        auto sourceSize = session->GetNextExpectedByte() + gcount;
        buffer.resize(gcount);

        auto expected = session->GetNextExpectedByte() + gcount;
        if (finalChunk)
        {
            uploadResponse = session->UploadFinalChunk(buffer, sourceSize);
        }
        else
        {
            uploadResponse = session->UploadChunk(buffer);
        }
        if (!uploadResponse)
        {
            return std::move(uploadResponse).GetStatus();
        }
        if (session->GetNextExpectedByte() != expected)
        {
            CSA_LOG_WARNING("Unexpected last committed byte: "
                " expected={} got={}", expected,
                session->GetNextExpectedByte());
            source.seekg(session->GetNextExpectedByte(), std::ios::beg);
        }
    }

    if (!uploadResponse)
    {
        return std::move(uploadResponse).GetStatus();
    }

    return *std::move(uploadResponse->m_payload);
}

FileWriteStream CloudStorageClient::WriteObjectImpl(
    internal::ResumableUploadRequest const& request)
{
    auto session = m_RawClient->CreateResumableSession(request);
    if (!session)
    {
        auto error = std::make_unique<
            internal ::ResumableUploadSessionError>(std::move(session).GetStatus());

        FileWriteStream errorStream(std::make_unique<
            internal::FileWriteStreambuf>(
                std::move(error), 0));
        errorStream.setstate(std::ios::badbit | std::ios::eofbit);
        errorStream.Close();
        return errorStream;
    }

    return FileWriteStream(
        std::make_unique<internal::FileWriteStreambuf>(
            *std::move(session),
            m_RawClient->GetClientOptions().GetUploadBufferSize()));
}

FileReadStream CloudStorageClient::ReadObjectImpl(
    internal::ReadFileRangeRequest const& request)
{
    auto source = m_RawClient->ReadFile(request);
    if (!source)
    {
        FileReadStream errorStream(
            std::make_unique<internal::FileReadStreambuf>(
                request, std::move(source).GetStatus()));
        errorStream.setstate(std::ios::badbit | std::ios::eofbit);
        return errorStream;
    }
    auto stream = FileReadStream(
        std::make_unique<internal::FileReadStreambuf>(
            request, *std::move(source)));
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
    auto report_error = [&request, dstFileName](char const* func, char const* what,
        Status const& status)
    {
        std::ostringstream msg;
        msg << func << "(" << request << ", " << dstFileName << "): " << what
            << " - status.message=" << status.Message();
        return Status(status.Code(), std::move(msg).str());
    };

    auto stream = ReadObjectImpl(request);
    if (!stream.GetStatus().Ok())
    {
        return report_error(__func__, "cannot open download source object",
            stream.GetStatus());
    }

    // Open the destination file, and immediate raise an exception on failure.
    std::ofstream os(dstFileName, std::ios::binary);
    if (!os.is_open()) {
        return report_error(
            __func__, "cannot open download destination file",
            Status(StatusCode::InvalidArgument, "ofstream::open()"));
    }

    std::string buffer;
    buffer.resize(m_RawClient->GetClientOptions().GetDownloadBufferSize(), '\0');
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
        return report_error(__func__, "error reading download source object",
            stream.GetStatus());
    }
    return Status();
}

}; // namespace csa
