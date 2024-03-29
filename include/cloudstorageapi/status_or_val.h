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

#pragma once

#include "status.h"
#include <optional>
#include <stdexcept>
#include <utility>

namespace csa {
/**
 * Holds a value or a `Status` indicating why there is no value.
 *
 * `StatusOrVal<T>` represents either a usable `T` value or a `Status` object
 * explaining why a `T` value is not present. It looks like a std::optional<T>,
 *  in that you first check its validity using a conversion to bool (or by calling
 * `StatusOrVal::Ok()`), then you may dereference the object to access the
 *  contained value. It is undefined behavior (UB) to dereference a
 * `StatusOrVal<T>` that is not "Ok". For example:
 *
 * @code
 * StatusOrVal<Foo> foo = FetchFoo();
 * if (!foo) {  // Same as !foo.Ok()
 *   // handle error and probably look at foo.status()
 * } else {
 *   foo->DoSomethingFooey();  // UB if !foo
 * }
 * @endcode
 *
 * Alternatively, you may call the `StatusOrVal::value()` member function,
 * which is defined to throw an exception if there is no `T` value, or crash
 * the program if exceptions are disabled. It is never UB to call
 * `.value()`.
 *
 * @code
 * StatusOrVal<Foo> foo = FetchFoo();
 * foo.value().DoSomethingFooey();  // May throw/crash if there is no value
 * @endcode
 *
 * Functions that can fail will often return a `StatusOrVal<T>` instead of
 * returning an error code and taking a `T` out-param, or rather than directly
 * returning the `T` and throwing an exception on error. StatusOrVal is used so
 * that callers can choose whether they want to explicitly check for errors,
 * crash the program, or throw exceptions. Since constructors do not have a
 * return value, they should be designed in such a way that they cannot fail by
 * moving the object's complex initialization logic into a separate factory
 * function that itself can return a `StatusOrVal<T>`. For example:
 *
 * @code
 * class Bar {
 *  public:
 *   Bar(Arg arg);
 *   ...
 * };
 * StatusOrVal<Bar> MakeBar() {
 *   ... complicated logic that might fail
 *   return Bar(std::move(arg));
 * }
 * @endcode
 *
 * `StatusOrVal<T>` supports equality comparisons if the underlying type `T` does.
 *
 * @tparam T the type of the value.
 */
template <typename T>
class StatusOrVal final
{
public:
    /**
     * A `value_type` member for use in generic programming.
     *
     * This is analogous to that of `std::optional::value_type`.
     */
    using value_type = T;

    /**
     * Initializes with an error status (UNKNOWN).
     */
    StatusOrVal() : StatusOrVal(Status(StatusCode::Unknown, "default")) {}

    /**
     * Creates a new `StatusOrVal<T>` holding the error condition @p rhs.
     *
     * @par Post-conditions
     * `ok() == false` and `status() == rhs`.
     *
     * @param rhs the status to initialize the object.
     * @throws std::invalid_argument if `rhs.Ok()`.
     */
    StatusOrVal(Status rhs) : m_status(std::move(rhs))
    {
        if (m_status.Ok())
        {
            throw std::invalid_argument(__func__);
        }
    }

    /**
     * Assigns the given non-OK Status to this `StatusOrVal<T>`.
     *
     * @throws std::invalid_argument if `status.ok()`. If exceptions are disabled
     *     the program terminates via `google::cloud::Terminate()`
     */
    StatusOrVal& operator=(Status status) { return *this = StatusOrVal(std::move(status)); }

    StatusOrVal(StatusOrVal&& rhs) noexcept(noexcept(T(std::move(*rhs)))) : m_status(std::move(rhs.m_status))
    {
        if (m_status.Ok())
        {
            m_value = std::move(rhs.m_value);
        }
    }

    StatusOrVal& operator=(StatusOrVal&& rhs) noexcept(noexcept(T(std::move(*rhs)))) = default;
    StatusOrVal(StatusOrVal const& rhs) = default;
    StatusOrVal& operator=(StatusOrVal const& rhs) = default;

    // Assign a `T` (or anything convertible to `T`) into the `StatusOrVal`.
    // Disable this assignment if U==StatusOrVal<T>. Well, really if U is a
    // cv-qualified version of StatusOrVal<T>, so we need to apply std::decay<> to
    // it first.
    template <typename U = T>
    typename std::enable_if<!std::is_same<StatusOrVal, typename std::decay<U>::type>::value, StatusOrVal>::type&
    operator=(U&& rhs)
    {
        m_value = std::forward<U>(rhs);
        m_status = Status();
        return *this;
    }

    // Creates a new `StatusOrVal<T>` holding the value @p rhs.
    //
    // @par Post-conditions
    // `ok() == true` and `value() == rhs`.
    //
    // @param rhs the value used to initialize the object.
    //
    // @throws only if `T`'s move constructor throws.
    //
    StatusOrVal(T&& rhs) : m_status(), m_value(std::move(rhs)) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    StatusOrVal(T const& rhs) : m_status(), m_value(rhs) {}

    bool Ok() const { return m_status.Ok(); }
    explicit operator bool() const { return m_status.Ok(); }

    //@{
    /**
     * @name Deference operators.
     *
     * @warning Using these operators when `ok() == false` results in undefined
     *     behavior.
     *
     * @return All these return a (properly ref and const-qualified) reference to
     *     the underlying value.
     */
    T& operator*() & { return m_value.value(); }

    T const& operator*() const& { return m_value.value(); }

    T&& operator*() && { return std::move(m_value).value(); }

    T const&& operator*() const&& { return std::move(m_value).value(); }
    //@}

    //@{
    /**
     * @name Member access operators.
     *
     * @warning Using these operators when `ok() == false` results in undefined
     *     behavior.
     *
     * @return All these return a (properly ref and const-qualified) pointer to
     *     the underlying value.
     */
    T* operator->() & { return &m_value.value(); }

    T const* operator->() const& { return &m_value.value(); }
    //@}

    //@{
    /**
     * @name Value accessors.
     *
     * @return All these member functions return a (properly ref and
     *     const-qualified) reference to the underlying value.
     *
     * @throws `RuntimeStatusError` with the contents of `status()` if the object
     *   does not contain a value, i.e., if `ok() == false`.
     */
    T& Value() &
    {
        CheckHasValue();
        return **this;
    }

    T const& Value() const&
    {
        CheckHasValue();
        return **this;
    }

    T&& Value() &&
    {
        CheckHasValue();
        return std::move(**this);
    }

    T const&& Value() const&&
    {
        CheckHasValue();
        return std::move(**this);
    }
    //@}

    //@{
    /**
     * @name Status accessors.
     *
     * @return All these member functions return the (properly ref and
     *     const-qualified) status. If the object contains a value then
     *     `status().ok() == true`.
     */
    Status& GetStatus() & { return m_status; }
    Status const& GetStatus() const& { return m_status; }
    Status&& GetStatus() && { return std::move(m_status); }
    Status const&& GetStatus() const&& { return std::move(m_status); }
    //@}

private:
    void CheckHasValue() const&
    {
        if (!Ok() || !m_value.has_value())
        {
            throw RuntimeStatusError(m_status);
        }
    }

    // When possible, do not copy the status.
    void CheckHasValue() &&
    {
        if (!Ok() || !m_value.has_value())
        {
            throw RuntimeStatusError(m_status);
        }
    }

    Status m_status;
    std::optional<T> m_value;
};

// Returns true IFF both `StatusOrVal<T>` objects hold an equal `Status` or an
// equal instance  of `T`. This function requires that `T` supports equality.
template <typename T>
bool operator==(StatusOrVal<T> const& a, StatusOrVal<T> const& b)
{
    if (!a || !b)
        return a.GetStatus() == b.GetStatus();
    return *a == *b;
}

// Returns true of `a` and `b` are not equal. See `operator==` docs above for
// the definition of equal.
template <typename T>
bool operator!=(StatusOrVal<T> const& a, StatusOrVal<T> const& b)
{
    return !(a == b);
}

template <typename T>
StatusOrVal<T> MakeStatusOrVal(T rhs)
{
    return StatusOrVal<T>(std::move(rhs));
}

}  // namespace csa
