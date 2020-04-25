// Copyright 2019 Andrew Karasyov
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

#include <iostream>

namespace csa {

    /**
     * This structure represents user information.
     */
    struct UserInfo
    {
        /** 
         * User email address.
         * This may not be present in certain contexts
         * if the user has not made their email address visible to the requester.
         */
        std::string m_email;

        /** A plain text displayable name for this user if available. */
        std::string m_name;
    };

    inline std::ostream& operator<<(std::ostream& os, UserInfo const& ui)
    {
        os << "user_info={"
            << "email=" << ui.m_email
            << ", name=" << ui.m_name;
        return os << "}";
    }

} // namespace csa
