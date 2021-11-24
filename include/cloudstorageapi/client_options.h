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

#pragma once

#include "cloudstorageapi/auth/credentials.h"
#include "cloudstorageapi/options.h"
#include "cloudstorageapi/providers.h"
#include "cloudstorageapi/retry_policy.h"
#include "cloudstorageapi/status_or_val.h"
#include <chrono>
#include <memory>

namespace csa {

namespace internal {
Options CreateDefaultOptionsWithCredentials(Options opts);
Options CreateDefaultOptions(std::shared_ptr<auth::Credentials> credentials, Options opts);
}  // namespace internal

struct ProviderOption
{
    using Type = EProvider;
};

/**
 * Set the HTTP version used by the client.
 *
 * If this option is not provided, or is set to `default` then the library uses
 * [libcurl's default], typically HTTP/2 with SSL. Possible settings include:
 * - "1.0": use HTTP/1.0, this is not recommended as would require a new
 *   connection for each request.
 * - "1.1": use HTTP/1.1, this may be useful when the overhead of HTTP/2 is
 *   unacceptable. Note that this may require additional connections.
 * - "2TLS": use HTTP/2 with TLS
 * - "2.0": use HTTP/2 with our without TLS.
 *
 * [libcurl's default]: https://curl.se/libcurl/c/CURLOPT_HTTP_VERSION.html
 */
struct HttpVersionOption
{
    using Type = std::string;
};

/// Configure auth::Credentials for the client library.
struct Oauth2CredentialsOption
{
    using Type = std::shared_ptr<auth::Credentials>;
};

/**
 * Set the maximum connection pool size.
 *
 * The C++ client library uses this value to limit the growth of the
 * connection pool. Once an operation (a RPC or a download) completes the
 * connection used for that operation is returned to the pool. If the pool is
 * full the connection is immediately released. If the pool has room the
 * connection is cached for the next RPC or download.
 *
 * @note The behavior of this pool may change in the future, depending on the
 * low-level implementation details of the library.
 *
 * @note The library does not create connections proactively, setting a high
 * value may result in very few connections if your application does not need
 * them.
 *
 * @warning The library may create more connections than this option configures,
 * for example if your application requests many simultaneous downloads.
 */
struct ConnectionPoolSizeOption
{
    using Type = std::size_t;
};

/**
 * Control the formatted I/O download buffer.
 *
 * When using formatted I/O operations (typically `operator>>(std::istream&...)`
 * this option controls the size of the in-memory buffer kept to satisfy any I/O
 * requests.
 *
 * Applications seeking optional performance for downloads should avoid
 * formatted I/O, and prefer using `std::istream::read()`. This option has no
 * effect in that case.
 */
struct DownloadBufferSizeOption
{
    using Type = std::size_t;
};

/**
 * Control the formatted I/O upload buffer.
 *
 * When using formatted I/O operations (typically `operator<<(std::istream&...)`
 * this option controls the size of the in-memory buffer kept before a chunk is
 * uploaded. Note that some providers only accept chunks in multiples of X KiB, so this
 * option is always rounded up to the next such multiple.
 *
 * Applications seeking optional performance for downloads should avoid
 * formatted I/O, and prefer using `std::istream::write()`. This option has no
 * effect in that case.
 */
struct UploadBufferSizeOption
{
    using Type = std::size_t;
};

/**
 * Defines the threshold to switch from simple to resumable uploads for files.
 *
 * When uploading small files the faster approach is to use a simple upload. For
 * very large files this is not feasible, as the whole file may not fit in
 * memory (we are ignoring memory mapped files in this discussion). The library
 * automatically switches to resumable upload for files larger than this
 * threshold.
 */
struct MaximumSimpleUploadSizeOption
{
    using Type = std::size_t;
};

/**
 * Disables automatic OpenSSL locking.
 *
 * With older versions of OpenSSL any locking must be provided by locking
 * callbacks in the application or intermediate libraries. The C++ client
 * library automatically provides the locking callbacks. If your application
 * already provides such callbacks, and you prefer to use them, set this option
 * to `false`.
 *
 * @note This option is only useful for applications linking against
 * OpenSSL 1.0.2.
 */
struct EnableCurlSslLockingOption
{
    using Type = bool;
};

/**
 * Disables automatic OpenSSL sigpipe handler.
 *
 * With some versions of OpenSSL it might be necessary to setup a SIGPIPE
 * handler. If your application already provides such a handler, set this option
 * to `false` to disable the handler in the GCS C++ client library.
 */
struct EnableCurlSigpipeHandlerOption
{
    using Type = bool;
};

/**
 * Control the maximum socket receive buffer.
 *
 * The default is to let the operating system pick a value. Applications that
 * perform multiple downloads in parallel may need to use smaller receive
 * buffers to avoid exhausting the OS resources dedicated to TCP buffers.
 */
struct MaximumCurlSocketRecvSizeOption
{
    using Type = std::size_t;
};

/**
 * Control the maximum socket send buffer.
 *
 * The default is to let the operating system pick a value, this is almost
 * always a good choice.
 */
struct MaximumCurlSocketSendSizeOption
{
    using Type = std::size_t;
};

/**
 * Sets the "stall" timeout.
 *
 * If the download "stalls", i.e., no bytes are received for a significant
 * period, it may be better to restart the download as this may indicate a
 * network glitch.
 */
struct DownloadStallTimeoutOption
{
    using Type = std::chrono::seconds;
};

/**
 * User-agent products to include with each request.
 *
 * Libraries or services that use Cloud C++ clients may want to set their own
 * user-agent product information. This can help them develop telemetry
 * information about number of users running particular versions of their
 * system or library.
 *
 * @see https://tools.ietf.org/html/rfc7231#section-5.5.3
 */
struct UserAgentProductsOption
{
    using Type = std::vector<std::string>;
};

/**
 * Return whether tracing is enabled for the given @p component.
 *
 * The C++ clients can log interesting events to help library and application
 * developers troubleshoot problems. To see log messages (maybe lots) you can
 * enable tracing for the component that interests you. Valid components are
 * currently:
 *
 * - http
 * - raw-client
 */
struct TracingComponentsOption
{
    using Type = std::set<std::string>;
};

/**
 * Configures a custom CA (Certificates Authority) certificates file.
 *
 * Most applications should use the system's root certificates and should avoid
 * setting this option unnecessarily. A common exception to this recommendation
 * are containerized applications. These often deploy without system's root
 * certificates and need to explicitly configure a root of trust.
 *
 * The value of this option should be the name of a file in [PEM format].
 * Consult your security team and/or system administrator for the contents of
 * this file. Be aware of the security implications of adding new CA
 * certificates to this file. Only use trustworthy sources for the CA
 * certificates.
 *
 * For REST-based libraries this configures the [CAINFO option] in libcurl.
 * These are used for all credentials that require authentication, including the
 * default credentials.
 *
 * @note CA certificates can be revoked or expire, plan for updates in your
 *     deployment.
 *
 * @see https://en.wikipedia.org/wiki/Certificate_authority for a general
 * introduction to SSL certificate authorities.
 *
 * [CAINFO Option]: https://curl.se/libcurl/c/CURLOPT_CAINFO.html
 * [PEM format]: https://en.wikipedia.org/wiki/Privacy-Enhanced_Mail
 */
struct CARootsFilePathOption
{
    using Type = std::string;
};

namespace internal {
/// This is only intended for testing. It is not for public use.
struct CAPathOption
{
    using Type = std::string;
};
}  // namespace internal

/// Set the retry policy for a client.
struct RetryPolicyOption
{
    using Type = std::shared_ptr<csa::RetryPolicy>;
};

/// Set the backoff policy for a client.
struct BackoffPolicyOption
{
    using Type = std::shared_ptr<csa::BackoffPolicy>;
};

/// The complete list of options accepted by `storage::Client`.
using CloudStorageClientOptionList =
    ::csa::OptionList<ProviderOption, Oauth2CredentialsOption, ConnectionPoolSizeOption, DownloadBufferSizeOption,
                      UploadBufferSizeOption, EnableCurlSslLockingOption, EnableCurlSigpipeHandlerOption,
                      MaximumCurlSocketRecvSizeOption, MaximumCurlSocketSendSizeOption, DownloadStallTimeoutOption,
                      HttpVersionOption, UserAgentProductsOption, TracingComponentsOption, CARootsFilePathOption,
                      RetryPolicyOption, BackoffPolicyOption>;

}  // namespace csa
