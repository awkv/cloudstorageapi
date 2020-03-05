# Copyright 2019 Andrew Karasyov
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(csa_sources
    auth/credential_factory.cpp
    auth/default_credentials_file.cpp
    auth/google_oauth2_credentials.cpp
    auth/refreshing_credentials_wrapper.cpp
    transport/clients/curl_googledrive_client.cpp
    transport/clients/google_metadata_parser.cpp
    transport/curl_client_base.cpp
    transport/curl_download_request.cpp
    transport/curl_handle.cpp
    transport/curl_handle_factory.cpp
    transport/curl_request.cpp
    transport/curl_request_builder.cpp
    transport/curl_resumable_upload_session.cpp
    transport/curl_wrappers.cpp
    transport/empty_response.cpp
    transport/file_requests.cpp
    transport/folder_requests.cpp
    transport/http_empty_response.cpp
    transport/http_response.cpp
    transport/resumable_upload_session.cpp
    client_options.cpp
    cloud_storage_client.cpp
    common_metadata.cpp
    file_metadata.cpp
    folder_metadata.cpp
    json_utils.cpp
    random.cpp
    rfc3339_time.cpp
    status.cpp
    terminate_handler.cpp
    utils.cpp
    well_known_headers.cpp
    )
	
add_library(cloudstorageapi STATIC
            ${csa_sources})
			
target_link_libraries(cloudstorageapi
                      PUBLIC CURL::libcurl
                             OpenSSL::SSL
                             OpenSSL::Crypto
                             ZLIB::ZLIB)

target_include_directories(cloudstorageapi
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    cloudstorageapi
    PROPERTIES
        VERSION
        ${CSA_VERSION_MAJOR}.${CSA_VERSION_MINOR}.${CSA_VERSION_PATCH}
        SOVERSION
        ${CSA_VERSION_MAJOR})
