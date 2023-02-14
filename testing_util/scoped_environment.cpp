// Copyright 2020 Andrew Karasyov
//
// Copyright 2020 Google LLC
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

#include "testing_util/scoped_environment.h"
#include "cloudstorageapi/internal/utils.h"

namespace csa {
namespace testing {
namespace internal {

ScopedEnvironment::ScopedEnvironment(std::string variable, std::optional<std::string> const& value)
    : m_variable(std::move(variable)), m_prevValue(csa::internal::GetEnv(m_variable.c_str()))
{
    csa::internal::SetEnv(m_variable.c_str(), value);
}

ScopedEnvironment::~ScopedEnvironment() { csa::internal::SetEnv(m_variable.c_str(), std::move(m_prevValue)); }

}  // namespace internal
}  // namespace testing
}  // namespace csa
