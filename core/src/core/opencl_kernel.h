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

#if defined(IPL_USES_OPENCL)

#include "opencl_device.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLProgram
// --------------------------------------------------------------------------------------------------------------------

class OpenCLProgram
{
public:
    OpenCLProgram(const OpenCLDevice& openCL,
                  const char* source);

    ~OpenCLProgram();

    cl_program program() const
    {
        return mProgram;
    }

private:
    cl_program mProgram;
};


// --------------------------------------------------------------------------------------------------------------------
// OpenCLKernel
// --------------------------------------------------------------------------------------------------------------------

class OpenCLKernel
{
public:
    OpenCLKernel(const OpenCLDevice& openCL,
                 const OpenCLProgram& program,
                 const char* name);

    ~OpenCLKernel();

    cl_kernel kernel() const
    {
        return mKernel;
    }

private:
    cl_kernel mKernel;
};

}

#endif