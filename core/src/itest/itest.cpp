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

#define IPL_BUILDING_MAIN
#include "itest.h"

void usage()
{
    auto names = getFunctionRegistry().getFunctionNames();
    printf("USAGE: phonon_itest <name>\n");
    printf("where <name> is one of:\n");
    for (const auto& name : names)
    {
        printf("\t%s\n", name.c_str());
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        usage();
        return 0;
    }

    if (!getFunctionRegistry().runFunction(argv[1]))
    {
        usage();
    }

    return 0;
}
