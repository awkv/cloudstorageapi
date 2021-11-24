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

#include "cloudstorageapi/internal/complex_option.h"
#include "cloudstorageapi/well_known_headers.h"
#include "cloudstorageapi/well_known_parameters.h"
#include <iostream>
#include <utility>

namespace csa {
namespace internal {

// Forward declare the template so we can specialize it first. Defining the
// specialization first, which is the base class, should be more readable.
template <typename Derived, typename Option, typename... Options>
class GenericRequestBase;

/**
 * Refactors common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `GenericRequest`, it is the
 * base class for a hierarchy of `GenericRequestBase<Parameters...>` when the
 * list has a single parameter.
 *
 * @tparam Option the type of the option contained in this object.
 */
template <typename Derived, typename Option>
class GenericRequestBase<Derived, Option>
{
public:
    Derived& SetOption(Option p)
    {
        m_option = std::move(p);
        return *static_cast<Derived*>(this);
    }

    template <typename HttpRequest>
    void AddOptionsToHttpRequest(HttpRequest& request) const
    {
        request.AddOption(m_option);
    }

    void DumpOptions(std::ostream& os, char const* sep) const
    {
        if (m_option.HasValue())
        {
            os << sep << m_option;
        }
    }

    template <typename O>
    bool HasOption() const
    {
        if (std::is_same<O, Option>::value)
        {
            return m_option.HasValue();
        }
        return false;
    }

    template <typename O, typename std::enable_if<std::is_same<O, Option>::value, int>::type = 0>
    O GetOption() const
    {
        return m_option;
    }

private:
    Option m_option;
};

/**
 * Refactors common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `GenericRequest` see below for
 * more details.
 */
template <typename Derived, typename Option, typename... Options>
class GenericRequestBase : public GenericRequestBase<Derived, Options...>
{
public:
    using GenericRequestBase<Derived, Options...>::SetOption;

    Derived& SetOption(Option p)
    {
        m_option = std::move(p);
        return *static_cast<Derived*>(this);
    }

    template <typename HttpRequest>
    void AddOptionsToHttpRequest(HttpRequest& request) const
    {
        request.AddOption(m_option);
        GenericRequestBase<Derived, Options...>::AddOptionsToHttpRequest(request);
    }

    void DumpOptions(std::ostream& os, char const* sep) const
    {
        if (m_option.HasValue())
        {
            os << sep << m_option;
            GenericRequestBase<Derived, Options...>::DumpOptions(os, ", ");
        }
        else
        {
            GenericRequestBase<Derived, Options...>::DumpOptions(os, sep);
        }
    }

    template <typename O>
    bool HasOption() const
    {
        if (std::is_same<O, Option>::value)
        {
            return m_option.HasValue();
        }
        return GenericRequestBase<Derived, Options...>::template HasOption<O>();
    }

    template <typename O, typename std::enable_if<std::is_same<O, Option>::value, int>::type = 0>
    O GetOption() const
    {
        return m_option;
    }

    template <typename O, typename std::enable_if<!std::is_same<O, Option>::value, int>::type = 0>
    O GetOption() const
    {
        return GenericRequestBase<Derived, Options...>::template GetOption<O>();
    }

private:
    Option m_option;
};

/**
 * Refactors common functions to operate on optional request parameters.
 *
 * Each operation in the client library has its own `*Request` class, and each
 * of these classes needs to define functions to change the optional parameters
 * of the request. This class implements these functions in a single place,
 * saving us a lot typing.
 *
 * To implement `FooRequest` you do three things:
 *
 * 1) Make this class a (private) base class of `FooRequest`, with the list of
 *    optional parameters it will support:
 * @code
 * class FooRequest : private internal::RequestOptions<UserProject, P1, P2>
 * @endcode
 *
 * 2) Define a generic function to set a parameter:
 * @code
 * class FooRequest // some things ommitted
 * {
 *   template <typename Parameter>
 *   FooRequest& set_parameter(Parameter&& p) {
 *     RequestOptions::set_parameter(p);
 *     return *this;
 *   }
 * @endcode
 *
 * Note that this is a single function, but only the parameter types declared
 * in the base class are accepted.
 *
 * 3) Define a generic function to set multiple parameters:
 * @code
 * class FooRequest // some things ommitted
 * {
 *   template <typename... Options>
 *   FooRequest& SetMultipleOptions(Options&&... p) {
 *     RequestOptions::SetMultipleOptions(std::forward<Parameter>(p)...);
 *     return *this;
 *   }
 * @endcode
 *
 * @tparam Options the list of options that the Request class will support.
 */
template <typename Derived, typename... Options>
class GenericRequest
    : public GenericRequestBase<Derived, CustomHeader, Fields, IfMatchEtag, IfNoneMatchEtag, Options...>
{
public:
    using Super = GenericRequestBase<Derived, CustomHeader, Fields, IfMatchEtag, IfNoneMatchEtag, Options...>;

    template <typename H, typename... T>
    Derived& SetMultipleOptions(H&& h, T&&... tail)
    {
        Super::SetOption(std::forward<H>(h));
        return SetMultipleOptions(std::forward<T>(tail)...);
    }

    Derived& SetMultipleOptions() { return *static_cast<Derived*>(this); }

    template <typename Option>
    bool HasOption() const
    {
        return Super::template HasOption<Option>();
    }

    template <typename Option>
    Option GetOption() const
    {
        return Super::template GetOption<Option>();
    }
};

}  // namespace internal
}  // namespace csa
