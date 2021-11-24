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

#include "cloudstorageapi/file_write_stream.h"
//#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/log.h"

namespace csa {
namespace {
std::unique_ptr<internal::FileWriteStreambuf> MakeErrorStreambuf()
{
    return std::make_unique<internal::FileWriteStreambuf>(
        std::make_unique<internal::ResumableUploadSessionError>(Status(StatusCode::Unimplemented, "null stream")),
        /*max_buffer_size=*/0, AutoFinalizeConfig::Disabled);
}
}  // namespace

static_assert(std::is_move_assignable<FileWriteStream>::value, "FileWriteStream must be move assignable.");
static_assert(std::is_move_constructible<FileWriteStream>::value, "FileWriteStream must be move constructible.");

FileWriteStream::FileWriteStream() : FileWriteStream(MakeErrorStreambuf()) {}

FileWriteStream::FileWriteStream(std::unique_ptr<internal::FileWriteStreambuf> buf)
    : std::basic_ostream<char>(buf.get()), m_buf(std::move(buf))
{
    // If m_buf is already closed, update internal state to represent
    // the fact that no more bytes can be uploaded to this object.
    if (m_buf && !m_buf->IsOpen())
        CloseBuf();
}

FileWriteStream::FileWriteStream(FileWriteStream&& rhs) noexcept
    : std::basic_ostream<char>(std::move(rhs)),
      // The spec guarantees the base class move constructor only changes a few
      // member variables in `std::basic_istream<>`, and there is no spooky
      // action through virtual functions because there are no virtual
      // functions.  A good summary of the specification is "it calls the
      // default constructor and then calls std::basic_ios<>::move":
      //   https://en.cppreference.com/w/cpp/io/basic_ios/move
      // In fact, as that page indicates, the base classes are designed such
      // that derived classes can define their own move constructor and move
      // assignment.
      m_buf(std::move(rhs.m_buf)),
      m_metadata(std::move(rhs.m_metadata)),
      m_headers(std::move(rhs.m_headers)),
      m_payload(std::move(rhs.m_payload))
{
    rhs.m_buf = MakeErrorStreambuf();
    rhs.set_rdbuf(rhs.m_buf.get());
    set_rdbuf(m_buf.get());
    if (!m_buf)
    {
        setstate(std::ios::badbit | std::ios::eofbit);
    }
    else
    {
        if (!m_buf->GetLastStatus().Ok())
            setstate(std::ios::badbit);
        if (!m_buf->IsOpen())
            setstate(std::ios::eofbit);
    }
}

FileWriteStream::~FileWriteStream()
{
    if (!IsOpen())
    {
        return;
    }
    // Disable exceptions, even if the application had enabled exceptions the
    // destructor is supposed to mask them.
    exceptions(std::ios_base::goodbit);
    m_buf->AutoFlushFinal();
}

void FileWriteStream::Close()
{
    if (!m_buf)
        return;
    CloseBuf();
}

void FileWriteStream::CloseBuf()
{
    auto response = m_buf->Close();
    if (!response.Ok())
    {
        m_metadata = std::move(response).GetStatus();
        setstate(std::ios_base::badbit);
        return;
    }
    m_headers = {};
    if (response->m_payload.has_value())
    {
        m_metadata = *std::move(response->m_payload);
    }
}

void FileWriteStream::Suspend() &&
{
    FileWriteStream tmp;
    swap(tmp);
    tmp.m_buf.reset();
}

}  // namespace csa
