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

/**
 * @file
 *
 * Implement types useful to test the behavior of template classes.
 *
 * Just like a function should be tested with different inputs, template classes
 * should be tested with types that have different characteristics. For example,
 * it is often interesting to test a template with a type that lacks a default
 * constructor. This file implements some types that are useful for
 * testing template classes.
 */

#include <string>
#include <utility>
#include <vector>

namespace csa {
namespace testing {
namespace util {

// A class without a default constructor.
class NoDefaultConstructor
{
public:
    NoDefaultConstructor() = delete;
    explicit NoDefaultConstructor(std::string x) : m_str(std::move(x)) {}

    std::string Str() const { return m_str; }

private:
    friend bool operator==(NoDefaultConstructor const& lhs,
        NoDefaultConstructor const& rhs);

    std::string m_str;
};

inline bool operator==(NoDefaultConstructor const& lhs,
                       NoDefaultConstructor const& rhs)
{
    return lhs.m_str == rhs.m_str;
}

inline bool operator!=(NoDefaultConstructor const& lhs,
                       NoDefaultConstructor const& rhs)
{
    return std::rel_ops::operator!=(lhs, rhs);
}

/**
 * A class that counts calls to its constructors.
 */
class Observable
{
public:
    static int default_constructor;
    static int value_constructor;
    static int copy_constructor;
    static int move_constructor;
    static int copy_assignment;
    static int move_assignment;
    static int destructor;

    static void reset_counters() {
        default_constructor = 0;
        value_constructor = 0;
        copy_constructor = 0;
        move_constructor = 0;
        copy_assignment = 0;
        move_assignment = 0;
        destructor = 0;
    }

    Observable() { ++default_constructor; }
    explicit Observable(std::string s) : m_str(std::move(s)) {
        ++value_constructor;
    }
    Observable(Observable const& rhs) : m_str(rhs.m_str) { ++copy_constructor; }
    Observable(Observable&& rhs) noexcept : m_str(std::move(rhs.m_str)) {
        rhs.m_str = "moved-out";
        ++move_constructor;
    }

    Observable& operator=(Observable const& rhs) {
        m_str = rhs.m_str;
        ++copy_assignment;
        return *this;
    }
    Observable& operator=(Observable&& rhs) noexcept {
        m_str = std::move(rhs.m_str);
        rhs.m_str = "moved-out";
        ++move_assignment;
        return *this;
    }
    ~Observable() { ++destructor; }

    std::string const& Str() const { return m_str; }

private:
    std::string m_str;
};

}  // namespace testing
}  // namespace util
}  // namespace csa
