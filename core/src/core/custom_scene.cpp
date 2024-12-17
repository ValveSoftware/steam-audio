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

#include "custom_scene.h"
#include "profiler.h"

namespace ipl {

CustomScene::CustomScene(ClosestHitCallback closestHitCallback,
                         AnyHitCallback anyHitCallback,
                         BatchedClosestHitCallback batchedClosestHitCallback,
                         BatchedAnyHitCallback batchedAnyHitCallback,
                         void* userData)
    : mClosestHitCallback(closestHitCallback)
    , mAnyHitCallback(anyHitCallback)
    , mBatchedClosestHitCallback(batchedClosestHitCallback)
    , mBatchedAnyHitCallback(batchedAnyHitCallback)
    , mUserData(userData)
{}

Hit CustomScene::closestHit(const Ray& ray,
                            float minDistance,
                            float maxDistance) const
{
    PROFILE_FUNCTION();

    Hit hit;
    mClosestHitCallback(&ray, minDistance, maxDistance, &hit, mUserData);
    return hit;
}

bool CustomScene::anyHit(const Ray& ray,
                         float minDistance,
                         float maxDistance) const
{
    PROFILE_FUNCTION();

    uint8_t occluded = 0;
    mAnyHitCallback(&ray, minDistance, maxDistance, &occluded, mUserData);
    return (occluded != 0);
}

void CustomScene::closestHits(int numRays,
                              const Ray* rays,
                              const float* minDistances,
                              const float* maxDistances,
                              Hit* hits) const
{
    PROFILE_FUNCTION();

    if (mBatchedClosestHitCallback)
    {
        mBatchedClosestHitCallback(numRays, rays, minDistances, maxDistances, hits, mUserData);
    }
    else
    {
        for (auto i = 0; i < numRays; ++i)
        {
            hits[i] = closestHit(rays[i], minDistances[i], maxDistances[i]);
        }
    }
}

void CustomScene::anyHits(int numRays,
                          const Ray* rays,
                          const float* minDistances,
                          const float* maxDistances,
                          bool* occluded) const
{
    PROFILE_FUNCTION();

    if (mBatchedAnyHitCallback)
    {
        mBatchedAnyHitCallback(numRays, rays, minDistances, maxDistances, reinterpret_cast<uint8_t*>(occluded), mUserData);
    }
    else
    {
        for (auto i = 0; i < numRays; ++i)
        {
            occluded[i] = (maxDistances[i] >= 0.0f) ? anyHit(rays[i], minDistances[i], maxDistances[i]) : true;
        }
    }
}

}
