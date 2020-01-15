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

cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME cloudstorageapi)
project(${PROJECT_NAME} LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CSA_VERSION_MAJOR 0)
set(CSA_VERSION_MINOR 0)
set(CSA_VERSION_PATCH 1)

string(CONCAT CSA_VERSION
              "${CSA_VERSION_MAJOR}"
              "."
              "${CSA_VERSION_MINOR}"
              "."
              "${CSA_VERSION_PATCH}")

# Generate the version information from the CMake values.
configure_file(include/cloudstorageapi/version_info.h.in
               include/cloudstorageapi/version_info.h)

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/binaries/${ARCH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/binaries/${ARCH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/binaries/${ARCH})

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Pick the right MSVC runtime libraries.
include(SelectMSVCRuntime)

include(FindCurlWithTargets)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)
