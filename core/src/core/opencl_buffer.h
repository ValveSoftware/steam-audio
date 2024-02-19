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
// OpenCLBuffer
// --------------------------------------------------------------------------------------------------------------------

class OpenCLBuffer
{
public:
    OpenCLBuffer(const OpenCLDevice& openCL,
                 size_t size);

    ~OpenCLBuffer();

    cl_mem& buffer()
    {
        return mBuffer;
    }

    const cl_mem& buffer() const
    {
        return mBuffer;
    }

    size_t size() const
    {
        return mSize;
    }

private:
    cl_mem mBuffer;
    size_t mSize;
};

}

#endif