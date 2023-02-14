// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/list_folder_reader.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "canonical_errors.h"
#include "testing_util/mock_cloud_storage_client.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <stdexcept>

namespace csa {
namespace internal {

using ::csa::internal::ListFolderRequest;
using ::csa::internal::ListFolderResponse;
using ::csa::internal::canonical_errors::PermanentError;
using ::testing::ContainerEq;
using ::testing::Return;

FolderMetadata CreateElement(int index)
{
    FolderMetadata fmeta;
    std::string id = "folder-" + std::to_string(index);
    fmeta.SetName(id);
    fmeta.SetCloudId("https://this.is.cloud.id/" + id);
    fmeta.SetSize(4096);
    return fmeta;
}

TEST(ListFolderReaderTest, Basic)
{
    // Create a synthetic list of FolderMetadata elements, each request will
    // return 2 of them.
    std::vector<FolderMetadata> expected;

    const int page_count = 3;
    for (int i = 0; i != 2 * page_count; ++i)
    {
        expected.emplace_back(CreateElement(i));
    }

    auto create_mock = [&expected, page_count](int i) {
        ListFolderResponse response;
        if (i < page_count)
        {
            if (i != page_count - 1)
            {
                response.m_nextPageToken = "page-" + std::to_string(i);
            }
            response.m_items.push_back(expected[2 * i]);
            response.m_items.push_back(expected[2 * i + 1]);
        }
        return [response](ListFolderRequest const&) { return MakeStatusOrVal(response); };
    };

    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, ListFolder).WillOnce(create_mock(0)).WillOnce(create_mock(1)).WillOnce(create_mock(2));

    auto reader = MakePaginationRange<ListFolderReader>(
        ListFolderRequest("foo-bar-baz"), [mock](ListFolderRequest const& r) { return mock->ListFolder(r); },
        [](internal::ListFolderResponse r) { return std::move(r.m_items); });
    std::vector<FolderMetadata> actual;
    for (auto&& folder : reader)
    {
        ASSERT_STATUS_OK(folder);
        struct FolderMetaCollector
        {
            std::vector<FolderMetadata>& m_folderMetas;
            void operator()(FolderMetadata&& m) { m_folderMetas.emplace_back(std::move(m)); }
            void operator()(FileMetadata&& m) { EXPECT_FALSE(true); }
        };
        std::visit(FolderMetaCollector{actual}, std::move(folder).Value());
    }
    EXPECT_THAT(actual, ContainerEq(expected));
}

TEST(ListFolderReaderTest, Empty)
{
    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, ListFolder).WillOnce(Return(MakeStatusOrVal(ListFolderResponse{})));

    auto reader = MakePaginationRange<ListFolderReader>(
        ListFolderRequest("foo-bar-baz"), [mock](ListFolderRequest const& r) { return mock->ListFolder(r); },
        [](ListFolderResponse r) { return std::move(r.m_items); });
    auto count = std::distance(reader.begin(), reader.end());
    EXPECT_EQ(0, count);
}

TEST(ListFolderReaderTest, PermanentFailure)
{
    // Create a synthetic list of ObjectMetadata elements, each request will
    // return 2 of them.
    std::vector<FolderMetadata> expected;

    int const page_count = 2;
    for (int i = 0; i != 2 * page_count; ++i)
    {
        expected.emplace_back(CreateElement(i));
    }

    auto create_mock = [&](int i) {
        ListFolderResponse response;
        response.m_nextPageToken = "page-" + std::to_string(i);
        response.m_items.emplace_back(CreateElement(2 * i));
        response.m_items.emplace_back(CreateElement(2 * i + 1));
        return [response](ListFolderRequest const&) { return StatusOrVal<ListFolderResponse>(response); };
    };

    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, ListFolder)
        .WillOnce(create_mock(0))
        .WillOnce(create_mock(1))
        .WillOnce([](ListFolderRequest const&) { return StatusOrVal<ListFolderResponse>(PermanentError()); });

    auto reader = MakePaginationRange<ListFolderReader>(
        ListFolderRequest("test-bucket"), [mock](ListFolderRequest const& r) { return mock->ListFolder(r); },
        [](ListFolderResponse r) { return std::move(r.m_items); });
    std::vector<FolderMetadata> actual;
    bool has_status_or_error = false;
    for (auto&& folder : reader)
    {
        if (folder.Ok())
        {
            struct FolderMetaCollector
            {
                std::vector<FolderMetadata>& m_folderMetas;
                void operator()(FolderMetadata&& m) { m_folderMetas.emplace_back(std::move(m)); }
                void operator()(FileMetadata&& m) { EXPECT_FALSE(true); }
            };
            std::visit(FolderMetaCollector{actual}, std::move(folder).Value());
            continue;
        }
        // The iteration should fail only once, an error should reset the iterator
        // to `end()`.
        EXPECT_FALSE(has_status_or_error);
        has_status_or_error = true;
        // Verify the error is what we expect.
        Status status = std::move(folder).GetStatus();
        EXPECT_EQ(PermanentError().Code(), status.Code());
        EXPECT_EQ(PermanentError().Message(), status.Message());
    }
    // The iteration should have returned an error at least once.
    EXPECT_TRUE(has_status_or_error);

    // The iteration should have returned all the elements prior to the error.
    EXPECT_THAT(actual, ContainerEq(expected));
}

}  // namespace internal
}  // namespace csa
