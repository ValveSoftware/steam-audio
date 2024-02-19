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

#include "array.h"
#include "containers.h"
#include "job.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// JobGraph
// --------------------------------------------------------------------------------------------------------------------

// Describes a job graph. A job graph is initialized with a fixed number of jobs. All the jobs in a job graph must be
// inserted before calls to processing next jobs is made. Effectively, there is a single producer (which produces at
// the beginning) and multiple consumers.
class JobGraph
{
public:
    JobGraph();

    bool isEmpty() const;

    void reset();

    void addJob(JobCallback callback);

    bool processNextJob(int threadId,
                        std::atomic<bool>& cancel);

private:
    vector<Job> mJobs;
    std::atomic<int> mJobConsumerIndex;
};

}
