// Copyright 2020 Andrew Karasyov
//
// Copyright 2018 Google LLC
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

#include "cloudstorageapi/internal/utils.h"
#include "cloudstorageapi/internal/random.h"
#include <gmock/gmock.h>

namespace csa {
namespace {

/// Environment variable utils
using csa::internal::SetEnv;
using csa::internal::GetEnv;
using csa::internal::UnsetEnv;

TEST(SetEnvTest, SetEmptyEnvVar)
{
    SetEnv("foo", "");
#ifdef _WIN32
    EXPECT_FALSE(GetEnv("foo").has_value());
#else
    EXPECT_TRUE(GetEnv("foo").has_value());
#endif  // _WIN32
}

TEST(SetEnvTest, UnsetEnvWithNullptr)
{
    SetEnv("foo", "bar");
    EXPECT_TRUE(GetEnv("foo").has_value());
    SetEnv("foo", nullptr);
    EXPECT_FALSE(GetEnv("foo").has_value());
}

TEST(SetEnvTest, UnsetEnv)
{
    SetEnv("foo", "bar");
    EXPECT_TRUE(GetEnv("foo").has_value());
    UnsetEnv("foo");
    EXPECT_FALSE(GetEnv("foo").has_value());
}

/// BinaryDataAsDebugString test
using csa::internal::BinaryDataAsDebugString;

TEST(BinaryDataAsDebugStringTest, Simple)
{
    auto actual = BinaryDataAsDebugString("123abc", 6);
    EXPECT_EQ(
        "123abc                   "
        "313233616263                                    \n",
        actual);
}

TEST(BinaryDataAsDebugStringTest, Multiline)
{
    auto actual =
        BinaryDataAsDebugString(" 123456789 123456789 123456789 123456789", 40);
    EXPECT_EQ(
        " 123456789 123456789 123 "
        "203132333435363738392031323334353637383920313233\n"
        "456789 123456789         "
        "34353637383920313233343536373839                \n",
        actual);
}

TEST(BinaryDataAsDebugStringTest, Blanks)
{
    auto actual = BinaryDataAsDebugString("\n \r \t \v \b \a \f ", 14);
    EXPECT_EQ(
        ". . . . . . .            "
        "0a200d2009200b20082007200c20                    \n",
        actual);
}

TEST(BinaryDataAsDebugStringTest, NonPrintable)
{
    auto actual = BinaryDataAsDebugString("\x03\xf1 abcd", 7);
    EXPECT_EQ(
        ".. abcd                  "
        "03f12061626364                                  \n",
        actual);
}

TEST(BinaryDataAsDebugStringTest, Limit)
{
    auto actual = BinaryDataAsDebugString(
        " 123456789 123456789 123456789 123456789", 40, 24);
    EXPECT_EQ(
        " 123456789 123456789 123 "
        "203132333435363738392031323334353637383920313233\n",
        actual);
}

// GenerateMessageBoundary test

using ::testing::HasSubstr;
using ::testing::Not;
using csa::internal::GenerateMessageBoundary;

TEST(GenerateMessageBoundaryTest, Simple)
{
    auto generator = csa::internal::MakeDefaultPRNG();

    auto string_generator = [&generator](int n)
    {
        static std::string const chars =
            "abcdefghijklmnopqrstuvwxyz012456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        return csa::internal::Sample(generator, n, chars);
    };

    // The magic constants here are uninteresting. We just want a large message
    // and a relatively short string to start searching for a boundary.
    auto message = string_generator(1024);
    auto boundary =
        GenerateMessageBoundary(message, std::move(string_generator), 16, 4);
    EXPECT_THAT(message, Not(HasSubstr(boundary)));
}

TEST(GenerateMessageBoundaryTest, RequiresGrowth)
{
    static std::string const chars =
        "abcdefghijklmnopqrstuvwxyz012456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    auto generator = csa::internal::MakeDefaultPRNG();

    // This test will ensure that both the message and the initial string contain
    // at least this many common characters.
    int constexpr MatchedStringLength = 32;
    int constexpr MismatchedStringLength = 512;

    auto g1 = csa::internal::MakeDefaultPRNG();
    std::string message =
        csa::internal::Sample(g1, MismatchedStringLength, chars);
    // Copy the PRNG to obtain the same sequence of random numbers that
    // `generator` will create later.
    g1 = generator;
    message += csa::internal::Sample(g1, MatchedStringLength, chars);
    g1 = csa::internal::MakeDefaultPRNG();
    message +=
        csa::internal::Sample(g1, MismatchedStringLength, chars);

    auto string_generator = [&generator](int n)
    {
        return csa::internal::Sample(generator, n, chars);
    };

    // The initial_size and growth_size parameters are set to
    // (MatchedStringLength / 2) and (MatchedStringLength / 4) respectively,
    // that forces the algorithm to find the initial string, and to grow it
    // several times before the MatchedStringLength common characters are
    // exhausted.
    auto boundary = GenerateMessageBoundary(message, std::move(string_generator),
        MatchedStringLength / 2,
        MatchedStringLength / 4);
    EXPECT_THAT(message, Not(HasSubstr(boundary)));

    // We expect that the string is longer than the common characters.
    EXPECT_LT(MatchedStringLength, boundary.size());
}

// RoundUpToQuantum test
using csa::internal::RoundUpToQuantum;

namespace {
struct RoundUpToQuantumTestData
{
    std::size_t m_val;
    std::size_t m_quantumSize;
    std::size_t m_expected;
} roundUpToQuantumTestData[] = {
    {0, 2, 0},
    {1, 2, 2},
    {2, 2, 2},
    {3, 2, 4},
    {4, 2, 4},
    {5, 2, 6},
    {6, 2, 6},
    //
    {0, 256, 0},
    {1, 256, 256},
    {256, 256, 256},
    {257, 256, 2*256},// 512
    {400, 256, 2*256},// 512
    {512, 256, 2*256},// 512
    {513, 256, 3*256},// 768
    {1000, 256, 4*256},// 1024
    {1025, 256, 5*256}, // 1280
    {2049, 256, 9*256}, // 2304
};

struct RoundUpToQuantumTest : public ::testing::TestWithParam<RoundUpToQuantumTestData>
{
};

} // namespace

INSTANTIATE_TEST_SUITE_P(RoundUpToQuantumTestInst, RoundUpToQuantumTest, ::testing::ValuesIn(roundUpToQuantumTestData));

TEST_P(RoundUpToQuantumTest, Simple)
{
    auto param = GetParam();

    EXPECT_EQ(RoundUpToQuantum(param.m_val, param.m_quantumSize), param.m_expected);
}

}  // namespace
}  // namespace csa
