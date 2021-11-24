// Copyright 2021 Andrew Karasyov
//
// Copyright 2021 Google LLC
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

#include "cloudstorageapi/internal/file_read_streambuf.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/utils.h"
#include <algorithm>
#include <sstream>

namespace csa {
namespace internal {

FileReadStreambuf::FileReadStreambuf(ReadFileRangeRequest const& request, std::unique_ptr<ObjectReadSource> source,
                                     std::streamoff posInStream)
    : m_source(std::move(source)), m_sourcePos(posInStream)
{
}

FileReadStreambuf::FileReadStreambuf(ReadFileRangeRequest const&, Status status)
    : m_source(new ObjectReadErrorSource(status)), m_sourcePos(-1), m_status(std::move(status))
{
}

FileReadStreambuf::pos_type FileReadStreambuf::seekpos(pos_type /*pos*/, std::ios_base::openmode /*which*/)
{
    // TODO: Implement proper seeking.
    return -1;
}

FileReadStreambuf::pos_type FileReadStreambuf::seekoff(off_type off, std::ios_base::seekdir dir,
                                                       std::ios_base::openmode which)
{
    // TODO: Implement proper seeking.
    // Seeking is non-trivial because the hash validator (if any) and `m_source` have to be
    // recreated in the general case, which doesn't fit the current code
    // organization.  We can, however, at least implement the bare minimum of this
    // function allowing `tellg()` to work.
    if (which == std::ios_base::in && dir == std::ios_base::cur && off == 0)
    {
        return m_sourcePos - in_avail();
    }
    return -1;
}

bool FileReadStreambuf::IsOpen() const { return m_source->IsOpen(); }

void FileReadStreambuf::Close()
{
    auto response = m_source->Close();
    if (!response.Ok())
    {
        ReportError(std::move(response).GetStatus());
    }
}

FileReadStreambuf::int_type FileReadStreambuf::ReportError(Status status)
{
    // The only way to report errors from a std::basic_streambuf<> (which this
    // class derives from) is to throw exceptions:
    //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
    // but we need to be able to report errors when the application has disabled
    // exceptions via `-fno-exceptions` or a similar option. In that case we set
    // `m_status`, and report the error as an EOF. This is obviously not ideal,
    // but it is the best we can do when the application disables the standard
    // mechanism to signal errors.
    if (status.Ok())
    {
        return traits_type::eof();
    }
    m_status = std::move(status);

    throw RuntimeStatusError(std::move(status));
}

bool FileReadStreambuf::CheckPreconditions(char const* function_name)
{
    if (in_avail() != 0)
        return true;
    return m_status.Ok() && IsOpen();
}

FileReadStreambuf::int_type FileReadStreambuf::underflow()
{
    if (!CheckPreconditions(__func__))
        return traits_type::eof();

    // If this function is called, then the internal buffer must be empty. We will
    // perform a read into a new buffer and reset the input area to use this
    // buffer.
    auto constexpr InitialPeekRead = 128 * 1024;
    std::vector<char> buffer(InitialPeekRead);
    auto const offset = xsgetn(buffer.data(), InitialPeekRead);
    if (offset == 0)
        return traits_type::eof();

    buffer.resize(static_cast<std::size_t>(offset));
    buffer.swap(m_currentIosBuffer);
    char* data = m_currentIosBuffer.data();
    setg(data, data, data + m_currentIosBuffer.size());
    return traits_type::to_int_type(*data);
}

std::streamsize FileReadStreambuf::xsgetn(char* s, std::streamsize count)
{
    if (!CheckPreconditions(__func__))
        return 0;

    // This function optimizes stream.read(), the data is copied directly from the
    // data source (typically libcurl) into a buffer provided by the application.
    std::streamsize offset = 0;

    // Maybe the internal get area is enough to satisfy this request, no need to
    // read more in that case:
    auto fromInternal = (std::min)(count, in_avail());
    if (fromInternal > 0)
    {
        std::memcpy(s, gptr(), static_cast<std::size_t>(fromInternal));
    }
    gbump(static_cast<int>(fromInternal));
    offset += fromInternal;
    // If we got all the data requested, there is no need for additional reads.
    // Likewise, if the underlying transport is closed, whatever we got is all the
    // data available.
    if (offset >= count || !IsOpen())
        return offset;

    auto const* function_name = __func__;

    StatusOrVal<ReadSourceResult> read = m_source->Read(s + offset, static_cast<std::size_t>(count - offset));
    // If there was an error set the internal state, but we still return the
    // number of bytes.
    if (!read)
    {
        ReportError(std::move(read).GetStatus());
        return offset;
    }

    offset += static_cast<std::streamsize>(read->m_bytesReceived);
    m_sourcePos += static_cast<std::streamoff>(read->m_bytesReceived);

    for (auto const& kv : read->m_response.m_headers)
    {
        m_headers.emplace(kv.first, kv.second);
    }

    ReportError(Status());
    return offset;
}

}  // namespace internal
}  // namespace csa
