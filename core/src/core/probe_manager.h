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

#include "probe_batch.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// ProbeManager
// ---------------------------------------------------------------------------------------------------------------------

class ProbeManager
{
public:
    int numProbeBatches() const
    {
        return static_cast<int>(mProbeBatches[0].size());
    }

    list<shared_ptr<ProbeBatch>>& probeBatches()
    {
        return mProbeBatches[0];
    }

    const list<shared_ptr<ProbeBatch>>& probeBatches() const
    {
        return mProbeBatches[0];
    }

    void addProbeBatch(shared_ptr<ProbeBatch> probeBatch);

    void removeProbeBatch(shared_ptr<ProbeBatch> probeBatch);

    void commit();

    void getInfluencingProbes(const Vector3f& point,
                              ProbeNeighborhood& neighborhood);

private:
    list<shared_ptr<ProbeBatch>> mProbeBatches[2];
};

}
