// Copyright 2021 Andrew Karasyov
//
// Copyright 2021 Google LLC
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
#include <iosfwd>

namespace csa {

enum class AutoFinalizeConfig
{
    Disabled,
    Enabled,
};

/**
 * Control whether upload streams auto-finalize on destruction.
 *
 * Some applications need to disable auto-finalization of resumable uploads.
 * This option (or rather the `AutoFinalizeDisabled()` helper) configures
 * whether FileWriteStream objects finalize an upload when the object is
 * destructed.
 */
struct AutoFinalize : public internal::ComplexOption<AutoFinalize, AutoFinalizeConfig>
{
    explicit AutoFinalize(AutoFinalizeConfig value) : ComplexOption(value) {}
    AutoFinalize() : ComplexOption(AutoFinalizeConfig::Enabled) {}

    static char const* name() { return "auto-finalize"; }
};

/// Configure a stream to automatically finalize an upload on destruction.
inline AutoFinalize AutoFinalizeEnabled() { return AutoFinalize(AutoFinalizeConfig::Enabled); }

/// Configure a stream to leave uploads pending (not finalized) on destruction.
inline AutoFinalize AutoFinalizeDisabled() { return AutoFinalize(AutoFinalizeConfig::Disabled); }

std::ostream& operator<<(std::ostream& os, AutoFinalize const& rhs);

}  // namespace csa
