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

#include <assert.h>

#include <exception>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Exception
// --------------------------------------------------------------------------------------------------------------------

// Error codes that can be returned by the C API.
enum class Status
{
    Success,
    Failure,
    OutOfMemory,
    Initialization
};

// Base exception class. Each exception contains an equivalent status code, as well as any class-specific information
// that may be suitable.
class Exception : public std::exception
{
public:
    // Creates an exception class with a given status code.
    Exception(Status status)
        : mStatus(status)
    {}

    Status status() const
    {
        return mStatus;
    }

private:
    Status mStatus; // The equivalent status code.
};

}
