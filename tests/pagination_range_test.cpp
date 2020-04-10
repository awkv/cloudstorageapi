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

#include "cloudstorageapi/internal/pagination_range.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {
namespace test {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Invoke;
struct ItemTypeFake
{
    ItemTypeFake(const char* name)
        : m_name(name)
    {}

    std::string GetName() const { return m_name; }
    bool operator==(ItemTypeFake const& other) const { return m_name == other.m_name; }

private:
    std::string m_name;
};

struct RequestFake
{
    void SetPageToken(std::string const& token) { m_nextPageToken = token; }
    std::string GetPageToken() const { return m_nextPageToken; }

    std::string m_nextPageToken;
};

struct ResponseFake
{
    void AddItem(ItemTypeFake const& item) { m_items.push_back(item); }
    void SetNextPageToken(std::string const& token) { m_nextPageToken = token; }

    std::string m_nextPageToken;
    std::vector<ItemTypeFake> m_items;
};

using TestedRange = PaginationRange<ItemTypeFake, RequestFake, ResponseFake>;

class MockRpc
{
public:
    MOCK_METHOD1(Loader, StatusOrVal<ResponseFake>(RequestFake const&));
    MOCK_METHOD1(GetItems, std::vector<ItemTypeFake>(ResponseFake));
};

TEST(RangeFromPagination, Empty)
{
    MockRpc mock;
    EXPECT_CALL(mock, Loader(_)).WillOnce(Invoke([](RequestFake const& request) {
        EXPECT_TRUE(request.GetPageToken().empty());
        ResponseFake response;
        return response;
        }));

    TestedRange range(
        RequestFake{}, [&](RequestFake const& r) { return mock.Loader(r); });
    EXPECT_TRUE(range.begin() == range.end());
}

TEST(RangeFromPagination, SinglePage)
{
    MockRpc mock;
    EXPECT_CALL(mock, Loader(_)).WillOnce(Invoke([](RequestFake const& request) {
        EXPECT_TRUE(request.GetPageToken().empty());
        ResponseFake response;
        response.AddItem("p1");
        response.AddItem("p2");
        return response;
        }));

    TestedRange range(
        RequestFake{}, [&](RequestFake const& r) { return mock.Loader(r); });
    std::vector<std::string> names;
    for (auto& p : range)
    {
        if (!p) break;
        names.push_back(p->GetName());
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2"));
}

TEST(RangeFromPagination, TwoPages)
{
    MockRpc mock;
    EXPECT_CALL(mock, Loader(_))
        .WillOnce(Invoke([](RequestFake const& request) {
            EXPECT_TRUE(request.GetPageToken().empty());
            ResponseFake response;
            response.SetNextPageToken("t1");
            response.AddItem("p1");
            response.AddItem("p2");
            return response;
            }))
        .WillOnce(Invoke([](RequestFake const& request) {
            EXPECT_EQ("t1", request.GetPageToken());
            ResponseFake response;
            response.AddItem("p3");
            response.AddItem("p4");
            return response;
            }));

    TestedRange range(
        RequestFake{}, [&](RequestFake const& r) { return mock.Loader(r); });
    std::vector<std::string> names;
    for (auto& p : range) {
        if (!p) break;
        names.push_back(p->GetName());
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TEST(RangeFromPagination, TwoPagesWithError)
{
    MockRpc mock;
    EXPECT_CALL(mock, Loader(_))
        .WillOnce(Invoke([](RequestFake const& request) {
        EXPECT_TRUE(request.GetPageToken().empty());
        ResponseFake response;
        response.SetNextPageToken("t1");
        response.AddItem("p1");
        response.AddItem("p2");
        return response;
            }))
        .WillOnce(Invoke([](RequestFake const& request) {
                EXPECT_EQ("t1", request.GetPageToken());
                ResponseFake response;
                response.SetNextPageToken("t2");
                response.AddItem("p3");
                response.AddItem("p4");
                return response;
            }))
        .WillOnce(Invoke([](RequestFake const& request) {
        EXPECT_EQ("t2", request.GetPageToken());
        return Status(StatusCode::Aborted, "bad-luck");
            }));

    TestedRange range(
        RequestFake{}, [&](RequestFake const& r) { return mock.Loader(r); });
    std::vector<std::string> names;
    for (auto& p : range)
    {
        if (!p) {
            EXPECT_EQ(StatusCode::Aborted, p.GetStatus().Code());
            EXPECT_THAT(p.GetStatus().Message(), HasSubstr("bad-luck"));
            break;
        }
        names.push_back(p->GetName());
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TEST(RangeFromPagination, IteratorCoverage)
{
    MockRpc mock;
    EXPECT_CALL(mock, Loader(_))
        .WillOnce(Invoke([](RequestFake const& request) {
        EXPECT_TRUE(request.GetPageToken().empty());
        ResponseFake response;
        response.SetNextPageToken("t1");
        response.AddItem("p1");
        return response;
            }))
        .WillOnce(Invoke([](RequestFake const& request) {
                EXPECT_EQ("t1", request.GetPageToken());
                return Status(StatusCode::Aborted, "bad-luck");
            }));

    TestedRange range(
        RequestFake{}, [&](RequestFake const& r) { return mock.Loader(r); });
    auto i0 = range.begin();
    auto i1 = i0;
    EXPECT_TRUE(i0 == i1);
    EXPECT_FALSE(i1 == range.end());
    ++i1;
    auto i2 = i1;
    EXPECT_FALSE(i0 == i1);
    EXPECT_TRUE(i1 == i2);
    ASSERT_FALSE(i1 == range.end());
    auto& item = *i1;
    EXPECT_EQ(StatusCode::Aborted, item.GetStatus().Code());
    EXPECT_THAT(item.GetStatus().Message(), HasSubstr("bad-luck"));
    ++i1;
    EXPECT_TRUE(i1 == range.end());
}

} // namespace test
} // namespace internal
} // namespace csa
