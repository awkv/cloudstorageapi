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
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {
namespace test {

using ::csa::testing::util::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

struct Item
{
    std::string data;
};

// A generic request. Fields with a "testonly_" prefix are used for testing but
// are not used in the real code.
struct Request
{
    std::string m_testonlyPageToken;
    void SetPageToken(std::string token) { m_testonlyPageToken = std::move(token); }
};

// Looks like a minimal protobuf response message. Fields with a "testonly_"
// prefix are used for testing but are not used in the real code.
struct ProtoResponse
{
    std::vector<Item> m_testonlyItems;
    std::string m_testonlyPageToken;
    std::string* GetMutableNextPageToken() { return &m_testonlyPageToken; }

    // Used for setting the token in tests, but it's not used in the real code.
    void TestonlySetPageToken(std::string s) { m_testonlyPageToken = std::move(s); }
};

// Looks like a minimal struct response message. Fields with a "testonly_"
// prefix are used for testing but are not used in the real code.
struct StructResponse
{
    std::vector<Item> m_testonlyItems;
    std::string m_nextPageToken;

    // Used for setting the token in tests, but it's not used in the real code.
    void TestonlySetPageToken(std::string s) { m_nextPageToken = std::move(s); }
};

using ItemRange = PaginationRange<Item>;

template <typename Response>
class MockRpc
{
public:
    MOCK_METHOD(StatusOrVal<Response>, Loader, (Request const&));
};

// A fixture for a "typed test". Each test below will be tested with a
// `ProtoResponse` object and a `StructResponse` object.
template <typename T>
class PaginationRangeTest : public ::testing::Test
{
};

using ResponseTypes = ::testing::Types<ProtoResponse, StructResponse>;
TYPED_TEST_SUITE(PaginationRangeTest, ResponseTypes);

TYPED_TEST(PaginationRangeTest, TypedEmpty)
{
    using ResponseType = TypeParam;
    MockRpc<ResponseType> mock;
    EXPECT_CALL(mock, Loader).WillOnce([](Request const& request) {
        EXPECT_TRUE(request.m_testonlyPageToken.empty());
        return ResponseType{};
    });
    auto range = MakePaginationRange<ItemRange>(
        Request{}, [&mock](Request const& r) { return mock.Loader(r); },
        [](ResponseType const& r) { return r.m_testonlyItems; });
    EXPECT_TRUE(range.begin() == range.end());
}

TYPED_TEST(PaginationRangeTest, SinglePage)
{
    using ResponseType = TypeParam;
    MockRpc<ResponseType> mock;
    EXPECT_CALL(mock, Loader).WillOnce([](Request const& request) {
        EXPECT_TRUE(request.m_testonlyPageToken.empty());
        ResponseType response;
        response.m_testonlyItems.push_back(Item{"p1"});
        response.m_testonlyItems.push_back(Item{"p2"});
        return response;
    });

    auto range = MakePaginationRange<ItemRange>(
        Request{}, [&mock](Request const& r) { return mock.Loader(r); },
        [](ResponseType const& r) { return r.m_testonlyItems; });
    std::vector<std::string> names;
    for (auto& p : range)
    {
        if (!p)
            break;
        names.push_back(p->data);
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2"));
}
TYPED_TEST(PaginationRangeTest, NonProtoRange)
{
    using ResponseType = TypeParam;
    MockRpc<ResponseType> mock;
    EXPECT_CALL(mock, Loader).WillOnce([](Request const& request) {
        EXPECT_TRUE(request.m_testonlyPageToken.empty());
        ResponseType response;
        response.m_testonlyItems.push_back(Item{"p1"});
        response.m_testonlyItems.push_back(Item{"p2"});
        return response;
    });

    using NonProtoRange = PaginationRange<std::string>;
    auto range = MakePaginationRange<NonProtoRange>(
        Request{}, [&mock](Request const& r) { return mock.Loader(r); },
        [](ResponseType const& r) {
            std::vector<std::string> v;
            for (auto const& i : r.m_testonlyItems)
            {
                v.push_back(i.data);
            }
            return v;
        });

    std::vector<std::string> names;
    for (auto& p : range)
    {
        if (!p)
            break;
        names.push_back(*p);
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2"));
}

TYPED_TEST(PaginationRangeTest, TwoPages)
{
    using ResponseType = TypeParam;
    MockRpc<ResponseType> mock;
    EXPECT_CALL(mock, Loader)
        .WillOnce([](Request const& request) {
            EXPECT_TRUE(request.m_testonlyPageToken.empty());
            ResponseType response;
            response.TestonlySetPageToken("t1");
            response.m_testonlyItems.push_back(Item{"p1"});
            response.m_testonlyItems.push_back(Item{"p2"});
            return response;
        })
        .WillOnce([](Request const& request) {
            EXPECT_EQ("t1", request.m_testonlyPageToken);
            ResponseType response;
            response.m_testonlyItems.push_back(Item{"p3"});
            response.m_testonlyItems.push_back(Item{"p4"});
            return response;
        });

    auto range = MakePaginationRange<ItemRange>(
        Request{}, [&mock](Request const& r) { return mock.Loader(r); },
        [](ResponseType const& r) { return r.m_testonlyItems; });
    std::vector<std::string> names;
    for (auto& p : range)
    {
        if (!p)
            break;
        names.push_back(p->data);
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TYPED_TEST(PaginationRangeTest, TwoPagesWithError)
{
    using ResponseType = TypeParam;
    MockRpc<ResponseType> mock;
    EXPECT_CALL(mock, Loader)
        .WillOnce([](Request const& request) {
            EXPECT_TRUE(request.m_testonlyPageToken.empty());
            ResponseType response;
            response.TestonlySetPageToken("t1");
            response.m_testonlyItems.push_back(Item{"p1"});
            response.m_testonlyItems.push_back(Item{"p2"});
            return response;
        })
        .WillOnce([](Request const& request) {
            EXPECT_EQ("t1", request.m_testonlyPageToken);
            ResponseType response;
            response.TestonlySetPageToken("t2");
            response.m_testonlyItems.push_back(Item{"p3"});
            response.m_testonlyItems.push_back(Item{"p4"});
            return response;
        })
        .WillOnce([](Request const& request) {
            EXPECT_EQ("t2", request.m_testonlyPageToken);
            return Status(StatusCode::Aborted, "bad-luck");
        });

    auto range = MakePaginationRange<ItemRange>(
        Request{}, [&mock](Request const& r) { return mock.Loader(r); },
        [](ResponseType const& r) { return r.m_testonlyItems; });
    std::vector<std::string> names;
    for (auto& p : range)
    {
        if (!p)
        {
            EXPECT_EQ(StatusCode::Aborted, p.GetStatus().Code());
            EXPECT_THAT(p.GetStatus().Message(), HasSubstr("bad-luck"));
            break;
        }
        names.push_back(p->data);
    }
    EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TYPED_TEST(PaginationRangeTest, IteratorCoverage)
{
    using ResponseType = TypeParam;
    MockRpc<ResponseType> mock;
    EXPECT_CALL(mock, Loader)
        .WillOnce([](Request const& request) {
            EXPECT_TRUE(request.m_testonlyPageToken.empty());
            ResponseType response;
            response.TestonlySetPageToken("t1");
            response.m_testonlyItems.push_back(Item{"p1"});
            return response;
        })
        .WillOnce([](Request const& request) {
            EXPECT_EQ("t1", request.m_testonlyPageToken);
            return Status(StatusCode::Aborted, "bad-luck");
        });

    auto range = MakePaginationRange<ItemRange>(
        Request{}, [&mock](Request const& r) { return mock.Loader(r); },
        [](ResponseType const& r) { return r.m_testonlyItems; });
    auto i0 = range.begin();
    auto i1 = i0;
    EXPECT_TRUE(i0 == i1);
    EXPECT_FALSE(i1 == range.end());
    ++i1;
    auto i2 = i1;
    EXPECT_TRUE(i1 == i2);
    ASSERT_FALSE(i1 == range.end());
    auto& item = *i1;
    EXPECT_EQ(StatusCode::Aborted, item.GetStatus().Code());
    EXPECT_THAT(item.GetStatus().Message(), HasSubstr("bad-luck"));
    ++i1;
    EXPECT_TRUE(i1 == range.end());
}

TEST(RangeFromPagination, Unimplemented)
{
    using NonProtoRange = PaginationRange<std::string>;
    auto range = MakeUnimplementedPaginationRange<NonProtoRange>();
    auto i = range.begin();
    EXPECT_NE(i, range.end());
    EXPECT_THAT(*i, StatusIs(StatusCode::Unimplemented));
}

}  // namespace test
}  // namespace internal
}  // namespace csa
