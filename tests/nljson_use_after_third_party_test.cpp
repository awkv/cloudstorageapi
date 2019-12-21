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

// Verify that we can #include the nljson.h header after a third-party has
// included json.hpp directly. We want to support both users that include
// nlohmann::json first (which is what this test file checks), and users that
// include our headers first (which nljson_use_third_party_test.cc checks).
// We simulate the nlohmann::json include by including nlohmann_json.hpp
#include "cloudstorageapi/internal/nlohmann_json.hpp"

#include "cloudstorageapi/internal/nljson.h"
#include <gtest/gtest.h>

/// @test Verify that we can compile against the nlohmann::json library.
TEST(NlJsonTest, Simple)
{
    csa::internal::nl::json json = {
        {"pi", 3.141},
        {"happy", true},
        {"nothing", nullptr},
        {"answer", {{"everything", 42}}},
        {"list", {1, 0, 2}},
        {"object", {{"currency", "USD"}, {"value", 42.99}}} };
    EXPECT_NEAR(3.141, json["pi"], 0.001);
    EXPECT_EQ("USD", json["object"]["currency"]);
    EXPECT_EQ(1, json["list"][0]);
}
