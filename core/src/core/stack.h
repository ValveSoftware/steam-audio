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

#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Stack<T, N>
// --------------------------------------------------------------------------------------------------------------------

// A fixed-size stack.
template <typename T, int N>
class Stack
{
public:
    Stack()
        : mTop(0)
    {}

    // Checks whether the stack is empty.
    bool isEmpty() const
    {
        return (mTop == 0);
    }

    // Pushes a new element onto the stack.
    void push(T const& element)
    {
        mElements[mTop++] = element;
    }

    // Pops the topmost element off the stack, and returns it.
    T pop()
    {
        return mElements[--mTop];
    }

private:
    T mElements[N];   // A fixed-size array for storing elements.
    int mTop;           // Array index at which the next element will be pushed.
};

}
