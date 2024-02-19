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

#include "sampling.h"

#include <fstream>

#include "math_functions.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RandomNumberGenerator
// --------------------------------------------------------------------------------------------------------------------

RandomNumberGenerator::RandomNumberGenerator()
    : mGenerator(static_cast<int>(time(NULL)))
    , mUniformDistribution(0, std::numeric_limits<int>::max())
    , mUniformDistributionNormalized(0.0f, 1.0f)
{}

int RandomNumberGenerator::uniformRandom()
{
    return mUniformDistribution(mGenerator);
}

float RandomNumberGenerator::uniformRandomNormalized()
{
    return mUniformDistributionNormalized(mGenerator);
}


// --------------------------------------------------------------------------------------------------------------------
// Sampling
// --------------------------------------------------------------------------------------------------------------------

namespace Sampling
{
    static float hammersley(int i);

    static float radicalInverse(int p,
                                int i);
}

void Sampling::generateSphereSamples(int numSamples,
                                     Vector3f* samples)
{
    for (auto i = 0; i < numSamples; ++i)
    {
        auto u = (i + 0.5f) / numSamples;
        auto v = hammersley(i);

        auto azimuth = 2.0f * Math::kPi * u;
        auto height = 2.0f * v - 1.0f;

        auto horizontal = sqrtf(1.0f - height*height);

        samples[i] = Vector3f(horizontal * cosf(azimuth), height, horizontal * -sinf(azimuth));
    }
}

void Sampling::generateHemisphereSamples(int numSamples,
                                         Vector3f* samples)
{
    for (auto i = 0; i < numSamples; ++i)
    {
        auto u = (i + 0.5f) / numSamples;
        auto v = hammersley(i);

        auto r = sqrtf(u);
        auto theta = 2.0f * Math::kPi * v;

        auto x = r * cosf(theta);
        auto y = r * sinf(theta);
        auto z = -sqrtf(std::max(0.0f, 1.0f - x * x - y * y));

        samples[i] = Vector3f(x, y, z);
    }
}

Vector3f Sampling::transformHemisphereSample(const Vector3f& sample,
                                             const Vector3f& normal)
{
    CoordinateSpace3f surface(normal, Vector3f::kZero);
    return Vector3f::unitVector(surface.transformDirectionFromLocalToWorld(sample));
}

// https://stackoverflow.com/questions/5408276/sampling-uniformly-distributed-random-points-inside-a-spherical-volume
void Sampling::generateSphereVolumeSamples(int numSamples,
                                           Vector3f* samples)
{
    for (auto i = 0; i < numSamples; ++i)
    {
        auto uPhi = radicalInverse(2, i);
        auto uTheta = radicalInverse(3, i);
        auto uR = radicalInverse(5, i);

        auto phi = 2.0f * Math::kPi * uPhi;
        auto theta = acosf(2.0f * uTheta - 1.0f);
        auto r = cbrtf(uR);

        auto x = r * sinf(theta) * cosf(phi);
        auto y = r * sinf(theta) * sinf(phi);
        auto z = r * cosf(theta);

        samples[i] = Vector3f(x, y, z);
    }
}

Vector3f Sampling::transformSphereVolumeSample(const Vector3f& sample,
                                               const Sphere& sphere)
{
    return (sphere.center + (sample * sphere.radius));
}

float Sampling::hammersley(int i)
{
    auto value = 0.0f;
    auto p = 0.5f;

    for (auto k = i; k > 0; p *= 0.5f, k >>= 1)
    {
        if (k & 1)
        {
            value += p;
        }
    }

    return value;
}

// http://www.pbr-book.org/3ed-2018/Sampling_and_Reconstruction/The_Halton_Sampler.html#RadicalInverseSpecialized
float Sampling::radicalInverse(int p,
                               int i)
{
    auto inv = 1.0f / p;
    auto reversed = 0;
    auto invN = 1.0f;

    while (i)
    {
        auto next = i / p;
        auto digit = i - (next * p);
        reversed = reversed * p + digit;
        invN *= inv;
        i = next;
    }

    return std::min(reversed * invN, 1.0f);
}

}
