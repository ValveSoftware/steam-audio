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

#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable

// --------------------------------------------------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------------------------------------------------

#define PI                          3.14159f              // Mathematical constant Pi
#define SOURCE_RADIUS               0.1f                  // Radius of the sound source sphere (in meters)
#define LISTENER_RADIUS             0.1f                  // Radius of the listener sphere (in meters)
#define SPEED_OF_SOUND              340.0f                // Speed of sound in air (in meters per second)
#define RAY_SURFACE_OFFSET          1e-2f                 // Small offset to prevent ray self-intersection (meters)
#define SPECULAR_EXPONENT           1e+2f                 // Exponent for specular reflection in Phong shading model (unitless, higher value = more specular)
#define NUM_BANDS                   3                     // Number of frequency bands (e.g., Low, Mid, High)
#define NUM_BINS                    256                   // Number of time bins used for energy histograms
#define BIN_DURATION                0.01f                 // Duration of each time bin (in seconds)
#define NUM_LOCAL_HISTOGRAMS        2                     // Number of local histograms per workgroup (for atomic contention reduction, tunable)

// --------------------------------------------------------------------------------------------------------------------
// CoordinateSpace
// --------------------------------------------------------------------------------------------------------------------

typedef struct __attribute__((packed)) CoordinateSpace_t
{
    float3 right;
    float3 up;
    float3 ahead;
    float3 origin;
} CoordinateSpace;

CoordinateSpace createCoordinateSpace(float3 normal)
{
    CoordinateSpace space;
    space.ahead = normal;

    if (fabs(normal.x) > fabs(normal.z))
    {
        float3 right = (float3) (-normal.y, normal.x, 0.0f);
        space.right = normalize(right);
    }
    else
    {
        float3 right = (float3) (0.0f, -normal.z, normal.y);
        space.right = normalize(right);
    }

    space.up = cross(space.right, space.ahead);

    return space;
}

float3 transformLocalToWorld(CoordinateSpace space,
                             float3 direction)
{
    return direction.x * space.right + direction.y * space.up - direction.z * space.ahead;
}

float3 transformWorldToLocal(CoordinateSpace space,
                             float3 direction)
{
    float3 transformedDirection;
    transformedDirection.x = dot(direction, space.right);
    transformedDirection.y = dot(direction, space.up);
    transformedDirection.z = -dot(direction, space.ahead);
    return transformedDirection;
}

float3 transformHemisphereSample(float3 direction,
                                 float3 normal)
{
    CoordinateSpace tangentSpace = createCoordinateSpace(normal);
    return normalize(transformLocalToWorld(tangentSpace, direction));
}

// --------------------------------------------------------------------------------------------------------------------
// Random Sampling
// --------------------------------------------------------------------------------------------------------------------

// The following code is from the Radeon Rays / Baikal GitHub repository. It can be found at:
//      https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRender-Baikal/blob/master/Baikal/Kernels/CL/sampling.cl

typedef struct RNG_t
{
    uint    value;
} RNG;

uint wangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint randUint(local RNG* rng)
{
    rng->value = wangHash(1664525U * rng->value + 1013904223U);
    return rng->value;
}

float randFloat(local RNG* rng)
{
    return ((float)randUint(rng)) / 0xffffffffU;
}

void initRNG(uint seed, local RNG* rng)
{
    rng->value = wangHash(seed);
}

float2 uniformRandom2d(local RNG* rng)
{
    return (float2) (randFloat(rng), randFloat(rng));
}

// --------------------------------------------------------------------------------------------------------------------
// Directivity
// --------------------------------------------------------------------------------------------------------------------

// NOTE: Custom directivity callbacks are not supported at this time.
typedef struct __attribute__((packed)) Directivity_t
{
    float dipoleWeight;
    float dipolePower;
} Directivity;

float evaluateDirectivity(float3 point,
                          CoordinateSpace coordinates,
                          Directivity directivity)
{
    float3 worldSpaceDirection = normalize(point - coordinates.origin);
    float3 localSpaceDirection = transformWorldToLocal(coordinates, worldSpaceDirection);

    float cosine = -localSpaceDirection.z;
    return pow(fabs((1.0f - directivity.dipoleWeight) + directivity.dipoleWeight * cosine), directivity.dipolePower);
}

// --------------------------------------------------------------------------------------------------------------------
// Material
// --------------------------------------------------------------------------------------------------------------------

typedef struct __attribute__((packed)) Material_t
{
    float  absorptionLow;
    float  absorptionMid;
    float  absorptionHigh;
    float  scattering;
    float  transmissionLow;
    float  transmissionMid;
    float  transmissionHigh;
} Material;

// --------------------------------------------------------------------------------------------------------------------
// Radeon Rays Helpers
// --------------------------------------------------------------------------------------------------------------------

// This must exactly match the ray data structure (struct ray) used by Radeon Rays.
typedef struct Ray_t
{
    float4  o;
    float4  d;
    int2    extra;
    int2    padding;
} Ray;

// This must exactly match the hit data structure (struct Intersection) used by Radeon Rays.
typedef struct Hit_t
{
    int       shapeid;
    int       primid;
    int       padding0;
    int       padding1;
    float4    uvwt;
} Hit;

// --------------------------------------------------------------------------------------------------------------------
// IIR Filtering
// --------------------------------------------------------------------------------------------------------------------

typedef struct IIR_t
{
    float a1, a2;
    float b0, b1, b2;
} IIR;

// --------------------------------------------------------------------------------------------------------------------
// Ray Generation Kernels
// --------------------------------------------------------------------------------------------------------------------

kernel void generateCameraRays(global CoordinateSpace* camera,
                               global Ray* rays)
{
    uint width = get_global_size(0);
    uint height = get_global_size(1);
    uint u = get_global_id(0);
    uint v = get_global_id(1);
    uint index = v * width + u;

    float du = ((u / (float) width) - 0.5f) * 2.0f;
    float dv = ((v / (float) height) - 0.5f) * 2.0f;

    rays[index].o = (float4) (camera->origin, FLT_MAX);
    rays[index].d = (float4) (normalize(du * camera->right + dv * camera->up - camera->ahead), 0.0f);
    rays[index].extra = (int2) (0xffffffff, 1);
}

kernel void generateListenerRays(global CoordinateSpace* listeners,
                                 global float4* sphereSamples,
                                 global Ray* rays)
{
    size_t rayIndex = get_global_id(0);
    size_t listenerIndex = get_global_id(1);
    size_t index = listenerIndex * get_global_size(0) + rayIndex;

    rays[index].o = (float4) (listeners[listenerIndex].origin, FLT_MAX);
    rays[index].d = (float4) (sphereSamples[rayIndex].xyz, 0.0f);
    rays[index].extra = (int2) (0xffffffff, 1);
}

// --------------------------------------------------------------------------------------------------------------------
// Sphere Occlusion Kernel
// --------------------------------------------------------------------------------------------------------------------

float raySphereIntersect(global const Ray* ray,
                         float3 center,
                         float radius)
{
    float3 origin = ray->o.xyz;
    float3 direction = ray->d.xyz;

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

kernel void sphereOcclusion(uint numSources,
                            global const CoordinateSpace* sources,
                            uint numListeners,
                            global const CoordinateSpace* listeners,
                            global Ray* rays,
                            global Hit* hits)
{
    uint numRays = get_global_size(0);
    uint rayIndex = get_global_id(0);

    for (int i = 0; i < numListeners; ++i)
    {
        uint index = i * numRays + rayIndex;

        float listenerSphereHitDistance = raySphereIntersect(&rays[index], listeners[i].origin, LISTENER_RADIUS);
        if (0.0f <= listenerSphereHitDistance && listenerSphereHitDistance < hits[index].uvwt.s3)
        {
            rays[index].extra.y = 0;
            hits[index].primid = -1;
            return;
        }

        for (int j = 0; j < numSources; ++j)
        {
            float sourceSphereHitDistance = raySphereIntersect(&rays[index], sources[j].origin, SOURCE_RADIUS);
            if (0.0f <= sourceSphereHitDistance && sourceSphereHitDistance < hits[index].uvwt.s3)
            {
                rays[index].extra.y = 0;
                hits[index].primid = -1;
                return;
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Shading + Shadow/Bounced Ray Generation Kernels
// --------------------------------------------------------------------------------------------------------------------

float3 calculateHitPoint(Ray ray,
                         Hit hit)
{
    return ray.o.xyz + hit.uvwt.s3 * ray.d.xyz;
}

float3 calculateHitNormal(Ray ray,
                          Hit hit,
                          global float4* normals)
{
    float3 hitNormal = normals[hit.primid].xyz;
    if (dot(hitNormal, ray.d.xyz) > 0.0f)
    {
        hitNormal = -hitNormal;
    }

    return hitNormal;
}

float pointSourceIrradiance(float distance,
                            float minDistance)
{
    float attenuation = 1.0f / max(distance, minDistance);
    float irradiance = (1.0f / (4.0f * PI)) * (attenuation * attenuation);
    return irradiance;
}

float3 reflect(float3 incident,
               float3 normal)
{
    return normalize(incident - (2.0f * dot(incident, normal) * normal));
}

kernel void shadeAndBounce(uint numSources,
                           global const CoordinateSpace* sources,
                           uint numListeners,
                           global const CoordinateSpace* listeners,
                           global const Directivity* directivities,
                           uint numRays,
                           uint numBounces,
                           float irradianceMinDistance,
                           global const Ray* rays,
                           global const Hit* hits,
                           global const float3* normals,
                           global const int* materialIndices,
                           global const Material* materials,
                           uint numDiffuseSamples,
                           global const float4* diffuseSamples,
                           uint randomNumber,
                           float scalar,
                           global Ray* shadowRays,
                           global Ray* reflectedRays,
                           global float4* energyDelay,
                           global float4* accumEnergyDelay)
{
    size_t numChunks = max(numListeners, numSources);
    size_t numPrimaryRays = get_global_size(0) / numChunks;
    size_t numShadowRays = get_global_size(0);

    uint rayIndex = (numListeners > 1) ? get_global_id(0) : (get_global_id(0) % numPrimaryRays);
    uint chunkIndex = get_global_id(0) / numPrimaryRays;
    uint listenerIndex = (numListeners > 1) ? chunkIndex : 0;
    uint sourceIndex = (numSources > 1) ? chunkIndex : 0;
    uint shadowRayIndex = get_global_id(0);

    // If this ray is disabled, don't do anything.
    if (rays[rayIndex].extra.y == 0 || hits[rayIndex].primid < 0)
    {
        shadowRays[shadowRayIndex].extra = 0;
        reflectedRays[rayIndex].extra = 0;
        energyDelay[shadowRayIndex] = (float4) 0.0f;
        return;
    }

    // Random number generation for work group.
    local RNG rng;
    local float randomFloat;
    local uint randomUint;
    if (get_local_id(0) == 0)
    {
        initRNG(randomNumber + rayIndex, &rng);
        randomFloat = randFloat(&rng);
        randomUint = randUint(&rng);
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // Calculate hit point.
    float3 rayDirection = rays[rayIndex].d.xyz;
    int triangleIndex = hits[rayIndex].primid;
    float hitDistance = hits[rayIndex].uvwt.s3;
    float3 hitPoint = rays[rayIndex].o.xyz + hitDistance * rayDirection;

    // Calculate hit normal.
    float3 hitNormal = normals[triangleIndex].xyz;
    if (dot(hitNormal, rayDirection) > 0.0f)
    {
        hitNormal = -hitNormal;
    }

    // Calculate hit material.
    Material hitMaterial = materials[materialIndices[triangleIndex]];
    float3 hitMaterialAbsorption = (float3) (hitMaterial.absorptionLow, hitMaterial.absorptionMid, hitMaterial.absorptionHigh);

    // Calculate shadow ray direction.
    float3 source = sources[sourceIndex].origin;
    float hitToSourceDistance = distance(hitPoint, source);
    float4 hitToSource = (float4) (normalize(source - hitPoint), 0.0f);

    // Skip the ray if:
    //  a) the hit point is inside the listener, or
    //  b) the hit point is too close to the source, or
    //  c) the ray hit a backfacing triangle.
    if (hitDistance <= LISTENER_RADIUS ||
        hitToSourceDistance <= irradianceMinDistance ||
        dot(hitToSource.xyz, hitNormal) < 0.0f)
    {
        shadowRays[shadowRayIndex].extra = 0;
        energyDelay[shadowRayIndex] = (float4) 0.0f;
    }
    else
    {
        // Generate the shadow ray.
        shadowRays[shadowRayIndex].o = (float4) (hitPoint + RAY_SURFACE_OFFSET * hitToSource.xyz, hitToSourceDistance);
        shadowRays[shadowRayIndex].d = hitToSource;
        shadowRays[shadowRayIndex].extra = (int2) (0xffffffff, 1);

        // Calculate shading values.
        float3 energy = (1.0f / PI) * hitMaterial.scattering * max(0.0f, dot(hitNormal, hitToSource.xyz));
        energy += ((SPECULAR_EXPONENT + 2.0f) / (8.0f * PI)) * (1.0f - hitMaterial.scattering) * pow(fabs(dot(normalize(hitToSource.xyz - rayDirection), hitNormal)), SPECULAR_EXPONENT);
        energy *= scalar;
        energy *= evaluateDirectivity(hitPoint, sources[sourceIndex], directivities[sourceIndex]);
        energy *= pointSourceIrradiance(hitToSourceDistance, irradianceMinDistance);
        energy *= accumEnergyDelay[rayIndex].xyz * ((float3) 1.0f - hitMaterialAbsorption);

        float delay = (hitDistance + hitToSourceDistance) / SPEED_OF_SOUND;
        delay += accumEnergyDelay[rayIndex].w - (distance(source, listeners[listenerIndex].origin) / SPEED_OF_SOUND);

        energyDelay[shadowRayIndex] = (float4) (energy, delay);
    }

    barrier(CLK_GLOBAL_MEM_FENCE);

    // Generate the bounced ray.
    if (numListeners > 1 || sourceIndex == 0)
    {
        accumEnergyDelay[rayIndex].xyz *= ((float3) 1.0f - hitMaterialAbsorption);
        accumEnergyDelay[rayIndex].w += hitDistance / SPEED_OF_SOUND;

        float4 reflectedDirection = (float4) 0.0f;
        if (randomFloat < hitMaterial.scattering)
        {
            uint sampleIndex = randomUint % numDiffuseSamples;
            float3 transformedDiffuseSample = transformHemisphereSample(diffuseSamples[sampleIndex].xyz, hitNormal);
            reflectedDirection = (float4) (transformedDiffuseSample, 0.0f);
        }
        else
        {
            reflectedDirection = (float4) (reflect(rayDirection, hitNormal), 0.0f);
        }

        reflectedRays[rayIndex].o = (float4) (hitPoint + RAY_SURFACE_OFFSET * reflectedDirection.xyz, FLT_MAX);
        reflectedRays[rayIndex].d = reflectedDirection;
        reflectedRays[rayIndex].extra = (int2) (0xffffffff, 1);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Image Gather Kernel
// --------------------------------------------------------------------------------------------------------------------

kernel void gatherImage(uint numSources,
                        global const int* occluded,
                        global const float4* totalEnergy,
                        global float4* image)
{
    size_t rayIndex = get_global_id(0);
    size_t numRays = get_global_size(0);

    for (uint i = 0; i < numSources; ++i)
    {
        if (occluded[i * numRays + rayIndex] < 0)
        {
            image[rayIndex] += totalEnergy[i * numRays + rayIndex];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Energy Field Gather Kernel
// --------------------------------------------------------------------------------------------------------------------

kernel void gatherEnergyField(float scale,
                              global const float4* totalEnergy,
                              uint offset,
                              global const int* occluded,
                              global const float* shCoefficients,
                              global int* energy)
{
	// The number of global work-items for this kernel is #rays * #bands * #channels.
	// The number of local work-items for this kernel is #bins * 1 * 1. Here, #bins is a compile-time constant.
	// This kernel runs in three stages:
	//	1.	Each work-group operates on a subset of rays, and accumulates their energy into one of multiple
	//		local-memory histograms.
	//	2.	Each work-group adds all of its local-memory histograms together.
	//	3.	The work-groups cooperate and combine their local-memory histograms into a global-memory histogram.

    uint rayIndex = get_global_id(0);
    uint band = get_global_id(1);
    uint channel = get_global_id(2);

    size_t numRays = get_global_size(0);

    bool isOccluded = (occluded[offset + rayIndex] >= 0);

	// FIXME: shouldn't this be just get_local_id(0)?
    const int localIndex = get_local_id(1) * get_local_size(0) + get_local_id(0);

	// Each work-group stores NUM_LOCAL_HISTOGRAMS histograms in local memory. These are interleaved, i.e.,
	// bin i of histogram j is at index (NUM_LOCAL_HISTOGRAMS * i + j) in the buffer. We use multiple local-memory
	// histograms to reduce the chance that an atomic_add leads to contention (see below for details).
    local int localEnergy[NUM_BINS * NUM_LOCAL_HISTOGRAMS];

	// These base pointers are used in stage 2 of the kernel. See below for details.
    local int* workItemEnergy = localEnergy + mul24(localIndex, NUM_LOCAL_HISTOGRAMS);

	// Initialize all local-memory histograms to zero.
    for (int i = 0; i < NUM_LOCAL_HISTOGRAMS; i++)
    {
        workItemEnergy[i] = 0.0f;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

	// Stage 1
	// Each work-item processes one ray. If it is not occluded (i.e., it carries non-zero energy), the energy is added
	// to one of the local-memory histograms for the work-group. These adds need to be atomic, since multiple rays
	// being processed in the same work-group may write to the same bin of the same histogram. The contention
	// due to atomic_adds can be reduced by increasing NUM_LOCAL_HISTOGRAMS, at the cost of increased local memory
	// usage.
    if (!isOccluded)
    {
		// Adjacent work-items in a work-group do not use the same local-memory histogram. A work-item with local
		// index i writes to local-memory histogram (i % NUM_LOCAL_HISTOGRAMS).
		// When calculating the bin index, bin index i is mapped to index  NUM_LOCAL_HISTOGRAMS * i in the
		// local-memory buffer. This is because the histograms are interleaved.
        local int* shiftedLocalEnergy = localEnergy + localIndex % NUM_LOCAL_HISTOGRAMS;

        global float* rayEnergy = (global float*) (&totalEnergy[offset + rayIndex]);

        float time = totalEnergy[offset + rayIndex].w;
        uint bin = convert_uint_sat(floor(time / BIN_DURATION)) * NUM_LOCAL_HISTOGRAMS;

        if (bin < NUM_BINS)
        {
            float energyValue = scale * rayEnergy[band] * shCoefficients[channel * numRays + rayIndex];
            int quantizedEnergyValue = convert_int_sat(floor(energyValue));
            atomic_add(shiftedLocalEnergy + bin, quantizedEnergyValue);
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

	// Stage 2
	// Each work-item processes one bin. The corresponding bins for all local-memory histograms are added, and the
	// result is stored in a private variable. No atomic operations or barriers are required here, since each work-item
	// operates on independent data.
    int accumulatedEnergy = 0;
    for (int i = 0; i < NUM_LOCAL_HISTOGRAMS; i++)
    {
        accumulatedEnergy += workItemEnergy[i];
    }

	// Stage 3
	// Each work-item processes one bin. The summed values from the local-memory histograms are added to the corresponding
	// bin in a global-memory histogram. Here, atomic_adds must be used, because multiple work-groups may be trying to
	// write to the same global-memory bin at the same time.
    atomic_add(energy + channel * NUM_BANDS * NUM_BINS + band * NUM_BINS + localIndex, accumulatedEnergy);
}

// --------------------------------------------------------------------------------------------------------------------
// Reconstruction Kernels
// --------------------------------------------------------------------------------------------------------------------

#define ENERGY_THRESHOLD            1e-7f
#define MIN_VARIANCE                1e-5f
#define NUM_WORK_ITEM_SAMPLES       32
#define WORK_GROUP_SIZE				64

kernel void applyIIRFilter(global IIR* filters,
                           global float* impulseResponse,
						   uint numBins,
						   uint samplesPerBin,
						   uint numSamples)
{
    size_t band = get_global_id(0);
    size_t channel = get_global_id(1);
    size_t numBands = get_global_size(0);
    size_t numChannels = get_global_size(1);
    size_t batch = get_global_id(2);

	local float localSamples[(NUM_WORK_ITEM_SAMPLES * WORK_GROUP_SIZE) + NUM_WORK_ITEM_SAMPLES];

	const int localIndex = get_local_id(1) * get_local_size(0) + get_local_id(0);
	local float* workItemSamples = &localSamples[localIndex * NUM_WORK_ITEM_SAMPLES];

    global float* signal = &impulseResponse[(batch * numChannels * numBands * numSamples) + (channel * numBands * numSamples) + (band * numSamples)];

	float xm1 = 0.0f;
	float xm2 = 0.0f;
	float ym1 = 0.0f;
	float ym2 = 0.0f;

    global IIR* filter = &filters[band];

    float a1 = filter->a1;
    float a2 = filter->a2;
    float b0 = filter->b0;
    float b1 = filter->b1;
    float b2 = filter->b2;

	for (uint i = 0; i < (numBins * samplesPerBin); i += NUM_WORK_ITEM_SAMPLES)
    {
		for (uint j = 0; j < NUM_WORK_ITEM_SAMPLES; ++j)
		{
			workItemSamples[j] = signal[i + j];
		}

		for (uint j = 0; j < NUM_WORK_ITEM_SAMPLES; ++j)
		{
			float x = workItemSamples[j];
			float y = (b0 * x) + (b1 * xm1) + (b2 * xm2) - (a1 * ym1) - (a2 * ym2);

			xm2 = xm1;
			xm1 = x;
			ym2 = ym1;
			ym1 = y;

			workItemSamples[j] = y;
		}

		for (uint j = 0; j < NUM_WORK_ITEM_SAMPLES; ++j)
		{
			signal[i + j] = workItemSamples[j];
		}
    }
}

// todo: linear reconstruction?
kernel void reconstructImpulseResponse(global int* energy,
									   uint samplingRate,
                                       uint samplesPerBin,
									   uint numSamples,
									   global float* airAbsorption,
									   global IIR* filters,
                                       global float* whiteNoise,
                                       global float* impulseResponse,
                                       uint offset,
									   float scale)
{
    size_t bin = get_global_id(0);
    size_t band = get_global_id(1);
    size_t channel = get_global_id(2);
	size_t numBins = get_global_size(0);
    size_t numBands = get_global_size(1);
    size_t numChannels = get_global_size(2);

    global float* impulseResponseBin = &impulseResponse[offset + (channel * numBands * numSamples) + (band * numSamples) + (bin * samplesPerBin)];
    global float* whiteNoiseBin = &whiteNoise[(channel * numBands * numSamples) + (band * numSamples) + (bin * samplesPerBin)];

    float e0 = (float)energy[0*numBands*NUM_BINS + band*NUM_BINS + bin] / scale;
    float e = (float)energy[channel*numBands*NUM_BINS + band*NUM_BINS + bin] / scale;

    if (fabs(e) < ENERGY_THRESHOLD || fabs(e0) < ENERGY_THRESHOLD)
    {
        for (int i = 0; i < samplesPerBin; ++i)
        {
            impulseResponseBin[i] = 0.0f;
        }
    }
    else
    {
        float tMean = ((bin + 0.5f) * samplesPerBin) / samplingRate;
        float tVariance = MIN_VARIANCE;

        int sample = bin * samplesPerBin;
        float binEnergy = 0.0f;

        float t = sample / (float) samplingRate;
        float dt = 1.0f / (float) samplingRate;

        float g = exp(-((t - tMean) * (t - tMean)) / (2.0f * tVariance));
        float dg = exp(-(dt * ((2.0f * (t - tMean)) + dt)) / (2.0f * tVariance));
        float ddg = exp(-(dt * dt) / tVariance);

        for (int i = 0; i < samplesPerBin; ++i)
        {
            impulseResponseBin[i] = g * whiteNoiseBin[i];
            binEnergy += impulseResponseBin[i] * impulseResponseBin[i];
            g *= dg;
            dg *= ddg;
        }

        float normalization = e / sqrt(e0 * sqrt(4.0f * PI));

        normalization *= exp(-0.5f * airAbsorption[band] * SPEED_OF_SOUND * ((bin + 0.5f) * samplesPerBin * (1.0f / samplingRate)));

        for (int i = 0; i < samplesPerBin; ++i)
        {
            impulseResponseBin[i] *= normalization;
        }
    }
}

kernel void combineBandpassedImpulseResponse(uint numSamples,
                                             global float* bandImpulseResponses,
											 global float* impulseResponse)
{
    size_t sample = get_global_id(0);
    size_t channel = get_global_id(1);
    size_t batch = get_global_id(2);
	size_t numChannels = get_global_size(1);

	float value = 0.0f;

    for (int i = 0; i < NUM_BANDS; ++i)
    {
        value += bandImpulseResponses[(batch * numChannels * NUM_BANDS * numSamples) + (channel * NUM_BANDS * numSamples) + (i * numSamples) + sample];
    }

	impulseResponse[(batch * numChannels * numSamples) + (channel * numSamples) + sample] = value;
}
