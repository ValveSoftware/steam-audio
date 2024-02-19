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

#include "types.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Log
// --------------------------------------------------------------------------------------------------------------------

// Severity of a log message.
enum class MessageSeverity
{
    Info,
    Warning,
    Error,
    Debug
};

typedef void (IPL_CALLBACK *LogCallback)(MessageSeverity severity, const char* message);

class Log
{
public:
    void init(LogCallback callback);

    void message(MessageSeverity severity,
                 const char* formatString,
                 ...);

private:
    LogCallback mCallback;
};

extern Log& gLog();

}
