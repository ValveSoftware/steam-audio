//
// Copyright 2017-2025 Valve Corporation.
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

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <catch.hpp>

#include <job_graph.h>
#include <thread_pool.h>

using namespace ipl;

static void addJobs(JobGraph& jobGraph,
                    int numJobs,
                    std::atomic<int>& numRun)
{
    for (auto i = 0; i < numJobs; ++i)
    {
        jobGraph.addJob([&numRun](int threadId, std::atomic<bool>& cancel)
        {
            if (cancel)
                return;

            std::this_thread::sleep_for(std::chrono::microseconds(50));
            ++numRun;
        });
    }
}

// Cancels a thread pool while it is processing a job graph. Once process() returns, the job graph belongs to the
// caller again (e.g., PathBaker frees it), so no worker thread may run any of its jobs after that.
static int cancelDuringProcess(int round)
{
    std::atomic<int> numRun{0};
    std::atomic<int> numRunAfterProcess{0};

    {
        auto jobGraph = std::make_unique<JobGraph>();
        auto threadPool = std::make_unique<ThreadPool>(4);

        addJobs(*jobGraph, 4000, numRun);

        std::thread canceller([&]()
        {
            while (numRun < 16 + round)
                std::this_thread::yield();

            threadPool->cancel();
        });

        threadPool->process(*jobGraph);
        canceller.join();

        // Replace the jobs with ones that should never run. A worker that is still using the job graph will run them.
        jobGraph->reset();
        for (auto i = 0; i < 64; ++i)
        {
            jobGraph->addJob([&numRunAfterProcess](int, std::atomic<bool>&)
            {
                ++numRunAfterProcess;
            });
        }

        // Processing again on a cancelled thread pool must also return.
        JobGraph nextJobGraph;
        addJobs(nextJobGraph, 64, numRun);
        threadPool->process(nextJobGraph);

        // Joins the worker threads.
        threadPool.reset();
    }

    return numRunAfterProcess;
}

TEST_CASE("ThreadPool does not wake up threads without work when cancelled", "[ThreadPool]")
{
    // If the bug is present, process() can wait forever, so run the test on a detached thread with a timeout.
    auto result = std::make_shared<std::promise<int>>();
    auto future = result->get_future();

    std::thread([result]()
    {
        auto numRunAfterProcess = 0;
        for (auto round = 0; round < 20; ++round)
        {
            numRunAfterProcess += cancelDuringProcess(round);
        }
        result->set_value(numRunAfterProcess);
    }).detach();

    REQUIRE(future.wait_for(std::chrono::seconds(60)) == std::future_status::ready);
    REQUIRE(future.get() == 0);
}
