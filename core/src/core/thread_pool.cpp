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

#include "thread_pool.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ThreadPool
// --------------------------------------------------------------------------------------------------------------------

ThreadPool::ThreadPool(int numThreads)
    : mThreads(numThreads)
    , mCancel(false)
    , mReady(0)
    , mCompleted(0)
    , mQuit(false)
    , mJobGraph(nullptr)
{
    for (auto i = 0; i < numThreads; ++i)
    {
        mThreads[i] = std::move(std::thread(&ThreadPool::threadFunc, this, i));
    }
}

ThreadPool::~ThreadPool()
{
    std::unique_lock<std::mutex> lock(mMutex);
    mQuit = true;
    mReady = static_cast<int>(mThreads.size(0));
    mCondVarReady.notify_all();
    lock.unlock();
    for (auto i = 0u; i < mThreads.size(0); ++i)
    {
        mThreads[i].join();
    }
}

void ThreadPool::process(JobGraph& jobGraph)
{
    mJobGraph = &jobGraph;
    std::unique_lock<std::mutex> lock(mMutex);
    mReady = static_cast<int>(mThreads.size(0));
    mCompleted = 0;
    mCondVarReady.notify_all();
    mCondVarComplete.wait(lock, [this]() { return (mCompleted == mThreads.size(0)); });
}

void ThreadPool::cancel()
{
    mCancel = true;
}

void ThreadPool::threadFunc(int threadId)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVarReady.wait(lock, [this]() { return (mReady > 0 || mCancel || mQuit); });
        --mReady;

        if (mQuit)
        {
            mCondVarComplete.notify_one();
            break;
        }

        lock.unlock();
        while (mJobGraph->processNextJob(threadId, mCancel))
        {
            if (mCancel)
                break;
        }

        lock.lock();
        ++mCompleted;
        mCondVarComplete.notify_one();
    }
}

}
