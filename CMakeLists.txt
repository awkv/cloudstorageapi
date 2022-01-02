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

cmake_minimum_required(VERSION 3.17)

set(PROJECT_NAME cloudstorageapi)
project(${PROJECT_NAME} VERSION 0.2.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate the version information from the CMake values.
configure_file(include/cloudstorageapi/version_info.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/include/cloudstorageapi/version_info.h)

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/binaries/${ARCH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/binaries/${ARCH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/binaries/${ARCH})

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Pick the right MSVC runtime libraries.
include(SelectMSVCRuntime)

include(IncludeNlohmannJson)
include(FindCurlWithTargets)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(quickstart)
