// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/internal/object_read_source.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

class MockObjectReadSource : public internal::ObjectReadSource
{
public:
    MOCK_METHOD(bool, IsOpen, (), (const, override));
    MOCK_METHOD(StatusOrVal<HttpResponse>, Close, (), (override));
    MOCK_METHOD(StatusOrVal<ReadSourceResult>, Read, (char* buf, std::size_t n), (override));
};

}  // namespace internal
}  // namespace csa
