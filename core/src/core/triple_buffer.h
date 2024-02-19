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

#include <atomic>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// TripleBuffer<T>
// --------------------------------------------------------------------------------------------------------------------

template <typename T>
class TripleBuffer
{
public:
    unique_ptr<T>       writeBuffer;
    unique_ptr<T>       shareBuffer;
    unique_ptr<T>       readBuffer;

    TripleBuffer()
        : mNewDataWritten(false)
    {}

    template <typename... Args>
    void initBuffers(Args&&... args)
    {
        writeBuffer = ipl::make_unique<T>(std::forward<Args>(args)...);
        shareBuffer = ipl::make_unique<T>(std::forward<Args>(args)...);
        readBuffer  = ipl::make_unique<T>(std::forward<Args>(args)...);
    }

    void commitWriteBuffer()
    {
        if (!mNewDataWritten)
        {
            shareBuffer.swap(writeBuffer);
            mNewDataWritten = true;
        }
    }

    bool updateReadBuffer()
    {
        auto newDataRead = false;

        if (mNewDataWritten)
        {
            readBuffer.swap(shareBuffer);
            newDataRead = true;
            mNewDataWritten = false;
        }

        return newDataRead;
    }

private:
    std::atomic<bool>   mNewDataWritten;
};

}
