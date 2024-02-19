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

#define EMBREE_STATIC_LIB
#include <rtcore.isph>
#include <rtcore_ray.isph>

#define FLT_MAX                 3.402823466e+38f
#define IPL_NUM_BANDS           3
#define IPL_HIT_SURFACE_OFFSET  1e-2f
#define IPL_SPECULAR_POWER      1e2f
#define IPL_BIN_DURATION        1e-2f

#define SOURCE_RADIUS   0.1f
#define LISTENER_RADIUS 0.1f

typedef float<3> float3;
typedef float<4> float4;

// --------------------------------------------------------------------------------------------------------------------
// Vector3
// --------------------------------------------------------------------------------------------------------------------

struct Vector3
{
    float x, y, z;
};

inline float dot(float3 a,
                 float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length(float3 v)
{
    return sqrt(dot(v, v));
}

inline float3 normalized(float3 v)
{
    return rsqrt(dot(v, v)) * v;
}

inline float3 cross(float3 a,
                    float3 b)
{
    float3 c;
    c.x = a.y * b.z - a.z * b.y;
    c.y = a.z * b.x - a.x * b.z;
    c.z = a.x * b.y - a.y * b.x;
    return c;
}

inline float3 reflect(float3 incident,
                      float3 normal)
{
    return normalized(incident - (2.0f * dot(incident, normal) * normal));
}

// --------------------------------------------------------------------------------------------------------------------
// CoordinateSpace
// --------------------------------------------------------------------------------------------------------------------

struct CoordinateSpace
{
    float3 right, up, ahead;
    float3 origin;
};

inline CoordinateSpace createCoordinateSpace(float3 normal)
{
    CoordinateSpace space;
    space.ahead = normal;

    if (abs(normal.x) > abs(normal.z))
    {
        float3 right = { -normal.y, normal.x, 0.0f };
        space.right = normalized(right);
    }
    else
    {
        float3 right = { 0.0f, -normal.z, normal.y };
        space.right = normalized(right);
    }

    space.up = cross(space.right, space.ahead);

    return space;
}

inline float3 transformLocalToWorld(const CoordinateSpace space,
                                    float3 direction)
{
    return (direction.x * space.right + direction.y * space.up - direction.z * space.ahead);
}

inline float3 transformWorldToLocal(const uniform CoordinateSpace space,
                                    float3 direction)
{
    float3 transformedDirection;
    transformedDirection.x = dot(direction, space.right);
    transformedDirection.y = dot(direction, space.up);
    transformedDirection.z = -dot(direction, space.ahead);
    return transformedDirection;
}

inline float3 transformHemisphereSample(float3 direction,
                                        float3 normal)
{
    CoordinateSpace tangentSpace = createCoordinateSpace(normal);
    return normalized(transformLocalToWorld(tangentSpace, direction));
}

// --------------------------------------------------------------------------------------------------------------------
// Random Sampling
// --------------------------------------------------------------------------------------------------------------------

// The following code is from the Embree GitHub repository. It can be found at:
//      https://github.com/embree/embree/blob/v2.17.2/tutorials/common/math/random_sampler.isph

struct RandomSampler
{
    unsigned int s;
};

inline uniform unsigned int MurmurHash3_mix(uniform unsigned int hash,
                                            uniform unsigned int k)
{
    const uniform unsigned int c1 = 0xcc9e2d51;
    const uniform unsigned int c2 = 0x1b873593;
    const uniform unsigned int r1 = 15;
    const uniform unsigned int r2 = 13;
    const uniform unsigned int m = 5;
    const uniform unsigned int n = 0xe6546b64;

    k *= c1;
    k = (k << r1) | (k >> (32 - r1));
    k *= c2;

    hash ^= k;
    hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;

    return hash;
}

inline uniform unsigned int MurmurHash3_finalize(uniform unsigned int hash)
{
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}

inline uniform unsigned int LCG_next(uniform unsigned int value)
{
    const uniform unsigned int m = 1664525;
    const uniform unsigned int n = 1013904223;

    return value * m + n;
}

inline void RandomSampler_init(uniform RandomSampler& self,
                               uniform int id)
{
    uniform unsigned int hash = 0;
    hash = MurmurHash3_mix(hash, id);
    hash = MurmurHash3_finalize(hash);

    self.s = hash;
}

inline void RandomSampler_init(uniform RandomSampler& self,
                               uniform int pixelId,
                               uniform int sampleId)
{
    uniform unsigned int hash = 0;
    hash = MurmurHash3_mix(hash, pixelId);
    hash = MurmurHash3_mix(hash, sampleId);
    hash = MurmurHash3_finalize(hash);

    self.s = hash;
}

inline void RandomSampler_init(uniform RandomSampler& self,
                               uniform int x,
                               uniform int y,
                               uniform int sampleId)
{
    RandomSampler_init(self, x | (y << 16), sampleId);
}

inline uniform int RandomSampler_getInt(uniform RandomSampler& self)
{
    self.s = LCG_next(self.s); return self.s >> 1;
}

inline uniform unsigned int RandomSampler_getUInt(uniform RandomSampler& self)
{
    self.s = LCG_next(self.s); return self.s;
}

inline uniform float RandomSampler_getFloat(uniform RandomSampler& self)
{
    return (float)RandomSampler_getInt(self) * 4.656612873077392578125e-10f;
}

inline uniform float RNG_uniformFloat(uniform RandomSampler& randomSampler)
{
    return RandomSampler_getFloat(randomSampler);
}

inline uniform int RNG_uniformInt(uniform RandomSampler& randomSampler,
                                  uniform int n)
{
    uniform int x = (uniform int) floor(RNG_uniformFloat(randomSampler) * (n - 1));
    return min(max(0, x), (n - 1));
}

// --------------------------------------------------------------------------------------------------------------------
// Directivity
// --------------------------------------------------------------------------------------------------------------------

typedef uniform float (*DirectivityFn)(uniform Vector3&, void*);

struct Directivity
{
    float dipoleWeight;
    float dipolePower;
    void* callback;
    void* userData;
};

float evaluateDirectivity(float3 point,
                          const uniform CoordinateSpace& source,
                          const uniform Directivity& directivity)
{
    float3 worldSpaceDirection = normalized(point - source.origin);
    float3 localSpaceDirection = transformWorldToLocal(source, worldSpaceDirection);

    if (directivity.callback)
    {
        uniform DirectivityFn directivityCallback = (DirectivityFn) directivity.callback;
        float directivities = 0.0f;
        for (uniform int i = 0; i < programCount; ++i)
        {
            uniform float x = extract(localSpaceDirection.x, i);
            uniform float y = extract(localSpaceDirection.y, i);
            uniform float z = extract(localSpaceDirection.z, i);
            uniform Vector3 direction = { x, y, z };
            directivities = insert(directivities, i, directivityCallback(direction, directivity.userData));
        }
        return directivities;
    }
    else
    {
        float cosine = -localSpaceDirection.z;
        return pow(abs((1.0f - directivity.dipoleWeight) + directivity.dipoleWeight * cosine), directivity.dipolePower);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Material
// --------------------------------------------------------------------------------------------------------------------

struct Material
{
    float absorption[IPL_NUM_BANDS];
    float scattering;
    float transmission[IPL_NUM_BANDS];
};

// --------------------------------------------------------------------------------------------------------------------
// RTCRay Helpers
// --------------------------------------------------------------------------------------------------------------------

inline RTCRay createRay(float3 origin,
                        float3 direction,
                        float tMin,
                        float tMax)
{
    RTCRay ray;
    ray.orgx = origin.x;
    ray.orgy = origin.y;
    ray.orgz = origin.z;
    ray.dirx = direction.x;
    ray.diry = direction.y;
    ray.dirz = direction.z;
    ray.tnear = tMin;
    ray.tfar = tMax;
    ray.time = 0.0f;
    ray.mask = 0xffffffff;
    ray.Ngx = 0.0f;
    ray.Ngy = 0.0f;
    ray.Ngz = 0.0f;
    ray.u = 0.0f;
    ray.v = 0.0f;
    ray.geomID = RTC_INVALID_GEOMETRY_ID;
    ray.primID = RTC_INVALID_GEOMETRY_ID;
    ray.instID = RTC_INVALID_GEOMETRY_ID;
    return ray;
}

// --------------------------------------------------------------------------------------------------------------------
// Sphere
// --------------------------------------------------------------------------------------------------------------------

inline float raySphereIntersect(RTCRay ray,
                                uniform float3 center,
                                uniform float radius)
{
    float3 origin = {ray.orgx, ray.orgy, ray.orgz};
    float3 direction = {ray.dirx, ray.diry, ray.dirz};

    float3 v = origin - center;
    float r = radius;

    float B = 2.0f * dot(v, direction);
    float C = dot(v, v) - (r * r);
    float D = (B * B) - (4.0f * C);

    if (D < 0.0f)
        return FLT_MAX;

    float t = -0.5f * (B + sqrt(D));
    return t;
}

// --------------------------------------------------------------------------------------------------------------------
// EmbreeScene
// --------------------------------------------------------------------------------------------------------------------

struct EmbreeScene
{
    RTCScene scene;
    const Material* const* materialsForGeometry;
    const int* const* materialIndicesForGeometry;
};

// --------------------------------------------------------------------------------------------------------------------
// EnergyField
// --------------------------------------------------------------------------------------------------------------------

struct EnergyField
{
    int numChannels;
    int numBins;
    float* const* const* data;
};

// --------------------------------------------------------------------------------------------------------------------
// EmbreeReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

struct EmbreeReflectionSimulator
{
    float speedOfSound;
    int maxNumRays;
    int numDiffuseSamples;
    float maxDuration;
    int maxOrder;
    int maxNumSources;
    int numSources;
    const uniform CoordinateSpace* sources;
    const uniform CoordinateSpace* listener;
    const uniform Directivity* directivities;
    int numRays;
    int numBounces;
    float duration;
    int order;
    float irradianceMinDistance;
    const uniform float* const uniform* listenerSamples;
    const uniform float* const uniform* diffuseSamples;
    const uniform float* const uniform* listenerCoeffs;
};

inline bool tracePath(const uniform EmbreeScene& scene,
                      const uniform EmbreeReflectionSimulator& simulator,
                      RTCRay& ray,
                      uniform int bounce,
                      float accumDistance,
                      float3& hitPoint,
                      float3& hitNormal,
                      Material& hitMaterial)
{
    uniform RTCIntersectContext context;
    context.flags = (bounce == 0) ? RTC_INTERSECT_COHERENT : RTC_INTERSECT_INCOHERENT;

    rtcIntersectEx(scene.scene, &context, ray);
    if (ray.geomID == RTC_INVALID_GEOMETRY_ID)
        return false;

    if (accumDistance > simulator.duration * simulator.speedOfSound)
        return false;

    if (ray.tfar < LISTENER_RADIUS)
        return false;

    float3 rayOrigin = { ray.orgx, ray.orgy, ray.orgz };
    float3 rayDirection = { ray.dirx, ray.diry, ray.dirz };

    hitNormal.x = ray.Ngx;
    hitNormal.y = ray.Ngy;
    hitNormal.z = ray.Ngz;
    hitNormal = normalized(hitNormal);
    if (dot(hitNormal, rayDirection) > 0.0f)
        hitNormal *= -1.0f;

    hitPoint = rayOrigin + (ray.tfar * rayDirection) + (IPL_HIT_SURFACE_OFFSET * hitNormal);

    int geomID = (ray.instID == RTC_INVALID_GEOMETRY_ID) ? ray.geomID : ray.instID;
    int materialIndex = scene.materialIndicesForGeometry[geomID][ray.primID];
    hitMaterial = scene.materialsForGeometry[geomID][materialIndex];

    if (bounce > 0)
    {
        uniform float3 L = { simulator.listener->origin.x, simulator.listener->origin.y, simulator.listener->origin.z };

        for (uniform int i = 0; i < simulator.numSources; ++i)
        {
            uniform float3 S = { simulator.sources[i].origin.x, simulator.sources[i].origin.y, simulator.sources[i].origin.z };
            uniform float3 SL = L - S;
            if (length(SL) > SOURCE_RADIUS)
            {
                float sourceHitDistance = raySphereIntersect(ray, simulator.sources[i].origin, SOURCE_RADIUS);
                if (0.0f <= sourceHitDistance && sourceHitDistance < ray.tfar)
                    return false;
            }
        }

        float listenerHitDistance = raySphereIntersect(ray, simulator.listener->origin, LISTENER_RADIUS);
        if (0.0f <= listenerHitDistance && listenerHitDistance < ray.tfar)
            return false;
    }

    return true;
}

inline bool shade(const uniform EmbreeScene& scene,
                  const uniform EmbreeReflectionSimulator& simulator,
                  RTCRay& ray,
                  uniform int bounce,
                  uniform int sourceIndex,
                  float3 hitPoint,
                  float3 hitNormal,
                  const Material& hitMaterial,
                  float3 accumEnergy,
                  float accumDistance,
                  const uniform float scalar,
                  float3& energy,
                  float& delay)
{
    uniform float3 listenerPos = { simulator.listener->origin.x, simulator.listener->origin.y, simulator.listener->origin.z };
    uniform float3 sourcePos = { simulator.sources[sourceIndex].origin.x, simulator.sources[sourceIndex].origin.y, simulator.sources[sourceIndex].origin.z };

    float3 hitToSource = sourcePos - hitPoint;
    if (dot(hitNormal, hitToSource) < 0.0f)
        return false;

    float hitToSourceDistance = length(hitToSource);
    if (hitToSourceDistance <= simulator.irradianceMinDistance)
        return false;

    float3 rayDirection = { ray.dirx, ray.diry, ray.dirz };

    float3 shadowRayDirection = hitToSource / hitToSourceDistance;
    RTCRay shadowRay = createRay(hitPoint, shadowRayDirection, 0.0f, hitToSourceDistance);

    uniform RTCIntersectContext shadowRayContext;
    shadowRayContext.flags = RTC_INTERSECT_INCOHERENT;

    rtcOccludedEx(scene.scene, &shadowRayContext, shadowRay);
    if (shadowRay.geomID != RTC_INVALID_GEOMETRY_ID)
        return false;

    float diffuseTerm = (1.0f / PI) * hitMaterial.scattering * max(dot(hitNormal, shadowRayDirection), 0.0f);
    float3 halfVector = normalized((shadowRayDirection - rayDirection) * 0.5f);
    float specularTerm = ((IPL_SPECULAR_POWER + 2.0f) / (8.0f * PI)) * (1.0f - hitMaterial.scattering) * pow(dot(halfVector, hitNormal), IPL_SPECULAR_POWER);
    float attenuation = 1.0f / max(hitToSourceDistance, simulator.irradianceMinDistance);
    float distanceTerm = (1.0f / (4.0f * PI)) * (attenuation * attenuation);
    float directivityTerm = evaluateDirectivity(hitPoint, simulator.sources[sourceIndex], simulator.directivities[sourceIndex]);
    float frequencyIndependentTerm = scalar * distanceTerm * directivityTerm * (diffuseTerm + specularTerm);

    for (uniform int i = 0; i < IPL_NUM_BANDS; ++i)
    {
        energy[i] = frequencyIndependentTerm * (1.0f - hitMaterial.absorption[i]) * accumEnergy[i];
    }

    float distance = accumDistance + ray.tfar + hitToSourceDistance;
    delay = (distance - length(sourcePos - listenerPos)) / simulator.speedOfSound;

    return true;
}

inline void bouncePath(const uniform EmbreeScene& scene,
                       const uniform EmbreeReflectionSimulator& simulator,
                       uniform RandomSampler& rng,
                       uniform int bounce,
                       float3 hitPoint,
                       float3 hitNormal,
                       const Material& hitMaterial,
                       RTCRay& ray,
                       float3& accumEnergy,
                       float& accumDistance)
{
    for (uniform int i = 0; i < IPL_NUM_BANDS; ++i)
    {
        accumEnergy[i] *= (1.0f - hitMaterial.absorption[i]);
    }

    accumDistance += ray.tfar;

    float3 rayDirection = { ray.dirx, ray.diry, ray.dirz };
    float3 bouncedDirection;

    if (RNG_uniformFloat(rng) < hitMaterial.scattering)
    {
        uniform int diffuseSampleIndex = RNG_uniformInt(rng, simulator.numDiffuseSamples);
        uniform float3 diffuseSample = { simulator.diffuseSamples[0][diffuseSampleIndex], simulator.diffuseSamples[1][diffuseSampleIndex] , simulator.diffuseSamples[2][diffuseSampleIndex] };
        bouncedDirection = transformHemisphereSample(diffuseSample, hitNormal);
    }
    else
    {
        bouncedDirection = reflect(rayDirection, hitNormal);
    }

    ray = createRay(hitPoint, bouncedDirection, 0.0f, FLT_MAX);
}

export void simulateImage(const uniform EmbreeScene* uniform scene,
                          const uniform EmbreeReflectionSimulator* uniform simulator,
                          uniform int xIndex,
                          uniform int yIndex,
                          uniform int blockSize,
                          uniform int imageSize,
                          uniform float* uniform image)
{
    const uniform float scalar = 500.0f;

    uniform RandomSampler rng;
    RandomSampler_init(rng, clock());

    foreach_tiled (v = xIndex ... (xIndex + blockSize), u = yIndex ... (yIndex + blockSize))
    {
        int i = v * imageSize + u;

        float du = ((u / (float) imageSize) - 0.5f) * 2.0f;
        float dv = ((v / (float) imageSize) - 0.5f) * 2.0f;
        float3 direction = normalized(du * simulator->listener->right + dv * simulator->listener->up - simulator->listener->ahead);

        RTCRay ray = createRay(simulator->listener->origin, direction, 0.0f, FLT_MAX);

        float3 accumEnergy = { 1.0f, 1.0f, 1.0f };
        float accumDistance = 0.0f;

        for (uniform int bounce = 0; bounce < simulator->numBounces; ++bounce)
        {
            float3 hitPoint;
            float3 hitNormal;
            Material hitMaterial;
            if (!tracePath(*scene, *simulator, ray, bounce, accumDistance, hitPoint, hitNormal, hitMaterial))
                break;

            for (uniform int j = 0; j < simulator->numSources; ++j)
            {
                float3 energy = { 0.0f, 0.0f, 0.0f };
                float delay = 0.0f;

                if (!shade(*scene, *simulator, ray, bounce, j, hitPoint, hitNormal, hitMaterial, accumEnergy, accumDistance, scalar, energy, delay))
                    continue;

                image[4 * i + 0] += energy[0];
                image[4 * i + 1] += energy[1];
                image[4 * i + 2] += energy[2];
            }

            if (bounce < simulator->numBounces - 1)
            {
                bouncePath(*scene, *simulator, rng, bounce, hitPoint, hitNormal, hitMaterial, ray, accumEnergy, accumDistance);
            }
        }
    }
}

export void simulateEnergyField(const uniform EmbreeScene* uniform scene,
                                const uniform EmbreeReflectionSimulator* uniform simulator,
                                uniform int startIndex,
                                uniform int endIndex,
                                uniform int threadIndex,
                                uniform EnergyField* uniform energyFields)
{
    const uniform float scalar = (4.0f * PI) / simulator->numRays;

    uniform RandomSampler rng;
    RandomSampler_init(rng, clock());

    foreach (i = startIndex ... endIndex)
    {
        float3 rayDirection = { simulator->listenerSamples[0][i], simulator->listenerSamples[1][i], simulator->listenerSamples[2][i] };
        RTCRay ray = createRay(simulator->listener->origin, rayDirection, 0.0f, FLT_MAX);

        float3 accumEnergy = { 1.0f, 1.0f, 1.0f };
        float accumDistance = 0.0f;

        for (uniform int bounce = 0; bounce < simulator->numBounces; ++bounce)
        {
            float3 hitPoint;
            float3 hitNormal;
            Material hitMaterial;
            if (!tracePath(*scene, *simulator, ray, bounce, accumDistance, hitPoint, hitNormal, hitMaterial))
                break;

            for (uniform int j = 0; j < simulator->numSources; ++j)
            {
                float3 energy = { 0.0f, 0.0f, 0.0f };
                float delay = 0.0f;

                if (!shade(*scene, *simulator, ray, bounce, j, hitPoint, hitNormal, hitMaterial, accumEnergy, accumDistance, scalar, energy, delay))
                    continue;

                uniform int energyFieldIndex = threadIndex * simulator->numSources + j;
                uniform EnergyField& energyField = energyFields[energyFieldIndex];

                uniform int numBins = energyField.numBins;
                int bin = (int) floor(delay / IPL_BIN_DURATION);
                if (bin < 0 || numBins <= bin)
                   continue;

                for (uniform int band = 0; band < IPL_NUM_BANDS; ++band)
                {
                    for (uniform int k = 0; k < energyField.numChannels; ++k)
                    {
                        atomic_add_local(&energyField.data[k][band][bin], simulator->listenerCoeffs[k][i] * energy[band]);
                    }
                }
            }

            if (bounce < simulator->numBounces - 1)
            {
                bouncePath(*scene, *simulator, rng, bounce, hitPoint, hitNormal, hitMaterial, ray, accumEnergy, accumDistance);
            }
        }
    }
}
