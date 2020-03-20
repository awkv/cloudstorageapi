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
    util/testing_types.cpp
    assert_ok_test.cpp
    curl_handle_test.cpp
    curl_wrappers_disable_sigpipe_handler_test.cpp
    curl_wrappers_enable_sigpipe_handler_test.cpp
    curl_wrappers_locking_already_present_test.cpp
    curl_wrappers_locking_disabled_test.cpp
    curl_wrappers_locking_enabled_test.cpp
    log_test.cpp
    main.cpp
    nljson_use_after_third_party_test.cpp
    nljson_use_third_party_test.cpp
    random_test.cpp
    rfc3339_time_test.cpp
    status_or_val_test.cpp
	status_test.cpp
    terminate_handler_test.cpp
    utils_test.cpp)

find_package(GTest CONFIG REQUIRED)

add_executable(csa_tests ${csa_tests})
target_link_libraries(csa_tests
                      PRIVATE GTest::gtest GTest::gtest_main GTest::gmock
                      cloudstorageapi)

target_include_directories(csa_tests
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                                  $<INSTALL_INTERFACE:include>)