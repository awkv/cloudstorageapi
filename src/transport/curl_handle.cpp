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

#include "cloudstorageapi/internal/curl_handle.h"
#include "cloudstorageapi/internal/log.h"
#include "cloudstorageapi/internal/utils.h"
#include <sstream>
#include <string.h>
#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#endif  // _WIN32

namespace csa {
namespace internal {
namespace {

std::size_t const MaxDataDebugSize = 48;

extern "C" int CurlHandleDebugCallback(CURL*, curl_infotype type, char* data, std::size_t size, void* userptr)
{
    auto* debug_info = reinterpret_cast<CurlHandle::DebugInfo*>(userptr);
    switch (type)
    {
    case CURLINFO_TEXT:
        debug_info->buffer += "== curl(Info): " + std::string(data, size);
        break;
    case CURLINFO_HEADER_IN:
        debug_info->buffer += "<< curl(Recv Header): " + std::string(data, size);
        break;
    case CURLINFO_HEADER_OUT:
        debug_info->buffer += ">> curl(Send Header): " + std::string(data, size);
        break;
    case CURLINFO_DATA_IN:
        ++debug_info->m_recvCount;
        if (size == 0)
        {
            ++debug_info->m_recvZeroCount;
        }
        else
        {
            debug_info->buffer += ">> curl(Recv Data): size=";
            debug_info->buffer += std::to_string(size) + "\n";
            debug_info->buffer += BinaryDataAsDebugString(data, size, MaxDataDebugSize);
        }
        break;
    case CURLINFO_DATA_OUT:
        ++debug_info->m_sendCount;
        if (size == 0)
        {
            ++debug_info->m_sendZeroCount;
        }
        else
        {
            debug_info->buffer += ">> curl(Send Data): size=";
            debug_info->buffer += std::to_string(size) + "\n";
            debug_info->buffer += BinaryDataAsDebugString(data, size, MaxDataDebugSize);
        }
        break;
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
        // Do not print SSL binary data because generally that is not useful.
    case CURLINFO_END:
        break;
    }
    return 0;
}

extern "C" int CurlSetSocketOptions(void* userdata, curl_socket_t curlfd, curlsocktype purpose)
{
    auto errno_msg = [] { return csa::internal::strerror(errno); };
    auto* options = reinterpret_cast<CurlHandle::SocketOptions*>(userdata);
    switch (purpose)
    {
    case CURLSOCKTYPE_IPCXN:
        // An option value of zero (the default) means "do not change the buffer
        // size", this is reasonable because 0 is an invalid value anyway.
        if (options->m_recvBufferSize != 0)
        {
            auto size = static_cast<long>(options->m_recvBufferSize);
#if _WIN32
            int r = setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char const*>(&size), sizeof(size));
#else
            int r = setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
#endif  // WIN32
            if (r != 0)
            {
                CSA_LOG_ERROR("{}(): setting socket recv buffer size to {} error={} [{}]", __func__, size,
                              ::strerror(errno), errno);
                return CURL_SOCKOPT_ERROR;
            }
        }
        if (options->m_sendBufferSize != 0)
        {
            auto size = static_cast<long>(options->m_sendBufferSize);
#if _WIN32
            int r = setsockopt(curlfd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char const*>(&size), sizeof(size));
#else
            auto r = setsockopt(curlfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
#endif  // WIN32
            if (r != 0)
            {
                CSA_LOG_ERROR("{}(): setting socket send buffer size to {} error={} [{}]", __func__, size,
                              ::strerror(errno), errno);
                return CURL_SOCKOPT_ERROR;
            }
        }
        break;
    case CURLSOCKTYPE_ACCEPT:
    case CURLSOCKTYPE_LAST:
        break;
    }  // switch(purpose)

    return CURL_SOCKOPT_OK;
}

}  // namespace

void AssertOptionSuccessImpl(CURLcode e, CURLoption opt, char const* where,
                             std::function<std::string()> const& format_parameter)
{
    std::ostringstream os;
    os << where << "() - error [" << e << "] while setting curl option [" << opt << "] to [" << format_parameter()
       << "], error description=" << curl_easy_strerror(e);
    CSA_LOG_ERROR("{}", os.str());
    throw std::logic_error(std::move(os).str());
}

CurlHandle::CurlHandle() : m_handle(curl_easy_init(), &curl_easy_cleanup)
{
    if (m_handle.get() == nullptr)
    {
        throw std::runtime_error("Cannot initialize CURL handle");
    }
}

CurlHandle::~CurlHandle() { FlushDebug(__func__); }

void CurlHandle::SetSocketCallback(SocketOptions const& options)
{
    m_socketOptions = options;
    SetOption(CURLOPT_SOCKOPTDATA, &m_socketOptions);
    SetOption(CURLOPT_SOCKOPTFUNCTION, &CurlSetSocketOptions);
}

StatusOrVal<std::int32_t> CurlHandle::GetResponseCode()
{
    long code;  // NOLINT(google-runtime-int)
    auto e = curl_easy_getinfo(m_handle.get(), CURLINFO_RESPONSE_CODE, &code);
    if (e == CURLE_OK)
        return static_cast<std::int32_t>(code);
    return AsStatus(e, __func__);
}

std::string CurlHandle::GetPeer()
{
    char* ip = nullptr;
    auto e = curl_easy_getinfo(m_handle.get(), CURLINFO_PRIMARY_IP, &ip);
    if (e == CURLE_OK && ip != nullptr)
        return ip;
    return std::string{"[error-fetching-peer]"};
}

void CurlHandle::EnableLogging(bool enabled)
{
    if (enabled)
    {
        m_debugInfo = std::make_shared<DebugInfo>();
        SetOption(CURLOPT_DEBUGDATA, m_debugInfo.get());
        SetOption(CURLOPT_DEBUGFUNCTION, &CurlHandleDebugCallback);
        SetOption(CURLOPT_VERBOSE, 1L);
    }
    else
    {
        SetOption(CURLOPT_DEBUGDATA, nullptr);
        SetOption(CURLOPT_DEBUGFUNCTION, nullptr);
        SetOption(CURLOPT_VERBOSE, 0L);
    }
}

void CurlHandle::FlushDebug(char const* where)
{
    if (!m_debugInfo || m_debugInfo->buffer.empty())
        return;

    CSA_LOG_DEBUG("{} recv_count={} ({} with no data), send_count={} ({} with no data).", where,
                  m_debugInfo->m_recvCount, m_debugInfo->m_recvZeroCount, m_debugInfo->m_sendCount,
                  m_debugInfo->m_sendZeroCount);
    CSA_LOG_DEBUG("{} {}", where, m_debugInfo->buffer);

    *m_debugInfo = DebugInfo{};
}

Status CurlHandle::AsStatus(CURLcode e, char const* where)
{
    if (e == CURLE_OK)
    {
        return Status();
    }
    std::ostringstream os;
    os << where << "() - CURL error [" << e << "]=" << curl_easy_strerror(e);
    // Map the CURLE* errors using the documentation on:
    //   https://curl.haxx.se/libcurl/c/libcurl-errors.html
    // The error codes are listed in the same order as shown on that page, so
    // one can quickly find out how an error code is handled. All the error codes
    // are listed, but those that do not appear in old libcurl versions are
    // commented out and handled by the `default:` case.
    StatusCode code;
    switch (e)
    {
    case CURLE_UNSUPPORTED_PROTOCOL:
    case CURLE_FAILED_INIT:
    case CURLE_URL_MALFORMAT:
    case CURLE_NOT_BUILT_IN:
        code = StatusCode::Unknown;
        break;

    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
        code = StatusCode::Unavailable;
        break;

    // missing in some older libcurl versions:   CURLE_WEIRD_SERVER_REPLY
    case CURLE_REMOTE_ACCESS_DENIED:
        code = StatusCode::PermissionDenied;
        break;

    case CURLE_FTP_ACCEPT_FAILED:
    case CURLE_FTP_WEIRD_PASS_REPLY:
    case CURLE_FTP_WEIRD_227_FORMAT:
    case CURLE_FTP_CANT_GET_HOST:
    case CURLE_FTP_COULDNT_SET_TYPE:
        code = StatusCode::Unknown;
        break;

    case CURLE_PARTIAL_FILE:
        code = StatusCode::Unavailable;
        break;

    case CURLE_FTP_COULDNT_RETR_FILE:
    case CURLE_QUOTE_ERROR:
    case CURLE_WRITE_ERROR:
    case CURLE_UPLOAD_FAILED:
    case CURLE_READ_ERROR:
    case CURLE_OUT_OF_MEMORY:
        code = StatusCode::Unknown;
        break;

    case CURLE_OPERATION_TIMEDOUT:
        code = StatusCode::DeadlineExceeded;
        break;

    case CURLE_FTP_PORT_FAILED:
    case CURLE_FTP_COULDNT_USE_REST:
        code = StatusCode::Unknown;
        break;

    case CURLE_RANGE_ERROR:
        // This is defined as "the server does not *support or *accept* range
        // requests", so it means something stronger than "your range value is
        // not valid".
        code = StatusCode::Unimplemented;
        break;

    case CURLE_HTTP_POST_ERROR:
        code = StatusCode::Unknown;
        break;

    case CURLE_SSL_CONNECT_ERROR:
        code = StatusCode::Unavailable;
        break;

    case CURLE_BAD_DOWNLOAD_RESUME:
        code = StatusCode::InvalidArgument;
        break;

    case CURLE_FILE_COULDNT_READ_FILE:
    case CURLE_LDAP_CANNOT_BIND:
    case CURLE_LDAP_SEARCH_FAILED:
    case CURLE_FUNCTION_NOT_FOUND:
        code = StatusCode::Unknown;
        break;

    case CURLE_ABORTED_BY_CALLBACK:
        code = StatusCode::Aborted;
        break;

    case CURLE_BAD_FUNCTION_ARGUMENT:
    case CURLE_INTERFACE_FAILED:
    case CURLE_TOO_MANY_REDIRECTS:
    case CURLE_UNKNOWN_OPTION:
    case CURLE_TELNET_OPTION_SYNTAX:
        code = StatusCode::Unknown;
        break;

    case CURLE_GOT_NOTHING:
        code = StatusCode::Unavailable;
        break;

    case CURLE_SSL_ENGINE_NOTFOUND:
        code = StatusCode::Unknown;
        break;

    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
        code = StatusCode::Unavailable;
        break;

    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_BAD_CONTENT_ENCODING:
    case CURLE_LDAP_INVALID_URL:
    case CURLE_FILESIZE_EXCEEDED:
    case CURLE_USE_SSL_FAILED:
    case CURLE_SEND_FAIL_REWIND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_LOGIN_DENIED:
    case CURLE_TFTP_NOTFOUND:
    case CURLE_TFTP_PERM:
    case CURLE_REMOTE_DISK_FULL:
    case CURLE_TFTP_ILLEGAL:
    case CURLE_TFTP_UNKNOWNID:
    case CURLE_REMOTE_FILE_EXISTS:
    case CURLE_TFTP_NOSUCHUSER:
    case CURLE_CONV_FAILED:
    case CURLE_CONV_REQD:
    case CURLE_SSL_CACERT_BADFILE:
        code = StatusCode::Unknown;
        break;

    case CURLE_REMOTE_FILE_NOT_FOUND:
        code = StatusCode::NotFound;
        break;

        // NOLINTNEXTLINE(bugprone-branch-clone)
    case CURLE_SSH:
    case CURLE_SSL_SHUTDOWN_FAILED:
        code = StatusCode::Unknown;
        break;

    case CURLE_AGAIN:
        // This looks like a good candidate for kUnavailable, but it is only
        // returned by curl_easy_{recv,send}, and should not with the
        // configuration we use for libcurl, and the recovery action is to call
        // curl_easy_{recv,send} again, which is not how this return value is used
        // (we restart the whole transfer).
        code = StatusCode::Unknown;
        break;

    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_FTP_PRET_FAILED:
    case CURLE_RTSP_CSEQ_ERROR:
    case CURLE_RTSP_SESSION_ERROR:
    case CURLE_FTP_BAD_FILE_LIST:
    case CURLE_CHUNK_FAILED:
        code = StatusCode::Unknown;
        break;

    // cSpell:disable
    // missing in some older libcurl versions:   CURLE_HTTP_RETURNED_ERROR
    // missing in some older libcurl versions:   CURLE_NO_CONNECTION_AVAILABLE
    // missing in some older libcurl versions:   CURLE_SSL_PINNEDPUBKEYNOTMATCH
    // missing in some older libcurl versions:   CURLE_SSL_INVALIDCERTSTATUS
    // missing in some older libcurl versions:   CURLE_HTTP2_STREAM
    // missing in some older libcurl versions:   CURLE_RECURSIVE_API_CALL
    // missing in some older libcurl versions:   CURLE_AUTH_ERROR
    // missing in some older libcurl versions:   CURLE_HTTP3
    // missing in some older libcurl versions:   CURLE_QUIC_CONNECT_ERROR
    // cSpell:enable
    default:
        // As described above, there are about 100 error codes, some are
        // explicitly marked as obsolete, some are not available in all libcurl
        // versions. Use this `default:` case to treat all such errors as
        // `Unavailable` and they will be retried.
        code = StatusCode::Unavailable;
        break;
    }

    return Status(code, std::move(os).str());
}

}  // namespace internal
}  // namespace csa
