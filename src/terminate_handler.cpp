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

#include "cloudstorageapi/terminate_handler.h"
#include <iostream>
#include <mutex>

namespace csa {
namespace {

class TerminateFunction {
public:
    explicit TerminateFunction(TerminateHandler handler)
        : m_handler(std::move(handler))
    {}

    TerminateHandler Get()
    {
        std::lock_guard<std::mutex> l(m_mu);
        return m_handler;
    }

    TerminateHandler Set(TerminateHandler handler) {
        std::lock_guard<std::mutex> l(m_mu);
        handler.swap(m_handler);
        return handler;
    }

private:
    std::mutex m_mu;
    TerminateHandler m_handler;
};

TerminateFunction& GetTerminateHolder()
{
    static TerminateFunction f([](const char* msg) {
        std::cerr << "Aborting: " << msg << "\n";
        std::abort();
        });
    return f;
}

}  // anonymous namespace

TerminateHandler SetTerminateHandler(TerminateHandler handler)
{
    return GetTerminateHolder().Set(std::move(handler));
}

TerminateHandler GetTerminateHandler()
{
    return GetTerminateHolder().Get();
}

[[noreturn]] void Terminate(const char* msg)
{
  GetTerminateHolder().Get()(msg);
  std::cerr << "Aborting because the installed terminate handler returned. "
               "Error details: "
            << msg << "\n";
  std::abort();
}

}  // namespace csa
