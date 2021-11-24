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

#pragma once

#include "cloudstorageapi/internal/file_read_streambuf.h"
#include <istream>
#include <map>
#include <memory>
#include <string>

namespace csa {

/// Represents the headers returned in a streaming upload or download operation.
using HeadersMap = std::multimap<std::string, std::string>;

/**
 * Defines a `std::basic_istream<char>` to read from a cloud storage file.
 */
class FileReadStream : public std::basic_istream<char>
{
public:
    /**
     * Creates a stream not associated with any buffer.
     *
     * Attempts to use this stream will result in failures.
     */
    FileReadStream();

    /**
     * Creates a stream associated with the given `streambuf`.
     */
    explicit FileReadStream(std::unique_ptr<internal::FileReadStreambuf> buf)
        : std::basic_istream<char>(nullptr), m_buf(std::move(buf))
    {
        // Initialize the basic_ios<> class
        init(m_buf.get());
    }

    FileReadStream(FileReadStream&& rhs) noexcept;

    FileReadStream& operator=(FileReadStream&& rhs) noexcept
    {
        FileReadStream tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    void swap(FileReadStream& rhs)
    {
        std::basic_istream<char>::swap(rhs);
        std::swap(m_buf, rhs.m_buf);
        rhs.set_rdbuf(rhs.m_buf.get());
        set_rdbuf(m_buf.get());
    }

    FileReadStream(FileReadStream const&) = delete;
    FileReadStream& operator=(FileReadStream const&) = delete;

    /// Closes the stream (if necessary).
    ~FileReadStream() override;

    bool IsOpen() const { return (bool)m_buf && m_buf->IsOpen(); }

    /**
     * Terminate the download, possibly before completing it.
     */
    void Close();

    //@{
    /**
     * Report any download errors.
     *
     * Note that errors may go undetected until the download completes.
     */
    Status const& GetStatus() const& { return m_buf->GetStatus(); }

    /// The headers returned by the service, for debugging only.
    HeadersMap const& GetHeaders() const { return m_buf->GetHeaders(); }
    //@}

private:
    std::unique_ptr<internal::FileReadStreambuf> m_buf;
};

}  // namespace csa
