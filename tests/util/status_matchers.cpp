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

#include "status_matchers.h"

namespace csa {
namespace testing {
namespace util {
namespace internal {

using ::testing::Matcher;
using ::testing::MatchResultListener;
using ::testing::StringMatchResultListener;

namespace {

template <typename T>
void Explain(char const* field, bool matched, Matcher<T> const& matcher, StringMatchResultListener* listener,
             std::ostream* os)
{
    std::string const explanation = listener->str();
    listener->Clear();
    *os << field;
    if (!explanation.empty())
    {
        *os << " " << explanation;
    }
    else
    {
        *os << " that ";
        if (matched)
        {
            matcher.DescribeTo(os);
        }
        else
        {
            matcher.DescribeNegationTo(os);
        }
    }
}

}  // namespace

bool StatusIsMatcher::MatchAndExplain(Status const& status, MatchResultListener* listener) const
{
    if (!listener->IsInterested())
    {
        return m_code_matcher.Matches(status.Code()) && m_message_matcher.Matches(status.Message());
    }
    auto* os = listener->stream();
    StringMatchResultListener innerListener;
    auto const codeMatched = m_code_matcher.MatchAndExplain(status.Code(), &innerListener);
    *os << "with a ";
    Explain("code", codeMatched, m_code_matcher, &innerListener, os);
    auto const messageMatched = m_message_matcher.MatchAndExplain(status.Message(), &innerListener);
    *os << ", " << (codeMatched != messageMatched ? "but" : "and") << " a ";
    Explain("message", messageMatched, m_message_matcher, &innerListener, os);
    return codeMatched && messageMatched;
}

void StatusIsMatcher::DescribeTo(std::ostream* os) const
{
    *os << "code ";
    m_code_matcher.DescribeTo(os);
    *os << " and message ";
    m_message_matcher.DescribeTo(os);
}

void StatusIsMatcher::DescribeNegationTo(std::ostream* os) const
{
    *os << "code ";
    m_code_matcher.DescribeNegationTo(os);
    *os << " or message ";
    m_message_matcher.DescribeNegationTo(os);
}

}  // namespace internal
}  // namespace util
}  // namespace testing
}  // namespace csa
