// Copyright 2021 Andrew Karasyov
//
// Copyright 2021 Google LLC
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

#include "cloudstorageapi/options.h"
#include "testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <set>
#include <string>

namespace csa {
namespace internal {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::UnorderedElementsAre;

struct IntOption
{
    using Type = int;
};

struct BoolOption
{
    using Type = bool;
};

struct StringOption
{
    using Type = std::string;
};

using TestOptionList = OptionList<IntOption, BoolOption, StringOption>;

// This is how customers should set a simple options.
TEST(OptionsUseCase, CustomerSettingSimpleOptions)
{
    auto const opts = Options{}.Set<IntOption>(123).Set<BoolOption>(true);

    EXPECT_TRUE(opts.Has<IntOption>());
    EXPECT_TRUE(opts.Has<BoolOption>());
}

// This is how customers should append to an option.
TEST(OptionsUseCase, CustomerSettingComplexOption)
{
    struct ComplexOption
    {
        using Type = std::set<std::string>;
    };

    Options opts;

    EXPECT_FALSE(opts.Has<ComplexOption>());
    opts.Lookup<ComplexOption>().insert("foo");
    EXPECT_TRUE(opts.Has<ComplexOption>());
    opts.Lookup<ComplexOption>().insert("bar");

    EXPECT_THAT(opts.Lookup<ComplexOption>(), UnorderedElementsAre("foo", "bar"));
}

TEST(Options, Has)
{
    Options opts;
    EXPECT_FALSE(opts.Has<IntOption>());
    opts.Set<IntOption>(42);
    EXPECT_TRUE(opts.Has<IntOption>());
}

TEST(Options, Set)
{
    Options opts;
    opts.Set<IntOption>({});
    EXPECT_TRUE(opts.Has<IntOption>());
    EXPECT_EQ(0, opts.Get<IntOption>());
    opts.Set<IntOption>(123);
    EXPECT_EQ(123, opts.Get<IntOption>());

    opts = Options{};
    opts.Set<BoolOption>({});
    EXPECT_TRUE(opts.Has<BoolOption>());
    EXPECT_EQ(false, opts.Get<BoolOption>());
    opts.Set<BoolOption>(true);
    EXPECT_EQ(true, opts.Get<BoolOption>());

    opts = Options{};
    opts.Set<StringOption>({});
    EXPECT_TRUE(opts.Has<StringOption>());
    EXPECT_EQ("", opts.Get<StringOption>());
    opts.Set<StringOption>("foo");
    EXPECT_EQ("foo", opts.Get<StringOption>());
}

TEST(Options, Get)
{
    Options opts;

    int const& i = opts.Get<IntOption>();
    EXPECT_EQ(0, i);
    opts.Set<IntOption>(42);
    EXPECT_EQ(42, opts.Get<IntOption>());

    std::string const& s = opts.Get<StringOption>();
    EXPECT_TRUE(s.empty());
    opts.Set<StringOption>("test");
    EXPECT_EQ("test", opts.Get<StringOption>());
}

TEST(Options, Lookup)
{
    Options opts;

    // Lookup with value-initialized default.
    EXPECT_FALSE(opts.Has<IntOption>());
    int& x = opts.Lookup<IntOption>();
    EXPECT_TRUE(opts.Has<IntOption>());
    EXPECT_EQ(0, x);  // Value initialized int.
    x = 42;           // Sets x within the Options
    EXPECT_EQ(42, opts.Lookup<IntOption>());

    // Lookup with user-supplied default value.
    opts.Unset<IntOption>();
    EXPECT_FALSE(opts.Has<IntOption>());
    EXPECT_EQ(42, opts.Lookup<IntOption>(42));
    EXPECT_TRUE(opts.Has<IntOption>());
}

TEST(Options, Copy)
{
    auto a = Options{}.Set<IntOption>(42).Set<BoolOption>(true).Set<StringOption>("foo");

    auto copy = a;  // NOLINT(performance-unnecessary-copy-initialization)
    EXPECT_TRUE(copy.Has<IntOption>());
    EXPECT_TRUE(copy.Has<BoolOption>());
    EXPECT_TRUE(copy.Has<StringOption>());

    EXPECT_EQ(42, copy.Get<IntOption>());
    EXPECT_EQ(true, copy.Get<BoolOption>());
    EXPECT_EQ("foo", copy.Get<StringOption>());
}

TEST(Options, Move)
{
    auto a = Options{}.Set<IntOption>(42).Set<BoolOption>(true).Set<StringOption>("foo");

    auto moved = std::move(a);
    EXPECT_TRUE(moved.Has<IntOption>());
    EXPECT_TRUE(moved.Has<BoolOption>());
    EXPECT_TRUE(moved.Has<StringOption>());

    EXPECT_EQ(42, moved.Get<IntOption>());
    EXPECT_EQ(true, moved.Get<BoolOption>());
    EXPECT_EQ("foo", moved.Get<StringOption>());
}

TEST(CheckUnexpectedOptions, Empty)
{
    testing::internal::ScopedLog log;
    Options opts;
    internal::CheckExpectedOptions<BoolOption>(opts, "caller");
    EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OneExpected)
{
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<BoolOption>({});
    internal::CheckExpectedOptions<BoolOption>(opts, "caller");
    EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, TwoExpected)
{
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<BoolOption>({});
    opts.Set<IntOption>({});
    internal::CheckExpectedOptions<BoolOption, IntOption>(opts, "caller");
    EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, FullishLogLine)
{
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<IntOption>({});
    internal::CheckExpectedOptions<BoolOption>(opts, "caller");
    // This tests exists just to show us what a full log line may look like.
    // The regex hides the nastiness of the actual mangled name.
    EXPECT_THAT(log.ExtractLines(),
                Contains(ContainsRegex(R"(caller: Unexpected option \(mangled name\): .+IntOption)")));
}

TEST(CheckUnexpectedOptions, OneUnexpected)
{
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<IntOption>({});
    internal::CheckExpectedOptions<BoolOption>(opts, "caller");
    EXPECT_THAT(log.ExtractLines(), Contains(ContainsRegex("caller: Unexpected option.+IntOption")));
}

TEST(CheckUnexpectedOptions, TwoUnexpected)
{
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<IntOption>({});
    opts.Set<StringOption>({});
    internal::CheckExpectedOptions<BoolOption>(opts, "caller");
    EXPECT_THAT(log.ExtractLines(), AllOf(Contains(ContainsRegex("caller: Unexpected option.+IntOption")),
                                          Contains(ContainsRegex("caller: Unexpected option.+StringOption"))));
}

TEST(CheckUnexpectedOptions, BasicOptionsList)
{
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<IntOption>({});
    opts.Set<StringOption>({});
    internal::CheckExpectedOptions<TestOptionList>(opts, "caller");
    EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OptionsListPlusOne)
{
    struct FooOption
    {
        using Type = int;
    };
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<IntOption>({});
    opts.Set<StringOption>({});
    opts.Set<FooOption>({});
    internal::CheckExpectedOptions<FooOption, TestOptionList>(opts, "caller");
    EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OptionsListOneUnexpected)
{
    struct FooOption
    {
        using Type = int;
    };
    testing::internal::ScopedLog log;
    Options opts;
    opts.Set<IntOption>({});
    opts.Set<StringOption>({});
    opts.Set<FooOption>({});
    internal::CheckExpectedOptions<TestOptionList>(opts, "caller");
    EXPECT_THAT(log.ExtractLines(), Contains(ContainsRegex("caller: Unexpected option.+FooOption")));
}

TEST(MergeOptions, Basics)
{
    auto a = Options{}.Set<StringOption>("from a").Set<IntOption>(42);
    auto b = Options{}.Set<StringOption>("from b").Set<BoolOption>(true);
    a = internal::MergeOptions(std::move(a), std::move(b));
    EXPECT_EQ(a.Get<StringOption>(), "from a");  // From a
    EXPECT_EQ(a.Get<BoolOption>(), true);        // From b
    EXPECT_EQ(a.Get<IntOption>(), 42);           // From a
}

}  // namespace internal
}  // namespace csa
