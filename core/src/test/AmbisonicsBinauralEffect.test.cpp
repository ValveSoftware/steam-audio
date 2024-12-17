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

extern bool IsFinite(IPLAudioBuffer& buffer);
extern void FillRandomData(float* buffer, size_t size);

bool ValidateAmbisonicsBinauralEffect(IPLHRTFSettings hrtfParams, int frameSize, int order)
{
    const int kSamplingRate = 48000;
    int numChannels = (order + 1) * (order + 1);

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, frameSize };

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &dspParams, &hrtfParams, &hrtf);
    if (hrtf == nullptr)
        return false;

    float** inData = new float*[numChannels];
    for (int i = 0; i < numChannels; ++i)
    {
        inData[i] = new float[frameSize];
        memset(inData[i], 0, sizeof(int) * frameSize);
        FillRandomData(inData[i], frameSize);
    }

    float* outData[2];
    for (int i = 0; i < 2; ++i)
    {
        outData[i] = new float[frameSize];
        memset(outData[i], 0, sizeof(int) * frameSize);
    }

    IPLAmbisonicsBinauralEffect effect = nullptr;
    IPLAmbisonicsBinauralEffectSettings effectSettings{ hrtf, order };
    iplAmbisonicsBinauralEffectCreate(context, &dspParams, &effectSettings, &effect);

    IPLAudioBuffer inBuffer{ numChannels, frameSize, inData };
    IPLAudioBuffer outBuffer{ 2, frameSize, outData };

    IPLAmbisonicsBinauralEffectParams params{ hrtf, order };
    iplAmbisonicsBinauralEffectApply(effect, &params, &inBuffer, &outBuffer);

    if (!IsFinite(outBuffer))
    {
        printf("Non finite output for order [%d]\n", order);
        return false;
    }

    iplAmbisonicsBinauralEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    for (int i = 0; i < numChannels; ++i)
    {
        delete[] inData[i];
    }
    delete[] inData;

    for (int i = 0; i < 2; ++i)
    {
        delete[] outData[i];
    }

    return true;
}

TEST_CASE("Applying ambisonics binaural effects using default HRTF does not produce NANs.", "[AmbisonicsBinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 0));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 1));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 2));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 3));
}

TEST_CASE("Applying ambisonics binaural effects using default HRTF does not produce NANs. 1024 frame size.", "[AmbisonicsBinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 0));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 1));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 2));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 3));
}

#if !defined(IPL_OS_IOS) && !defined(IPL_OS_WASM)

TEST_CASE("Applying ambisonics binaural effects using SOFA HRTF does not produce NANs. D1.", "[AmbisonicsBinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 0));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 1));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 2));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 3));
}

TEST_CASE("Applying ambisonics binaural effects using SOFA HRTF does not produce NANs. D1. 1024 frame size.", "[AmbisonicsBinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 0));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 1));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 2));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 3));
}

TEST_CASE("Applying ambisonics binaural effects using SOFA HRTF does not produce NANs. H12.", "[AmbisonicsBinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_h12.sofa";
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 0));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 1));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 2));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 512, 3));
}

TEST_CASE("Applying ambisonics binaural effects using SOFA HRTF does not produce NANs. H12. 1024 frame size.", "[AmbisonicsBinauralEffect]")
{
    IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    hrtfParams.sofaFileName = "../../data/hrtf/sadie_h12.sofa";
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 0));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 1));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 2));
    REQUIRE(ValidateAmbisonicsBinauralEffect(hrtfParams, 1024, 3));
}

#endif
