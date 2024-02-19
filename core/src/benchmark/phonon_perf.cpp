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

#include <fstream>
#include <stdarg.h>
#include <stdio.h>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#endif

#include <context.h>
#include <containers.h>
#include <vector.h>
using namespace ipl;

#define IPL_BUILDING_MAIN
#include "phonon_perf.h"

// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

FILE* gOutFile = NULL;

void PrintOutput(const char *format, ...)
{
    va_list stdout_args, log_args;
    va_start(stdout_args, format);
    va_copy(log_args, stdout_args);
    vprintf(format, stdout_args);
    va_end(stdout_args);

    if (gOutFile)
    {
        vfprintf(gOutFile, format, log_args);
        va_end(log_args);
    }
}

void FillRandomData(float* buffer, size_t size)
{
    for (auto i = 0U; i < size; ++i)
        buffer[i] = (rand() % 10001) / 10000.0f;
}

void LoadObj(const std::string& fileName, std::vector<float>& vertices,
    std::vector<int32_t>& triangleIndices, std::vector<int>& materialIndices)
{
    std::ifstream file(fileName.c_str());
    if (!file.good())
    {
        printf("Unable to load .obj file: %s\n", fileName.c_str());
        return;
    }

    vertices.clear();
    triangleIndices.clear();

    while (true)
    {
        std::string command;
        file >> command;
        if (file.eof())
            break;

        if (command == "v")
        {
            float x, y, z;
            file >> x >> y >> z;
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
        else if (command == "f")
        {
            std::string s[3];
            file >> s[0] >> s[1] >> s[2];

            for (auto i = 0; i < 3; ++i)
                triangleIndices.push_back(atoi(s[i].substr(0, s[i].find_first_of("/")).c_str()) - 1);
        }
    }

    for (auto i = 0U; i < triangleIndices.size() / 3; ++i)
        materialIndices.push_back(0);
}

void SetCoreAffinityForBenchmarking()
{
#if defined(IPL_OS_WINDOWS)
    HANDLE process = GetCurrentProcess();

    DWORD_PTR processAffinityMask;
    DWORD_PTR systemAffinityMask;

    if (!GetProcessAffinityMask(process, &processAffinityMask, &systemAffinityMask))
        return;

    DWORD_PTR mask = 0x1;
    for (int bit = 0, currentCore = 1; bit < static_cast<int>(std::thread::hardware_concurrency()); bit++)
    {
        if (mask & processAffinityMask)
        {
            if (currentCore % 2 == 0)
            {
                processAffinityMask &= ~mask;
            }
            currentCore++;
        }
        mask = mask << 1;
    }

    SetProcessAffinityMask(process, processAffinityMask);
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// main
// --------------------------------------------------------------------------------------------------------------------

void PrintOptions()
{
    auto names = getFunctionRegistry().getFunctionNames();
    printf("USAGE: phonon_perf all|<name>\n");
    printf("where <name> is one of:\n");
    for (const auto& name : names)
    {
        printf("\t%s\n", name.c_str());
    }
}

void RunBenchmarks(const std::string& benchmarkName)
{
    if (benchmarkName == "all")
    {
        auto names = getFunctionRegistry().getFunctionNames();
        for (const auto& name : names)
        {
            getFunctionRegistry().runFunction(name);
        }
    }
    else
    {
        if (!getFunctionRegistry().runFunction(benchmarkName))
        {
            PrintOptions();
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintOptions();
        return -1;
    }

    if (argc == 3)
    {
        gOutFile = fopen(argv[2], "w");
    }

    RunBenchmarks(std::string(argv[1]));

    if (gOutFile)
    {
        fclose(gOutFile);
    }

    return 0;
}
