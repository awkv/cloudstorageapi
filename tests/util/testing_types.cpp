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

#include "testing_types.h"

namespace csa {
namespace testing {
namespace util {

int Observable::default_constructor = 0;
int Observable::value_constructor = 0;
int Observable::copy_constructor = 0;
int Observable::move_constructor = 0;
int Observable::copy_assignment = 0;
int Observable::move_assignment = 0;
int Observable::destructor = 0;

}  // namespace util
}  // namespace testing
}  // namespace csa
