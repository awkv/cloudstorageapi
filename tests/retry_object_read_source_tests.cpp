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

// #include "cloudstorageapi/testing/retry_tests.h"
#include "cloudstorageapi/cloud_storage_client.h"
#include "cloudstorageapi/retry_policy.h"
#include "canonical_errors.h"
#include "testing_util/mock_cloud_storage_client.h"
#include "testing_util/mock_object_read_source.h"
#include "testing_util/chrono_literals.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using ::testing::HasSubstr;
using ::testing::Return;
using ::csa::testing::util::chrono_literals::operator"" _us;
using ::csa::internal::canonical_errors::PermanentError;
using ::csa::internal::canonical_errors::TransientError;

class BackoffPolicyMockState
{
public:
    MOCK_METHOD(std::chrono::milliseconds, OnCompletion, ());

    int m_numCallsFromLastClone{0};
    int m_numClones{0};
};

// Pretend independent backoff policies, but be only one under the hood.
// This is a trick to count the number of `clone()` calls.
class BackoffPolicyMock : public BackoffPolicy
{
public:
    BackoffPolicyMock() : m_state(new BackoffPolicyMockState) {}

    std::chrono::milliseconds OnCompletion() override
    {
        ++m_state->m_numCallsFromLastClone;
        return m_state->OnCompletion();
    }

    std::unique_ptr<BackoffPolicy> Clone() const override
    {
        m_state->m_numCallsFromLastClone = 0;
        ++m_state->m_numClones;
        return std::unique_ptr<::csa::BackoffPolicy>(new BackoffPolicyMock(*this));
    }

    int NumCallsFromLastClone() { return m_state->m_numCallsFromLastClone; }
    int NumClones() { return m_state->m_numClones; }

    std::shared_ptr<BackoffPolicyMockState> m_state;
};

Options BasicTestPolicies()
{
    return Options{}
        .Set<RetryPolicyOption>(::csa::LimitedErrorCountRetryPolicy(3).Clone())
        .Set<BackoffPolicyOption>(
            // Make the tests faster.
            ::csa::ExponentialBackoffPolicy(1_us, 2_us, 2).Clone());
}

/// @test No failures scenario.
TEST(RetryObjectReadSourceTest, NoFailures)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile).WillOnce([](ReadFileRangeRequest const&) {
        auto source = std::make_unique<MockObjectReadSource>();
        EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
        return std::unique_ptr<ObjectReadSource>(std::move(source));
    });

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_STATUS_OK(source);
    ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
}

/// @test Permanent failure when creating the raw source
TEST(RetryObjectReadSourceTest, PermanentFailureOnSessionCreation)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile).WillOnce([](ReadFileRangeRequest const&) { return PermanentError(); });

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_FALSE(source);
    EXPECT_EQ(PermanentError().Code(), source.GetStatus().Code());
}

/// @test Transient failures exhaust retry policy when creating the raw source
TEST(RetryObjectReadSourceTest, TransientFailuresExhaustOnSessionCreation)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile).Times(4).WillRepeatedly([](ReadFileRangeRequest const&) {
        return TransientError();
    });

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_FALSE(source);
    EXPECT_EQ(TransientError().Code(), source.GetStatus().Code());
}

/// @test Recovery from a transient failures when creating the raw source
TEST(RetryObjectReadSourceTest, SessionCreationRecoversFromTransientFailures)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile)
        .WillOnce([](ReadFileRangeRequest const&) { return TransientError(); })
        .WillOnce([](ReadFileRangeRequest const&) { return TransientError(); })
        .WillOnce([](ReadFileRangeRequest const&) {
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        });

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_STATUS_OK(source);
    EXPECT_STATUS_OK((*source)->Read(nullptr, 1024));
}

/// @test A permanent error after a successful read
TEST(RetryObjectReadSourceTest, PermanentReadFailure)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto* raw_source = new MockObjectReadSource;
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile).WillOnce([raw_source](ReadFileRangeRequest const&) {
        return std::unique_ptr<ObjectReadSource>(raw_source);
    });
    EXPECT_CALL(*raw_source, Read).WillOnce(Return(ReadSourceResult{})).WillOnce(Return(PermanentError()));

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_STATUS_OK(source);
    ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
    auto res = (*source)->Read(nullptr, 1024);
    ASSERT_FALSE(res);
    EXPECT_EQ(PermanentError().Code(), res.GetStatus().Code());
}

/// @test Test if backoff policy is reset on success
TEST(RetryObjectReadSourceTest, BackoffPolicyResetOnSuccess)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    int num_backoff_policy_called = 0;
    BackoffPolicyMock backoff_policy_mock;
    EXPECT_CALL(*backoff_policy_mock.m_state, OnCompletion()).WillRepeatedly([&] {
        ++num_backoff_policy_called;
        return std::chrono::milliseconds(0);
    });
    auto client =
        std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client),
                                      BasicTestPolicies().Set<BackoffPolicyOption>(backoff_policy_mock.Clone()));

    EXPECT_EQ(0, num_backoff_policy_called);

    EXPECT_CALL(*raw_client, ReadFile)
        .WillOnce([](ReadFileRangeRequest const&) {
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(TransientError()));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        })
        .WillOnce([](ReadFileRangeRequest const&) {
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(TransientError()));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        })
        .WillOnce([](ReadFileRangeRequest const&) {
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{})).WillOnce(Return(TransientError()));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        })
        .WillOnce([](ReadFileRangeRequest const&) {
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        });

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_STATUS_OK(source);
    // The policy was cloned by the options, the ctor, and once by the RetryClient
    EXPECT_EQ(3, backoff_policy_mock.NumClones());
    EXPECT_EQ(0, num_backoff_policy_called);

    // raw_source1 and raw_source2 fail, then a success
    ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
    // Two retries, so the backoff policy was called twice.
    EXPECT_EQ(2, num_backoff_policy_called);
    // The backoff should have been cloned during the read.
    EXPECT_EQ(4, backoff_policy_mock.NumClones());
    // The backoff policy was used twice in the first retry.
    EXPECT_EQ(2, backoff_policy_mock.NumCallsFromLastClone());

    // raw_source3 fails, then a success
    ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
    // This read caused a third retry.
    EXPECT_EQ(3, num_backoff_policy_called);
    // The backoff should have been cloned during the read.
    EXPECT_EQ(5, backoff_policy_mock.NumClones());
    // The backoff policy was only once in the second retry.
    EXPECT_EQ(1, backoff_policy_mock.NumCallsFromLastClone());
}

/// @test Check that retry policy is shared between reads and resetting session
TEST(RetryObjectReadSourceTest, RetryPolicyExhaustedOnResetSession)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile)
        .WillOnce([](ReadFileRangeRequest const&) {
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{})).WillOnce(Return(TransientError()));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        })
        .WillOnce([](ReadFileRangeRequest const&) { return TransientError(); })
        .WillOnce([](ReadFileRangeRequest const&) { return TransientError(); })
        .WillOnce([](ReadFileRangeRequest const&) { return TransientError(); });

    auto source = client->ReadFile(ReadFileRangeRequest{});
    ASSERT_STATUS_OK(source);
    ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
    auto res = (*source)->Read(nullptr, 1024);
    // It takes 4 retry attempts to exhaust the policy. Only a retry policy shared
    // between reads and resetting the session could exhaust it.
    ASSERT_FALSE(res);
    EXPECT_EQ(TransientError().Code(), res.GetStatus().Code());
    EXPECT_THAT(res.GetStatus().Message(), HasSubstr("Retry policy exhausted"));
}

/// @test `ReadLast` behaviour after a transient failure
TEST(RetryObjectReadSourceTest, TransientFailureWithReadLastOption)
{
    auto raw_client = std::make_shared<MockClient>(EProvider::GoogleDrive);
    auto client = std::make_shared<RetryClient>(std::shared_ptr<internal::RawClient>(raw_client), BasicTestPolicies());

    EXPECT_CALL(*raw_client, ReadFile)
        .WillOnce([](ReadFileRangeRequest const& req) {
            EXPECT_EQ(1029, req.GetOption<ReadLast>().Value());
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read)
                .WillOnce(Return(ReadSourceResult{static_cast<std::size_t>(1024), HttpResponse{200, "", {}}}))
                .WillOnce(Return(TransientError()));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        })
        .WillOnce([](ReadFileRangeRequest const& req) {
            EXPECT_EQ(5, req.GetOption<ReadLast>().Value());
            auto source = std::make_unique<MockObjectReadSource>();
            EXPECT_CALL(*source, Read).WillOnce(Return(ReadSourceResult{}));
            return std::unique_ptr<ObjectReadSource>(std::move(source));
        });

    ReadFileRangeRequest req("test_file");
    req.SetOption(ReadLast(1029));
    auto source = client->ReadFile(req);
    ASSERT_STATUS_OK(source);
    ASSERT_STATUS_OK((*source)->Read(nullptr, 1024));
    auto res = (*source)->Read(nullptr, 1024);
    ASSERT_TRUE(res);
}

}  // namespace internal
}  // namespace csa
