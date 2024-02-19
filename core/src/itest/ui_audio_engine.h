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

#include <portaudio.h>

#include <audio_buffer.h>
using namespace ipl;

namespace ipl {

typedef std::function<void(const AudioBuffer&, AudioBuffer&)> AudioCallback;
typedef std::function<AudioEffectState(AudioBuffer&)> AudioTailCallback;

class UIAudioEngine
{
public:
	static const std::string kAudioClipsDirectory;

	UIAudioEngine(int samplingRate,
				  int frameSize,
				  AudioCallback audioCallback,
				  AudioTailCallback tailCallback = nullptr);

	~UIAudioEngine();

	void play(int index);

	void stop();

	std::vector<std::string> mAudioClips;
	std::vector<const char*> mAudioClipsStr;

private:
	PaStream* mStream;
	AudioCallback mAudioCallback;
	AudioTailCallback mTailCallback;
	unique_ptr<AudioBuffer> mAudioClip;
	int mPlayCursor;
	AudioBuffer mInputBuffer;
	AudioBuffer mOutputBuffer;
	bool mPlaying;
	int mNumTailCalls;

	static int processAudio(const void* input,
							void* output,
							unsigned long frameCount,
							const PaStreamCallbackTimeInfo* timeInfo,
							PaStreamCallbackFlags statusFlags,
							void* userData);

	static void onStreamFinished(void* userData);
};

}