// Copyright 2019 Andrew Karasyov
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

#include "cloudstorageapi/status_or_val.h"
#include "testing_util/assert_ok.h"
#include "testing_util/expect_exception.h"
#include "testing_util/testing_types.h"
#include <gmock/gmock.h>

namespace csa {
namespace {
using ::testing::HasSubstr;

TEST(StatusOrTest, DefaultConstructor)
{
    StatusOrVal<int> actual;
    EXPECT_FALSE(actual.Ok());
    EXPECT_FALSE(actual.GetStatus().Ok());
    EXPECT_FALSE(actual);
}

TEST(StatusOrTest, StatusConstructorNormal)
{
    StatusOrVal<int> actual(Status(StatusCode::NotFound, "NOT FOUND"));
    EXPECT_FALSE(actual.Ok());
    EXPECT_FALSE(actual);
    EXPECT_EQ(StatusCode::NotFound, actual.GetStatus().Code());
    EXPECT_EQ("NOT FOUND", actual.GetStatus().Message());
}

TEST(StatusOrTest, StatusConstructorInvalid)
{
    testing::util::ExpectException<std::invalid_argument>(
        [&] { StatusOrVal<int> actual(Status{}); },
        [&](std::invalid_argument const& ex) { EXPECT_THAT(ex.what(), HasSubstr("StatusOrVal")); },
        "exceptions are disabled: ");
}

TEST(StatusOrTest, StatusAssignment)
{
    auto const error = Status(StatusCode::Unknown, "blah");
    StatusOrVal<int> sorv;
    sorv = error;
    EXPECT_FALSE(sorv.Ok());
    EXPECT_EQ(error, sorv.GetStatus());
}

struct NoEquality
{
};

TEST(StatusOrTest, Equality)
{
    auto const err1 = Status(StatusCode::Unknown, "foo");
    auto const err2 = Status(StatusCode::Unknown, "bar");

    EXPECT_EQ(StatusOrVal<int>(err1), StatusOrVal<int>(err1));
    EXPECT_NE(StatusOrVal<int>(err1), StatusOrVal<int>(err2));

    EXPECT_NE(StatusOrVal<int>(err1), StatusOrVal<int>(1));
    EXPECT_NE(StatusOrVal<int>(1), StatusOrVal<int>(err1));

    EXPECT_EQ(StatusOrVal<int>(1), StatusOrVal<int>(1));
    EXPECT_NE(StatusOrVal<int>(1), StatusOrVal<int>(2));

    // Verify that we can still construct a StatusOrVal with a type that does not
    // support equality, we just can't compare the StatusOrVal for equality with
    // anything else (obviously).
    StatusOrVal<NoEquality> no_equality;
    no_equality = err1;
    no_equality = NoEquality{};
}

TEST(StatusOrTest, ValueConstructor)
{
    StatusOrVal<int> actual(42);
    EXPECT_STATUS_OK(actual);
    EXPECT_TRUE(actual);
    EXPECT_EQ(42, actual.Value());
    EXPECT_EQ(42, std::move(actual).Value());
}

TEST(StatusOrTest, ValueConstAccessors)
{
    StatusOrVal<int> const actual(42);
    EXPECT_STATUS_OK(actual);
    EXPECT_EQ(42, actual.Value());
    EXPECT_EQ(42, std::move(actual).Value());
}

TEST(StatusOrTest, ValueAccessorNonConstThrows)
{
    StatusOrVal<int> actual(Status(StatusCode::Internal, "BAD"));

    testing::util::ExpectException<RuntimeStatusError>([&] { actual.Value(); },
                                                       [&](RuntimeStatusError const& ex) {
                                                           EXPECT_EQ(StatusCode::Internal, ex.GetStatus().Code());
                                                           EXPECT_EQ("BAD", ex.GetStatus().Message());
                                                       },
                                                       "exceptions are disabled: BAD \\[INTERNAL\\]");

    testing::util::ExpectException<RuntimeStatusError>([&] { std::move(actual).Value(); },
                                                       [&](RuntimeStatusError const& ex) {
                                                           EXPECT_EQ(StatusCode::Internal, ex.GetStatus().Code());
                                                           EXPECT_EQ("BAD", ex.GetStatus().Message());
                                                       },
                                                       "exceptions are disabled: BAD \\[INTERNAL\\]");
}

TEST(StatusOrTest, ValueAccessorConstThrows)
{
    StatusOrVal<int> actual(Status(StatusCode::Internal, "BAD"));

    testing::util::ExpectException<RuntimeStatusError>([&] { actual.Value(); },
                                                       [&](RuntimeStatusError const& ex) {
                                                           EXPECT_EQ(StatusCode::Internal, ex.GetStatus().Code());
                                                           EXPECT_EQ("BAD", ex.GetStatus().Message());
                                                       },
                                                       "exceptions are disabled: BAD \\[INTERNAL\\]");

    testing::util::ExpectException<RuntimeStatusError>([&] { std::move(actual).Value(); },
                                                       [&](RuntimeStatusError const& ex) {
                                                           EXPECT_EQ(StatusCode::Internal, ex.GetStatus().Code());
                                                           EXPECT_EQ("BAD", ex.GetStatus().Message());
                                                       },
                                                       "exceptions are disabled: BAD \\[INTERNAL\\]");
}

TEST(StatusOrTest, StatusConstAccessors)
{
    StatusOrVal<int> const actual(Status(StatusCode::Internal, "BAD"));
    EXPECT_EQ(StatusCode::Internal, actual.GetStatus().Code());
    EXPECT_EQ(StatusCode::Internal, std::move(actual).GetStatus().Code());
}

TEST(StatusOrTest, ValueDeference)
{
    StatusOrVal<std::string> actual("42");
    EXPECT_STATUS_OK(actual);
    EXPECT_EQ("42", *actual);
    EXPECT_EQ("42", std::move(actual).Value());
}

TEST(StatusOrTest, ValueConstDeference)
{
    StatusOrVal<std::string> const actual("42");
    EXPECT_STATUS_OK(actual);
    EXPECT_EQ("42", *actual);
    EXPECT_EQ("42", std::move(actual).Value());
}

TEST(StatusOrTest, ValueArrow)
{
    StatusOrVal<std::string> actual("42");
    EXPECT_STATUS_OK(actual);
    EXPECT_EQ(std::string("42"), actual->c_str());
}

TEST(StatusOrTest, ValueConstArrow)
{
    StatusOrVal<std::string> const actual("42");
    EXPECT_STATUS_OK(actual);
    EXPECT_EQ(std::string("42"), actual->c_str());
}

using testing::util::NoDefaultConstructor;

TEST(StatusOrNoDefaultConstructor, DefaultConstructed)
{
    StatusOrVal<NoDefaultConstructor> empty;
    EXPECT_FALSE(empty.Ok());
}

TEST(StatusOrNoDefaultConstructor, ValueConstructed)
{
    StatusOrVal<NoDefaultConstructor> actual(NoDefaultConstructor(std::string("foo")));
    EXPECT_STATUS_OK(actual);
    EXPECT_EQ(actual->Str(), "foo");
}

using testing::util::Observable;

/// @test A default-constructed status does not call the default constructor.
TEST(StatusOrObservableTest, NoDefaultConstruction)
{
    Observable::reset_counters();
    StatusOrVal<Observable> other;
    EXPECT_EQ(0, Observable::default_constructor);
    EXPECT_FALSE(other.Ok());
}

/// @test A copy-constructed status calls the copy constructor for T.
TEST(StatusOrObservableTest, Copy)
{
    Observable::reset_counters();
    StatusOrVal<Observable> other(Observable("foo"));
    EXPECT_EQ("foo", other.Value().Str());
    EXPECT_EQ(1, Observable::move_constructor);

    Observable::reset_counters();
    StatusOrVal<Observable> copy(other);
    EXPECT_EQ(1, Observable::copy_constructor);
    EXPECT_STATUS_OK(copy);
    EXPECT_STATUS_OK(other);
    EXPECT_EQ("foo", copy->Str());
}

/// @test A move-constructed status calls the move constructor for T.
TEST(StatusOrObservableTest, MoveCopy)
{
    Observable::reset_counters();
    StatusOrVal<Observable> other(Observable("foo"));
    EXPECT_EQ("foo", other.Value().Str());
    EXPECT_EQ(1, Observable::move_constructor);

    Observable::reset_counters();
    StatusOrVal<Observable> copy(std::move(other));
    EXPECT_EQ(1, Observable::move_constructor);
    EXPECT_STATUS_OK(copy);
    EXPECT_EQ("foo", copy->Str());
    EXPECT_STATUS_OK(other);
    EXPECT_EQ("moved-out", other->Str());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignment_NoValue_NoValue)
{
    StatusOrVal<Observable> other;
    StatusOrVal<Observable> assigned;
    EXPECT_FALSE(other.Ok());
    EXPECT_FALSE(assigned.Ok());

    Observable::reset_counters();
    assigned = std::move(other);
    EXPECT_FALSE(other.Ok());
    EXPECT_FALSE(assigned.Ok());
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignment_NoValue_Value)
{
    StatusOrVal<Observable> other(Observable("foo"));
    StatusOrVal<Observable> assigned;
    EXPECT_STATUS_OK(other);
    EXPECT_FALSE(assigned.Ok());

    Observable::reset_counters();
    assigned = std::move(other);
    EXPECT_STATUS_OK(other);
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("moved-out", other->Str());
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(1, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignment_NoValue_T)
{
    Observable other("foo");
    StatusOrVal<Observable> assigned;
    EXPECT_FALSE(assigned.Ok());

    Observable::reset_counters();
    assigned = std::move(other);
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("moved-out", other.Str());
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(1, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignment_Value_NoValue)
{
    StatusOrVal<Observable> other;
    StatusOrVal<Observable> assigned(Observable("bar"));
    EXPECT_FALSE(other.Ok());
    EXPECT_STATUS_OK(assigned);

    Observable::reset_counters();
    assigned = std::move(other);
    EXPECT_FALSE(other.Ok());
    EXPECT_FALSE(assigned.Ok());
    EXPECT_EQ(1, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignment_Value_Value)
{
    StatusOrVal<Observable> other(Observable("foo"));
    StatusOrVal<Observable> assigned(Observable("bar"));
    EXPECT_STATUS_OK(other);
    EXPECT_STATUS_OK(assigned);

    Observable::reset_counters();
    assigned = std::move(other);
    EXPECT_STATUS_OK(other);
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(1, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("moved-out", other->Str());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignment_Value_T)
{
    Observable other("foo");
    StatusOrVal<Observable> assigned(Observable("bar"));
    EXPECT_STATUS_OK(assigned);

    Observable::reset_counters();
    assigned = std::move(other);
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(1, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("moved-out", other.Str());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignment_NoValue_NoValue)
{
    StatusOrVal<Observable> other;
    StatusOrVal<Observable> assigned;
    EXPECT_FALSE(other.Ok());
    EXPECT_FALSE(assigned.Ok());

    Observable::reset_counters();
    assigned = other;
    EXPECT_FALSE(other.Ok());
    EXPECT_FALSE(assigned.Ok());
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignment_NoValue_Value)
{
    StatusOrVal<Observable> other(Observable("foo"));
    StatusOrVal<Observable> assigned;
    EXPECT_STATUS_OK(other);
    EXPECT_FALSE(assigned.Ok());

    Observable::reset_counters();
    assigned = other;
    EXPECT_STATUS_OK(other);
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("foo", other->Str());
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(1, Observable::copy_constructor);
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignment_NoValue_T)
{
    Observable other("foo");
    StatusOrVal<Observable> assigned;
    EXPECT_FALSE(assigned.Ok());

    Observable::reset_counters();
    assigned = other;
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("foo", other.Str());
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(1, Observable::copy_constructor);
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignment_Value_NoValue)
{
    StatusOrVal<Observable> other;
    StatusOrVal<Observable> assigned(Observable("bar"));
    EXPECT_FALSE(other.Ok());
    EXPECT_STATUS_OK(assigned);

    Observable::reset_counters();
    assigned = other;
    EXPECT_FALSE(other.Ok());
    EXPECT_FALSE(assigned.Ok());
    EXPECT_EQ(1, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignment_Value_Value)
{
    StatusOrVal<Observable> other(Observable("foo"));
    StatusOrVal<Observable> assigned(Observable("bar"));
    EXPECT_STATUS_OK(other);
    EXPECT_STATUS_OK(assigned);

    Observable::reset_counters();
    assigned = other;
    EXPECT_STATUS_OK(other);
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(1, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("foo", other->Str());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignment_Value_T)
{
    Observable other("foo");
    StatusOrVal<Observable> assigned(Observable("bar"));
    EXPECT_STATUS_OK(assigned);

    Observable::reset_counters();
    assigned = other;
    EXPECT_STATUS_OK(assigned);
    EXPECT_EQ(0, Observable::destructor);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(1, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
    EXPECT_EQ("foo", assigned->Str());
    EXPECT_EQ("foo", other.Str());
}

TEST(StatusOrObservableTest, MoveValue)
{
    StatusOrVal<Observable> other(Observable("foo"));
    EXPECT_EQ("foo", other.Value().Str());

    Observable::reset_counters();
    auto observed = std::move(other).Value();
    EXPECT_EQ("foo", observed.Str());
    EXPECT_STATUS_OK(other);
    EXPECT_EQ("moved-out", other->Str());
}

}  // namespace
}  // namespace csa
