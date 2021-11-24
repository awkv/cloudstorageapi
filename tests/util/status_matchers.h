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

#pragma once

#include "cloudstorageapi/status_or_val.h"
#include <gmock/gmock.h>
#include <string>

namespace csa {
namespace testing {
namespace util {
namespace internal {

/*
 * Implementation of the StatusIs() matcher for a Status, a StatusOrVal<T>,
 * or a reference to either of them.
 */
class StatusIsMatcher
{
public:
    template <typename CodeMatcher, typename MessageMatcher>
    StatusIsMatcher(CodeMatcher&& codeMatcher, MessageMatcher&& messageMatcher)
        : m_code_matcher(::testing::MatcherCast<StatusCode>(std::forward<CodeMatcher>(codeMatcher))),
          m_message_matcher(::testing::MatcherCast<std::string const&>(std::forward<MessageMatcher>(messageMatcher)))
    {
    }

    bool MatchAndExplain(Status const& status, ::testing::MatchResultListener* listener) const;

    template <typename T>
    bool MatchAndExplain(StatusOrVal<T> const& value, ::testing::MatchResultListener* listener) const
    {
        // Because StatusOrVal<T> does not have a printer, gMock will render the
        // value using RawBytesPrinter as "N-byte object <...>", which is not
        // very useful. Accordingly, we print the enclosed Status so that a
        // failing expectation does not require further explanation.
        Status const& status = value.GetStatus();
        *listener << "whose status is " << ::testing::PrintToString(status);

        ::testing::StringMatchResultListener innerListener;
        auto const match = MatchAndExplain(status, &innerListener);
        auto const explanation = innerListener.str();
        if (!explanation.empty())
            *listener << ", " << explanation;
        return match;
    }

    void DescribeTo(std::ostream* os) const;
    void DescribeNegationTo(std::ostream* os) const;

private:
    ::testing::Matcher<StatusCode> const m_code_matcher;
    ::testing::Matcher<std::string const&> const m_message_matcher;
};

}  // namespace internal

/**
 * Returns a gMock matcher that matches a `Status` or `StatusOrVal<T>` whose
 * code matches `codeMatcher` and whose message matches `messageMatcher`.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_THAT(status, StatusIs(StatusCode::InvalidArgument,
 *                                testing::HasSubstr("no rows"));
 * @endcode
 */
template <typename CodeMatcher, typename MessageMatcher>
::testing::PolymorphicMatcher<internal::StatusIsMatcher> StatusIs(CodeMatcher&& codeMatcher,
                                                                  MessageMatcher&& messageMatcher)
{
    return ::testing::MakePolymorphicMatcher(internal::StatusIsMatcher(std::forward<CodeMatcher>(codeMatcher),
                                                                       std::forward<MessageMatcher>(messageMatcher)));
}

/**
 * Returns a gMock matcher that matches a `Status` or `StatusOrVal<T>` whose
 * code matches `codeMatcher` and whose message matches anything.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_THAT(status, StatusIs(StatusCode::InvalidArgument));
 * @endcode
 */
template <typename CodeMatcher>
::testing::PolymorphicMatcher<internal::StatusIsMatcher> StatusIs(CodeMatcher&& codeMatcher)
{
    return StatusIs(std::forward<CodeMatcher>(codeMatcher), ::testing::_);
}

/**
 * Returns a gMock matcher that matches a `Status` or `StatusOrVal<T>` whose
 * code is OK and whose message matches anything.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_THAT(status, IsOk());
 * @endcode
 */
inline ::testing::PolymorphicMatcher<internal::StatusIsMatcher> IsOk()
{
    // We could use ::testing::IsEmpty() here, but historically have not.
    return StatusIs(StatusCode::Ok, ::testing::_);
}

/**
 * Expectations that a `Status` or `StatusOrVal<T>` has an OK code.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_STATUS_OK(status);
 * @endcode
 */
#define EXPECT_STATUS_OK(expression) EXPECT_THAT(expression, ::csa::testing::util::IsOk())
#define ASSERT_STATUS_OK(expression) ASSERT_THAT(expression, ::csa::testing::util::IsOk())

}  // namespace util
}  // namespace testing
}  // namespace csa
