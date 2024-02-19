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

#include "job_graph.h"

#include "containers.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// JobGraph
// --------------------------------------------------------------------------------------------------------------------

JobGraph::JobGraph()
{
    reset();
}

bool JobGraph::isEmpty() const
{
    return mJobs.empty();
}

void JobGraph::reset()
{
    mJobs.clear();
    mJobConsumerIndex = -1;
}

void JobGraph::addJob(JobCallback callback)
{
    mJobs.push_back(Job(callback));
}

// This function could possibly return different codes depending on
// whether simulation has completed or not.
bool JobGraph::processNextJob(int threadId,
                              std::atomic<bool>& cancel)
{
    auto jobQueueSize = static_cast<int>(mJobs.size());

    // Possibly nothing added.
    if (jobQueueSize == 0)
        return false;

    // Possibly return processing complete.
    if (mJobConsumerIndex >= jobQueueSize - 1)
        return false;

    // Increment is done after size check to avoid any index overrun due to
    // integer size limits.
    auto consumerIndex = ++mJobConsumerIndex;
    if (consumerIndex < jobQueueSize)
    {
        mJobs[consumerIndex].process(threadId, cancel);
    }

    // Possibly return continue processing.
    return true;
}

}
