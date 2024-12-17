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

#include <catch.hpp>

#include <phonon.h>

IPLVector3 GetRandomDirection()
{
    float theta = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2 * 3.14159265f;
    float z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float oneminusz2 = sqrtf(1 - z * z);
    return { oneminusz2 * cosf(theta), oneminusz2 * sinf(theta), z };
}

bool IsFinite(IPLAudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels; ++i)
        for (int j = 0; j < buffer.numSamples; ++j)
            if (!std::isfinite(buffer.data[i][j]))
                return false;

    return true;
}

void FillRandomData(float* buffer, size_t size)
{
    for (auto i = 0U; i < size; ++i)
        buffer[i] = (rand() % 10001) / 10000.0f;
}

bool ValidateBinauralEffect(int numChannels, IPLHRTFInterpolation interpolation, IPLHRTFSettings hrtfParams, int frameSize)
{
    const int kNumRuns = 10000;
    const int kSamplingRate = 48000;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, frameSize };

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &dspParams, &hrtfParams, &hrtf);
    if (hrtf == nullptr)
        return false;

    float* inData[2];
    float* outData[2];

    inData[0] = new float[frameSize];
    inData[1] = new float[frameSize];
    memset(inData[0], 0, sizeof(int) * frameSize);
    memset(inData[1], 0, sizeof(int) * frameSize);

    outData[0] = new float[frameSize];
    outData[1] = new float[frameSize];
    memset(outData[0], 0, sizeof(int) * frameSize);
    memset(outData[1], 0, sizeof(int) * frameSize);

    FillRandomData(inData[0], frameSize);
    FillRandomData(inData[1], frameSize);

    IPLBinauralEffect effect = nullptr;
    IPLBinauralEffectSettings effectSettings{ hrtf };
    iplBinauralEffectCreate(context, &dspParams, &effectSettings, &effect);

    IPLAudioBuffer inBuffer{ numChannels, frameSize, inData };
    IPLAudioBuffer outBuffer{ 2, frameSize, outData };

    for (auto i = 0; i < kNumRuns; ++i)
    {
        IPLVector3 direction = GetRandomDirection();
        IPLBinauralEffectParams params{ direction, interpolation, 1.0f, hrtf };
        iplBinauralEffectApply(effect, &params, &inBuffer, &outBuffer);
        if (!IsFinite(outBuffer))
        {
            printf("Non finite output for direction [%f %f %f]\n", direction.x, direction.y, direction.z);
            return false;
        }
    }

    iplBinauralEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    delete[] inData[0];
    delete[] outData[0];
    delete[] outData[1];

    return true;
}

TEST_CASE("Applying binaural effects with nearest neighbor interpolation on default HRTF does not produce NANs.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_NEAREST, hrtfParams, 512));
}

TEST_CASE("Applying binaural effects with bilinear interpolation on default HRTF does not produce NANs.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_BILINEAR, hrtfParams, 512));
}

TEST_CASE("Applying binaural effects with nearest neighbor interpolation on default HRTF does not produce NANs. 1024 frame size.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_NEAREST, hrtfParams, 1024));
}

TEST_CASE("Applying binaural effects with bilinear interpolation on default HRTF does not produce NANs. 1024 frame size.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_BILINEAR, hrtfParams, 1024));
}

#if !defined(IPL_OS_IOS) && !defined(IPL_OS_WASM)

TEST_CASE("Applying binaural effects with nearest neighbor interpolation on SOFA HRTF does not produce NANs. D1.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_NEAREST, hrtfParams, 512));
}

TEST_CASE("Applying binaural effects with bilinear interpolation on SOFA HRTF does not produce NANs. D1.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_BILINEAR, hrtfParams, 512));
}

TEST_CASE("Applying binaural effects with nearest neighbor interpolation on SOFA HRTF does not produce NANs. D1. 1024 frame size.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_NEAREST, hrtfParams, 1024));
}

TEST_CASE("Applying binaural effects with bilinear interpolation on SOFA HRTF does not produce NANs. D1. 1024 frame size.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_BILINEAR, hrtfParams, 1024));
}

TEST_CASE("Applying binaural effects with nearest neighbor interpolation on SOFA HRTF does not produce NANs. H12.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_h12.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_NEAREST, hrtfParams, 512));
}

TEST_CASE("Applying binaural effects with bilinear interpolation on SOFA HRTF does not produce NANs. H12.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_h12.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_BILINEAR, hrtfParams, 512));
}

TEST_CASE("Applying binaural effects with nearest neighbor interpolation on SOFA HRTF does not produce NANs. H12. 1024 frame size.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_h12.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_NEAREST, hrtfParams, 1024));
}

TEST_CASE("Applying binaural effects with bilinear interpolation on SOFA HRTF does not produce NANs. H12. 1024 frame size.", "[BinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_h12.sofa";
    REQUIRE(ValidateBinauralEffect(1, IPL_HRTFINTERPOLATION_BILINEAR, hrtfParams, 1024));
}

#endif
