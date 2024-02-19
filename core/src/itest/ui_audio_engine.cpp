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

#include "ui_audio_engine.h"

#include <windows.h>

#define DRWAV_IMPLEMENTATION
#include <dr_wav.h>

namespace ipl {

const std::string UIAudioEngine::kAudioClipsDirectory{"../../data/audio/"};

UIAudioEngine::UIAudioEngine(int samplingRate,
							 int frameSize,
							 AudioCallback audioCallback,
							 AudioTailCallback tailCallback /* = nullptr */)
	: mAudioCallback(audioCallback)
	, mTailCallback(tailCallback)
	, mPlayCursor(0)
	, mInputBuffer(2, frameSize)
	, mOutputBuffer(2, frameSize)
	, mPlaying(false)
	, mNumTailCalls(0)
{
	Pa_Initialize();
	Pa_OpenDefaultStream(&mStream, 0, 2, paFloat32, samplingRate, frameSize, processAudio, this);
	Pa_SetStreamFinishedCallback(mStream, onStreamFinished);

	auto findData = WIN32_FIND_DATAA{};
	auto findHandle = FindFirstFileA((kAudioClipsDirectory + "*.wav").c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE)
	{
		printf("WARNING: No audio clips found when searching: %s.\n", kAudioClipsDirectory.c_str());
		return;
	}

	do
	{
		mAudioClips.push_back(findData.cFileName);
	}
	while (FindNextFileA(findHandle, &findData));

	FindClose(findHandle);
	printf("Found %zd audio clips.\n", mAudioClips.size());

	for (auto i = 0; i < mAudioClips.size(); ++i)
	{
		mAudioClipsStr.push_back(mAudioClips[i].c_str());
	}
}

UIAudioEngine::~UIAudioEngine()
{
	if (mPlaying)
		Pa_StopStream(mStream);

	Pa_CloseStream(mStream);
	Pa_Terminate();
}

void UIAudioEngine::play(int index)
{
	if (mPlaying)
		Pa_StopStream(mStream);

	auto fileName = kAudioClipsDirectory + mAudioClips[index];

	unsigned int numChannels = 0;
	unsigned int samplingRate = 0;
	drwav_uint64 numSamples = 0;
	auto samples = drwav_open_file_and_read_pcm_frames_f32(fileName.c_str(), &numChannels, &samplingRate, &numSamples, nullptr);
	mAudioClip = make_unique<AudioBuffer>(numChannels, static_cast<int>(numSamples));
	mAudioClip->write(samples);
	drwav_free(samples, nullptr);
	mPlayCursor = 0;

	Pa_StartStream(mStream);

	mPlaying = true;
	mNumTailCalls = 0;
}

void UIAudioEngine::stop()
{
	mPlaying = false;
}

int UIAudioEngine::processAudio(const void* input,
								void* output,
								unsigned long frameCount,
								const PaStreamCallbackTimeInfo* timeInfo,
								PaStreamCallbackFlags statusFlags,
								void* userData)
{
	auto audioEngine = reinterpret_cast<UIAudioEngine*>(userData);

	if (audioEngine->mPlaying)
    {
        if (audioEngine->mAudioClip->numChannels() == 1)
        {
            for (auto i = 0u; i < frameCount; ++i)
            {
                audioEngine->mInputBuffer[0][i] = (*audioEngine->mAudioClip)[0][audioEngine->mPlayCursor];
                audioEngine->mInputBuffer[1][i] = (*audioEngine->mAudioClip)[0][audioEngine->mPlayCursor];

                audioEngine->mPlayCursor = (audioEngine->mPlayCursor + 1) % audioEngine->mAudioClip->numSamples();
            }
        }
        else if (audioEngine->mAudioClip->numChannels() == 2)
        {
            for (auto i = 0u; i < frameCount; ++i)
            {
                audioEngine->mInputBuffer[0][i] = (*audioEngine->mAudioClip)[0][audioEngine->mPlayCursor];
                audioEngine->mInputBuffer[1][i] = (*audioEngine->mAudioClip)[1][audioEngine->mPlayCursor];

                audioEngine->mPlayCursor = (audioEngine->mPlayCursor + 1) % audioEngine->mAudioClip->numSamples();
            }
        }

        audioEngine->mAudioCallback(audioEngine->mInputBuffer, audioEngine->mOutputBuffer);

        audioEngine->mOutputBuffer.read(reinterpret_cast<float*>(output));

        return paContinue;
	}
	else
	{
		if (audioEngine->mTailCallback)
		{
			audioEngine->mNumTailCalls++;
			auto audioEffectState = audioEngine->mTailCallback(audioEngine->mOutputBuffer);
			audioEngine->mOutputBuffer.read(reinterpret_cast<float*>(output));

			if (audioEffectState == AudioEffectState::TailRemaining)
			{
				return paContinue;
			}
			else
			{
				printf("%s: tail complete after %d tail calls.\n", __FUNCTION__, audioEngine->mNumTailCalls);
				return paComplete;
			}
		}
		else
		{
			// no tail callback specified, so assume there's no tail
			return paComplete;
		}
	}
}

void UIAudioEngine::onStreamFinished(void* userData)
{
    auto audioEngine = reinterpret_cast<UIAudioEngine*>(userData);
	if (!audioEngine)
		return;

	Pa_StopStream(audioEngine->mStream);
}

}