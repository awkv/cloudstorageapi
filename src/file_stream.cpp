// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/file_stream.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/log.h"

namespace csa {

static_assert(std::is_move_assignable<FileReadStream>::value, "FileReadStream must be move assignable.");
static_assert(std::is_move_constructible<FileReadStream>::value, "FileReadStream must be move constructible.");

FileReadStream::~FileReadStream()
{
    if (!IsOpen())
    {
        return;
    }
    try
    {
        Close();
    }
    catch (std::exception const& ex)
    {
        CSA_LOG_INFO("Ignored exception while trying to close stream: {}", ex.what());
    }
    catch (...)
    {
        CSA_LOG_INFO("Ignored unknown exception while trying to close stream");
    }
}

void FileReadStream::Close()
{
    if (!IsOpen())
    {
        return;
    }
    m_buf->Close();
    if (!GetStatus().Ok())
    {
        setstate(std::ios_base::badbit);
    }
}

////////////////////////////////////////////////////////////////////////////////

FileWriteStream::FileWriteStream(std::unique_ptr<internal::FileWriteStreambuf> buf)
    : std::basic_ostream<char>(nullptr), m_buf(std::move(buf))
{
    init(m_buf.get());
    // If m_buf is already closed, update internal state to represent
    // the fact that no more bytes can be uploaded to this object.
    if (!m_buf->IsOpen())
    {
        CloseBuf();
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
    Close();
}

void FileWriteStream::Close()
{
    if (!m_buf)
    {
        return;
    }
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
    m_metadata = *std::move(response->m_payload);
}

void FileWriteStream::Suspend() && { m_buf.reset(); }

}  // namespace csa
