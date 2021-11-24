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

#include "cloudstorageapi/file_read_stream.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/log.h"

namespace csa {

namespace {
std::unique_ptr<internal::FileReadStreambuf> MakeErrorStreambuf()
{
    return std::make_unique<internal::FileReadStreambuf>(internal::ReadFileRangeRequest(""),
                                                         Status(StatusCode::Unimplemented, "null stream"));
}
}  // namespace

static_assert(std::is_move_assignable<FileReadStream>::value, "FileReadStream must be move assignable.");
static_assert(std::is_move_constructible<FileReadStream>::value, "FileReadStream must be move constructible.");

FileReadStream::FileReadStream() : FileReadStream(MakeErrorStreambuf()) {}

FileReadStream::FileReadStream(FileReadStream&& rhs) noexcept
    : std::basic_istream<char>(std::move(rhs)),
      // The spec guarantees the base class move constructor only changes a few
      // member variables in `std::basic_istream<>`, and there is no spooky
      // action through virtual functions because there are no virtual
      // functions.  A good summary of the specification is "it calls the
      // default constructor and then calls std::basic_ios<>::move":
      //   https://en.cppreference.com/w/cpp/io/basic_ios/move
      // In fact, as that page indicates, the base classes are designed such
      // that derived classes can define their own move constructor and move
      // assignment.
      m_buf(std::move(rhs.m_buf))
{
    rhs.m_buf = MakeErrorStreambuf();
    rhs.set_rdbuf(rhs.m_buf.get());
    set_rdbuf(m_buf.get());
}

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

}  // namespace csa
