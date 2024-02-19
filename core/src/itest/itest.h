//
// Copyright 2017-2023 Valve Corporation.
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
//

#pragma once

#include <map>
#include <string>
#include <vector>


class FunctionRegistry
{
public:
    typedef void (*Function)();

    void registerFunction(const std::string& name, Function function)
    {
        m_functions[name] = function;
    }

    std::vector<std::string> getFunctionNames() const
    {
        std::vector<std::string> names;

        for (const auto& kv : m_functions)
        {
            names.push_back(kv.first);
        }

        return names;
    }

    bool runFunction(const std::string& name)
    {
        if (m_functions.find(name) != m_functions.end())
        {
            m_functions[name]();
            return true;
        }
        else
        {
            printf("ERROR: No function defined for name: %s\n", name.c_str());
            return false;
        }
    }

private:
    std::map<std::string, Function> m_functions;
};

#if defined(IPL_BUILDING_MAIN)
FunctionRegistry& getFunctionRegistry()
{
    static FunctionRegistry functionRegistry{};
    return functionRegistry;
}
#else
FunctionRegistry& getFunctionRegistry();
#endif


class SelfRegisteringFunction
{
public:
    SelfRegisteringFunction(const std::string& name, FunctionRegistry::Function function)
    {
        getFunctionRegistry().registerFunction(name, function);
    }
};


#define ITEST(name) \
    static void itest_##name(); \
    SelfRegisteringFunction register_itest_##name(#name, itest_##name); \
    static void itest_##name()
