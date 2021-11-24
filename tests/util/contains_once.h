// Copyright 2021 Andrew Karasyov
//
// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <gmock/gmock.h>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace csa {
namespace testing {
namespace internal {

template <typename Container>
class ContainsOnceMatcherImpl : public ::testing::MatcherInterface<Container>
{
public:
    template <typename InnerMatcher>
    explicit ContainsOnceMatcherImpl(InnerMatcher inner_matcher)
        : m_innerMatcher(::testing::SafeMatcherCast<Element const&>(inner_matcher))
    {
    }

    void DescribeTo(std::ostream* os) const override
    {
        *os << "contains exactly one element that ";
        this->m_innerMatcher.DescribeTo(os);
    }

    void DescribeNegationTo(std::ostream* os) const override
    {
        *os << "doesn't contain exactly one element that ";
        this->m_innerMatcher.DescribeTo(os);
    }

    bool MatchAndExplain(Container const& container, ::testing::MatchResultListener* listener) const override
    {
        std::size_t matches = 0;
        for (auto const& element : container)
        {
            if (m_innerMatcher.MatchAndExplain(element, listener))
            {
                ++matches;
            }
        }
        if (matches != 1)
        {
            *listener << "matched " << matches << " times";
            return false;
        }
        return true;
    }

private:
    using Element = typename std::decay<Container>::type::value_type;
    ::testing::Matcher<Element const&> const m_innerMatcher;
};

template <typename InnerMatcher>
class ContainsOnceMatcher
{
public:
    explicit ContainsOnceMatcher(InnerMatcher inner_matcher) : m_innerMatcher(std::move(inner_matcher)) {}

    template <typename Container>
    // NOLINTNEXTLINE(google-explicit-constructor)
    operator ::testing::Matcher<Container>() const
    {
        return ::testing::Matcher<Container>(new ContainsOnceMatcherImpl<Container const&>(m_innerMatcher));
    }

private:
    InnerMatcher const m_innerMatcher;
};

}  // namespace internal

namespace util {

/**
 * Matches an STL-style container or a native array that contains exactly
 * one element matching the given value or matcher.
 *
 * @par Example
 *
 * @code
 * using ::testing::HasSubstr;
 * using testing_util::ContainsOnce;
 * std::vector<std::string> v = { ... };
 * EXPECT_THAT(v, ContainsOnce("foo"));
 * EXPECT_THAT(v, ContainsOnce(HasSubstr("bar")));
 * @endcode
 */
template <typename InnerMatcher>
testing::internal::ContainsOnceMatcher<typename std::decay<InnerMatcher>::type> ContainsOnce(
    InnerMatcher&& inner_matcher)
{
    return testing::internal::ContainsOnceMatcher<typename std::decay<InnerMatcher>::type>(
        std::forward<InnerMatcher>(inner_matcher));
}

}  // namespace util
}  // namespace testing
}  // namespace csa
