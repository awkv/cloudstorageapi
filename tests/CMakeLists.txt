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

set(csa_tests
    util/assert_ok.cpp
    util/scoped_environment.cpp
    util/scoped_log.cpp
    util/status_matchers.cpp
    util/testing_types.cpp
    algorithm_test.cpp
    anonymous_credentials_test.cpp
    assert_ok_test.cpp
    auto_finalize_test.cpp
    backoff_policy_internal_tests.cpp
    complex_option_test.cpp
    const_buffer_equality_test.cpp
    const_buffer_test.cpp
    contains_once_test.cpp
    curl_googledrive_client_test.cpp
    curl_handle_factory_test.cpp
    curl_handle_test.cpp
    curl_resumable_upload_session_test.cpp
    curl_wrappers_disable_sigpipe_handler_test.cpp
    curl_wrappers_enable_sigpipe_handler_test.cpp
    curl_wrappers_locking_already_present_test.cpp
    curl_wrappers_locking_disabled_test.cpp
    curl_wrappers_locking_enabled_test.cpp
    curl_wrappers_test.cpp
    default_credentials_file_test.cpp
    file_metadata_test.cpp
    file_read_streambuf_test.cpp
    file_requests_tests.cpp
    file_stream_test.cpp
    file_write_streambuf_test.cpp
    folder_metadata_test.cpp
    folder_requests_tests.cpp
    generic_request_test.cpp
    google_metadata_parser_test.cpp
    google_oauth2_credentials_test.cpp
    google_response_parser_test.cpp
    google_utils_test.cpp
    http_response_test.cpp
    json_utils_test.cpp
    list_folder_reader_test.cpp
    log_test.cpp
    logging_client_test.cpp
    logging_resumable_upload_session_test.cpp
    main.cpp
    mock_fake_clock.cpp
    mock_http_request.cpp
    oauth2_credentials_test.cpp
    options_test.cpp
    pagination_range_test.cpp
    random_test.cpp
    retry_object_read_source_tests.cpp
    retry_policy_internal_tests.cpp
    retry_policy_tests.cpp
    retry_resumable_upload_session_tests.cpp
    rfc3339_time_test.cpp
    scoped_environment_test.cpp
    status_matchers_test.cpp
    status_or_val_test.cpp
	status_test.cpp
    storage_include_test.cpp
    stream_range_test.cpp
    terminate_handler_test.cpp
    type_list_test.cpp
    utils_test.cpp
    well_known_headers_test.cpp
    well_known_parameters_test.cpp)

find_package(GTest CONFIG REQUIRED)

add_executable(csa_tests ${csa_tests})
target_link_libraries(csa_tests
                      PRIVATE GTest::gtest GTest::gtest_main GTest::gmock
                      cloudstorageapi
                      nlohmann_json::nlohmann_json)

target_include_directories(csa_tests
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                                  $<INSTALL_INTERFACE:include>)