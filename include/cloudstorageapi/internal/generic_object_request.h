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

#include "cloudstorageapi/internal/generic_request.h"

namespace csa {
namespace internal {
// Common attributes for requests about objects.
template <typename Derived, typename... Parameters>
class GenericObjectRequest : public GenericRequest<Derived, Parameters...>
{
public:
    GenericObjectRequest() = default;

    explicit GenericObjectRequest(std::string objectId)
        : m_objectId(std::move(objectId)) {}

    std::string const& GetObjectId() const { return m_objectId; }
    Derived& SetObjectId(std::string objectId)
    {
        m_objectId = std::move(objectId);
        return *static_cast<Derived*>(this);
    }

private:
    std::string m_objectId;
};

}  // namespace internal
}  // namespace csa
