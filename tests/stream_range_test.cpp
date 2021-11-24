// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/internal/stream_range.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>
#include <vector>

namespace csa {
namespace internal {

using ::csa::testing::util::StatusIs;
using ::testing::ElementsAre;

TEST(StreamRange, DefaultConstructed)
{
    StreamRange<int> sr;
    auto it = sr.begin();
    auto end = sr.end();
    EXPECT_EQ(it, end);
    EXPECT_EQ(it, it);
    EXPECT_EQ(end, end);
}

TEST(StreamRange, MoveOnly)
{
    auto const reader = [] { return Status{}; };
    StreamRange<int> sr = MakeStreamRange<int>(reader);
    StreamRange<int> move_construct = std::move(sr);
    StreamRange<int> move_assign = MakeStreamRange<int>(reader);
    move_assign = std::move(move_construct);
}

TEST(StreamRange, EmptyRange)
{
    StreamRange<int> sr = MakeStreamRange<int>([] { return Status{}; });
    auto it = sr.begin();
    auto end = sr.end();
    EXPECT_EQ(it, end);
    EXPECT_EQ(it, it);
    EXPECT_EQ(end, end);
}

TEST(StreamRange, OneElement)
{
    auto counter = 0;
    auto reader = [&counter]() -> StreamReader<int>::result_type {
        if (counter++ < 1)
            return 42;
        return Status{};
    };

    StreamRange<int> sr = MakeStreamRange<int>(std::move(reader));
    auto it = sr.begin();
    EXPECT_NE(it, sr.end());
    EXPECT_TRUE(*it);
    EXPECT_EQ(**it, 42);
    ++it;
    EXPECT_EQ(it, sr.end());
}

TEST(StreamRange, OneError)
{
    auto reader = []() -> StreamReader<int>::result_type { return Status(StatusCode::Unknown, "oops"); };

    StreamRange<int> sr = MakeStreamRange<int>(std::move(reader));
    auto it = sr.begin();
    EXPECT_NE(it, sr.end());
    EXPECT_FALSE(*it);
    EXPECT_THAT(*it, StatusIs(StatusCode::Unknown, "oops"));
    ++it;
    EXPECT_EQ(it, sr.end());
}

TEST(StreamRange, FiveElements)
{
    auto counter = 0;
    auto reader = [&counter]() -> StreamReader<int>::result_type {
        if (counter++ < 5)
            return counter;
        return Status{};
    };

    StreamRange<int> sr = MakeStreamRange<int>(std::move(reader));
    std::vector<int> v;
    for (StatusOrVal<int>& x : sr)
    {
        EXPECT_TRUE(x);
        v.push_back(*x);
    }
    EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5));
}

TEST(StreamRange, PostFixIteration)
{
    auto counter = 0;
    auto reader = [&counter]() -> StreamReader<int>::result_type {
        if (counter++ < 5)
            return counter;
        return Status{};
    };

    StreamRange<int> sr = MakeStreamRange<int>(std::move(reader));
    std::vector<int> v;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = sr.begin(); it != sr.end(); it++)
    {
        EXPECT_TRUE(*it);
        v.push_back(**it);
    }
    EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5));
}

TEST(StreamRange, Distance)
{
    // Empty range
    StreamRange<int> sr = MakeStreamRange<int>([] { return Status{}; });
    EXPECT_EQ(0, std::distance(sr.begin(), sr.end()));

    // Range of one element
    auto counter = 0;
    StreamRange<int> one = MakeStreamRange<int>([&counter]() -> StreamReader<int>::result_type {
        if (counter++ < 1)
            return counter;
        return Status{};
    });
    EXPECT_EQ(1, std::distance(one.begin(), one.end()));

    // Range of five elements
    counter = 0;
    StreamRange<int> five = MakeStreamRange<int>([&counter]() -> StreamReader<int>::result_type {
        if (counter++ < 5)
            return counter;
        return Status{};
    });
    EXPECT_EQ(5, std::distance(five.begin(), five.end()));
}

TEST(StreamRange, StreamError)
{
    auto counter = 0;
    auto reader = [&counter]() -> StreamReader<int>::result_type {
        if (counter++ < 2)
            return counter;
        return Status(StatusCode::Unknown, "oops");
    };

    StreamRange<int> sr = MakeStreamRange<int>(std::move(reader));

    auto it = sr.begin();
    EXPECT_NE(it, sr.end());
    EXPECT_TRUE(*it);
    EXPECT_EQ(**it, 1);

    ++it;
    EXPECT_NE(it, sr.end());
    EXPECT_TRUE(*it);
    EXPECT_EQ(**it, 2);

    // Error, but we return the Status, not end of stream.
    ++it;
    EXPECT_NE(it, sr.end());
    EXPECT_FALSE(*it);
    EXPECT_THAT(*it, StatusIs(StatusCode::Unknown, "oops"));

    // Since the previous result was an error, we're at the end.
    ++it;
    EXPECT_EQ(it, sr.end());
}

}  // namespace internal
}  // namespace csa
