#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

#include <phonon.h>

std::vector<float> load_input_audio(const std::string filename)
{
    std::ifstream file(filename.c_str(), std::ios::binary);

    file.seekg(0, std::ios::end);
    auto filesize = file.tellg();
    auto numsamples = static_cast<int>(filesize / sizeof(float));

    std::vector<float> inputaudio(numsamples);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(inputaudio.data()), filesize);

    return inputaudio;
}

void save_output_audio(const std::string filename, std::vector<float> outputaudio)
{
    std::ofstream file(filename.c_str(), std::ios::binary);
    file.write(reinterpret_cast<char*>(outputaudio.data()), outputaudio.size() * sizeof(float));
}

int main(int argc, char** argv)
{
    auto inputaudio = load_input_audio("inputaudio.raw");
    
    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;

    IPLContext context{};
    iplContextCreate(&contextSettings, &context);

    auto const samplingrate = 44100;
    auto const framesize    = 1024;
    IPLAudioSettings audioSettings{ samplingrate, framesize };

    IPLHRTFSettings hrtfSettings{};
    hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;
    hrtfSettings.volume = 1.0f;

    IPLHRTF hrtf{};
    iplHRTFCreate(context, &audioSettings, &hrtfSettings, &hrtf);

    IPLBinauralEffectSettings effectSettings;
    effectSettings.hrtf = hrtf;

    IPLBinauralEffect effect{};
    iplBinauralEffectCreate(context, &audioSettings, &effectSettings, &effect);

    std::vector<float> outputaudioframe(2 * framesize);
    std::vector<float> outputaudio;

    auto numframes = static_cast<int>(inputaudio.size() / framesize);
    float* inData[] = { inputaudio.data() };

    IPLAudioBuffer inBuffer{ 1, audioSettings.frameSize, inData };
    
    IPLAudioBuffer outBuffer;
    iplAudioBufferAllocate(context, 2, audioSettings.frameSize, &outBuffer);

    for (auto i = 0; i < numframes; ++i)
    {
        IPLBinauralEffectParams params;
        params.direction = IPLVector3{ 1.0f, 1.0f, 1.0f };
        params.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
        params.spatialBlend = 1.0f;
        params.hrtf = hrtf;
        params.peakDelays = nullptr;

        iplBinauralEffectApply(effect, &params, &inBuffer, &outBuffer);

        iplAudioBufferInterleave(context, &outBuffer, outputaudioframe.data());

        std::copy(std::begin(outputaudioframe), std::end(outputaudioframe), std::back_inserter(outputaudio));

        inData[0] += audioSettings.frameSize;
    }

    iplAudioBufferFree(context, &outBuffer);
    iplBinauralEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    save_output_audio("outputaudio.raw", outputaudio);
    return 0;
}
