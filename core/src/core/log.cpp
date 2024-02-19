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

#include "log.h"

#include "types.h"

#include <cstdarg>
#include <cstring>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#endif

#if defined(IPL_OS_ANDROID)
#include <android/log.h>
#endif

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Log
// --------------------------------------------------------------------------------------------------------------------

void Log::init(LogCallback callback)
{
    mCallback = callback;
}

void Log::message(MessageSeverity severity,
                  const char *formatString,
                  ...)
{
    static const size_t kMessageBufferLength = 16384;
    static const size_t kFormattedMessageBufferLength = 17408;  // kMessageBufferLength + 1024 chars for meta data

    static char kMessageBuffer[kMessageBufferLength];
    static char kFormattedMessageBuffer[kFormattedMessageBufferLength];

    // Populate a char buffer with the message parameter.
    va_list ap;
    memset(kMessageBuffer, 0, kMessageBufferLength);
    va_start(ap, formatString);
    vsprintf(kMessageBuffer, formatString, ap);
    va_end(ap);

    const char* const kMessageSeverityNames[] =
    {
        "",
        "Warning: ",
        "ERROR: ",
        "(debug) "
    };

    // Populate a char buffer with a formatted message string.
    memset(kFormattedMessageBuffer, 0, kFormattedMessageBufferLength);
    sprintf(kFormattedMessageBuffer, "%s%s\n", kMessageSeverityNames[static_cast<size_t>(severity)], kMessageBuffer);

    if (mCallback)
    {
        mCallback(severity, kFormattedMessageBuffer);
    }
    else
    {
#if defined(IPL_OS_WINDOWS)
        OutputDebugStringA(kFormattedMessageBuffer);
#endif
#if defined(IPL_OS_ANDROID)
        __android_log_print(ANDROID_LOG_INFO, "Phonon", "%s", kFormattedMessageBuffer);
#endif
        printf("%s", kFormattedMessageBuffer);
    }
}

}
