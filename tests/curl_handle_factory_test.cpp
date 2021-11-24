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

#include "cloudstorageapi/internal/curl_handle_factory.h"
#include <gmock/gmock.h>
#include <map>

namespace csa {
namespace internal {

using ::testing::IsEmpty;

// Version of DefaultCurlHandleFactory that keeps track of what calls have been
// made to SetCurlStringOption.
class OverriddenDefaultCurlHandleFactory : public DefaultCurlHandleFactory
{
public:
    OverriddenDefaultCurlHandleFactory() = default;
    explicit OverriddenDefaultCurlHandleFactory(Options const& options) : DefaultCurlHandleFactory(options) {}

protected:
    void SetCurlStringOption(CURL* handle, CURLoption option_tag, const char* value) override
    {
        m_setOptions[option_tag] = value;
        DefaultCurlHandleFactory::SetCurlStringOption(handle, option_tag, value);
    }

public:
    std::map<int, std::string> m_setOptions;
};

// Version of DefaultCurlHandleFactory that keeps track of what calls have been
// made to SetCurlStringOption.
class OverriddenPooledCurlHandleFactory : public PooledCurlHandleFactory
{
public:
    explicit OverriddenPooledCurlHandleFactory(std::size_t maximum_size) : PooledCurlHandleFactory(maximum_size) {}
    OverriddenPooledCurlHandleFactory(std::size_t maximum_size, Options const& options)
        : PooledCurlHandleFactory(maximum_size, options)
    {
    }

protected:
    void SetCurlStringOption(CURL* handle, CURLoption option_tag, const char* value) override
    {
        m_setOptions[option_tag] = value;
        PooledCurlHandleFactory::SetCurlStringOption(handle, option_tag, value);
    }

public:
    std::map<int, std::string> m_setOptions;
};

TEST(CurlHandleFactoryTest, DefaultFactoryNoChannelOptionsDoesntCallSetOptions)
{
    OverriddenDefaultCurlHandleFactory objectUnderTest;

    objectUnderTest.CreateHandle();
    EXPECT_THAT(objectUnderTest.m_setOptions, IsEmpty());
}

TEST(CurlHandleFactoryTest, DefaultFactoryChannelOptionsCallsSetOptions)
{
    auto options = Options{}.Set<CARootsFilePathOption>("foo");
    OverriddenDefaultCurlHandleFactory objectUnderTest(options);

    auto const expected = std::make_pair(CURLOPT_CAINFO, std::string("foo"));

    objectUnderTest.CreateHandle();
    EXPECT_THAT(objectUnderTest.m_setOptions, testing::ElementsAre(expected));
}

TEST(CurlHandleFactoryTest, PooledFactoryNoChannelOptionsDoesntCallSetOptions)
{
    OverriddenPooledCurlHandleFactory objectUnderTest(2);

    objectUnderTest.CreateHandle();
    EXPECT_THAT(objectUnderTest.m_setOptions, IsEmpty());
}

TEST(CurlHandleFactoryTest, PooledFactoryChannelOptionsCallsSetOptions)
{
    auto options = Options{}.Set<CARootsFilePathOption>("foo");
    OverriddenPooledCurlHandleFactory objectUnderTest(2, options);

    auto const expected = std::make_pair(CURLOPT_CAINFO, std::string("foo"));

    {
        objectUnderTest.CreateHandle();
        EXPECT_THAT(objectUnderTest.m_setOptions, testing::ElementsAre(expected));
    }
    // the above should have left the handle in the cache. Check that cached
    // handles get their options set again.
    objectUnderTest.m_setOptions.clear();

    objectUnderTest.CreateHandle();
    EXPECT_THAT(objectUnderTest.m_setOptions, testing::ElementsAre(expected));
}

}  // namespace internal
}  // namespace csa
