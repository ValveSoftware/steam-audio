//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <string.h>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include <phonon.h>
#include "steamaudio_fmod.h"

template <typename T>
class WorkerThread {
public:
    WorkerThread() {
        thread = std::thread(runTasks, this);
    }
    
    ~WorkerThread() {
        cancel = true;
        mutex.lock();
        condvar.notify_all();
        mutex.unlock();
        thread.join();
    }

    std::future<T> addTask(std::function<T(void)> function) {
        std::lock_guard<std::mutex> lock(mutex);
        std::packaged_task<T(void)> task(function);
        auto future = task.get_future();
        tasks.push(std::move(task));
        ready = true;
        condvar.notify_one();
        return future;
    }

private:
    static void runTasks(WorkerThread* state) {
        while (!state->cancel) {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->condvar.wait(lock, [state]() { return (state->ready || state->cancel); });
            state->ready = false;
            while (!state->tasks.empty()) {
                auto function = std::move(state->tasks.front());
                state->tasks.pop();
                function();
            }
        }
    }

    std::thread                             thread  {};
    std::queue<std::packaged_task<T(void)>> tasks   {};
    std::mutex                              mutex   {};
    std::condition_variable                 condvar {};
    std::atomic<bool>                       ready   { false };
    std::atomic<bool>                       cancel  { false };
};

struct BinauralRendererInfo
{
    IPLhandle               binauralRenderer;
    std::future<IPLhandle>  future;
    bool                    pending;

    BinauralRendererInfo()
        :
        binauralRenderer(nullptr),
        future(),
        pending(false)
    {}
};

/** Data shared by all effect instances created by the audio engine, across all scenes.
 */
class AudioEngineSettings
{
public:
    /** Initializes the audio engine global state. Must be called in the process callback of all effects before any
     *  other Steam Audio function is called. This function may be called repeatedly by different effects over
     *  different frames; initialization will be performed only the first time this function is called. This function
     *  must only be called from the audio thread.
     */
    AudioEngineSettings(const IPLRenderingSettings& renderingSettings,
                        const IPLAudioFormat&       audioFormat);

    /** Destroys the binaural renderer.
     */
    ~AudioEngineSettings();
    
    IPLhandle context() const;

    /** Returns the Rendering Settings object that describes the settings used by the audio engine.
     */
    IPLRenderingSettings renderingSettings() const;

    /** Returns the mixer output format used by the audio engine. This format is used for all audio effects in the
     *  mixer graph, regardless of where they are in the graph.
     */
    IPLAudioFormat outputFormat() const;

    /** Returns the binaural renderer used by the audio engine.
     */
    IPLhandle binauralRenderer();
    IPLhandle binauralRenderer(const int index) const;

    /** Returns the global Audio Engine Settings object.
     */
    static std::shared_ptr<AudioEngineSettings> get();

    /** Initializes a new Audio Engine Settings object.
     */
    static void create(const IPLRenderingSettings& renderingSettings, const IPLAudioFormat& audioFormat);

    static int addSOFAFile(const char* sofaFileName);
    static void removeSOFAFile(const char* sofaFileName);
    static void setCurrentSOFAFile(const int index);

    /** Destroys any existing Audio Engine Settings object.
     */
    static void destroy();

    //std::mutex mSOFAMutex;

private:
	static void queueSOFAFile(const char* sofaFileName);
	static int sofaFileIndex(const char* sofaFileName);
    static void createPendingBinauralRenderers();

    IPLhandle mContext;
    
    /** Rendering Settings that describe the settings used by the audio engine. */
    IPLRenderingSettings mRenderingSettings;

    /** Mixer output format used by the audio engine. */
    IPLAudioFormat mOutputFormat;

    WorkerThread<IPLhandle> mWorkerThread;

    /** Mutex for preventing concurrent accesses to the audio engine settings. */
    static std::mutex sMutex;

    /** The binaural renderers. */
    static std::vector<std::string> sSOFAFileNames;
    static std::unordered_map<std::string, BinauralRendererInfo> sBinauralRenderers;
    static int sCurrentSOFAFileIndex;

    /** Pointer to the shared Audio Engine Settings object used by all effects. */
    static std::shared_ptr<AudioEngineSettings> sAudioEngineSettings;
};

extern "C"
{
	/** Mini-API wrapper around GlobalState::destroy.
	*/
	F_EXPORT void F_CALL iplFmodResetAudioEngine();

	F_EXPORT int F_CALL iplFmodAddSOFAFileName(char* sofaFileName);

	F_EXPORT void F_CALL iplFmodRemoveSOFAFileName(char* sofaFileName);

	F_EXPORT void F_CALL iplFmodSetCurrentSOFAFile(int index);
}
