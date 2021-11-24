// Copyright 2021 Andrew Karasyov
//
// Copyright 2019 Google LLC
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

#include "cloudstorageapi/internal/generic_request.h"
#include "cloudstorageapi/well_known_headers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

struct Placeholder : public GenericRequest<Placeholder>
{
};

TEST(GenericRequestTest, SetOptionRValueFirstBase)
{
    Placeholder req;
    req.SetOption(Fields("f1"));
    EXPECT_TRUE(req.HasOption<Fields>());
    EXPECT_EQ("f1", req.GetOption<Fields>().value());
}

TEST(GenericRequestTest, SetOptionLValueFirstBase)
{
    Placeholder req;
    Fields arg("f1");
    req.SetOption(arg);
    EXPECT_TRUE(req.HasOption<Fields>());
    EXPECT_EQ("f1", req.GetOption<Fields>().value());
}

TEST(GenericRequestTest, SetOptionRValueLastBase)
{
    Placeholder req;
    req.SetOption(CustomHeader("header1", "val1"));
    EXPECT_TRUE(req.HasOption<CustomHeader>());
    EXPECT_EQ("header1", req.GetOption<CustomHeader>().CustomHeaderName());
}

TEST(GenericRequestTest, SetOptionLValueLastBase)
{
    Placeholder req;
    CustomHeader arg("header1", "val1");
    req.SetOption(arg);
    EXPECT_TRUE(req.HasOption<CustomHeader>());
    EXPECT_EQ("header1", req.GetOption<CustomHeader>().CustomHeaderName());
}

}  // namespace internal
}  // namespace csa
