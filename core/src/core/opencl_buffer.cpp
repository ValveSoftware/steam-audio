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

#if defined(IPL_USES_OPENCL)

#include "opencl_buffer.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLBuffer
// --------------------------------------------------------------------------------------------------------------------

OpenCLBuffer::OpenCLBuffer(const OpenCLDevice& openCL, size_t size)
    : mSize(size)
{
    auto status = CL_SUCCESS;
    mBuffer = clCreateBuffer(openCL.context(), CL_MEM_READ_WRITE, size, nullptr, &status);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);
}

OpenCLBuffer::~OpenCLBuffer()
{
    clReleaseMemObject(mBuffer);
}

}

#endif