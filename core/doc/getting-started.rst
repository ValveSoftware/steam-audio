Getting Started
===============

.. highlight:: c++

Requirements
------------

Steam Audio supports the following platforms:

-  Windows 7 or later (32-bit and 64-bit)
-  Linux (32-bit and 64-bit, tested with Ubuntu 18.04 LTS)
-  macOS 10.7 or later (64-bit Intel)
-  Android 5.0 or later (32-bit ARM, 64-bit ARM, 32-bit Intel, and 64-bit Intel)
-  iOS 11.0 or later (64-bit ARM)

Steam Audio supports the following C/C++ compilers on different platforms:

========    =============================================
Platform    Compiler Version
========    =============================================
Windows     Microsoft Visual Studio 2015 or later
Linux       GCC 4.8 or later, glibc 2.19 or later
macOS       Xcode 7 or later
Android     Android NDK r10e or later, Clang 3.6 or later
iOS         Xcode 14 or later
========    =============================================


Compiling and linking with Steam Audio
--------------------------------------

Before you can access any Steam Audio API functionality, you must point your compiler to the Steam Audio headers. To do this, add ``SDKROOT/include`` to your compiler's header file search path.

.. note::

    In all instructions on this page, ``SDKROOT`` refers to the directory in which you extracted the ``steamaudio.zip`` file.

Next, you must include the necessary header file::

    #include <phonon.h>

This will pull in all the Steam Audio API functions, and make them available for use in your program.

Finally, you must point your compiler to the Steam Audio libraries. This involves setting the library search path, and specifying the libraries to link against:

=====================   =============================   ===================
Platform                Library Directory               Library To Link
=====================   =============================   ===================
Windows 32-bit          ``SDKROOT/lib/windows-x86``     ``phonon.dll``
Windows 64-bit          ``SDKROOT/lib/windows-x64``     ``phonon.dll``
Linux 32-bit            ``SDKROOT/lib/linux-x86``       ``libphonon.so``
Linux 64-bit            ``SDKROOT/lib/linux-x64``       ``libphonon.so``
macOS                   ``SDKROOT/lib/osx``             ``libphonon.dylib``
Android ARMv7           ``SDKROOT/lib/android-armv7``   ``libphonon.so``
Android ARMv8/AArch64   ``SDKROOT/lib/android-armv8``   ``libphonon.so``
Android x86             ``SDKROOT/lib/android-x86``     ``libphonon.so``
Android x64             ``SDKROOT/lib/android-x64``     ``libphonon.so``
iOS ARMv8/AArch64       ``SDKROOT/lib/ios``             ``libphonon.a``
=====================   =============================   ===================

For more instructions on how to configure search paths and link libraries in your development environment, refer to the documentation for your compiler or IDE.


Example: Spatializing an audio clip
-----------------------------------

This example shows how to use the Steam Audio SDK to apply HRTF-based 3D audio to a mono (single-channel) audio clip.

To avoid complicating this example with code related to graphical rendering, audio engines, and codecs, this example is a C++ command-line tool that loads audio data from a file, applies 3D audio effects to it, and saves it to another file.

Before using any Steam Audio functionality, include the necessary headers::

    #include <phonon.h>

Initialization: Context
~~~~~~~~~~~~~~~~~~~~~~~

The first step in initializing Steam Audio is to create a *context* object. This object keeps track of various optional global settings of Steam Audio, and will be used to create all other Steam Audio API objects.

To create a context object, first initialize an ``IPLContextSettings`` structure with the settings we want to use when creating the context::

    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;

The only setting we need to specify is ``version``. This is the version of the Steam Audio API that your program is compiled with, and will typically be set to ``STEAMAUDIO_VERSION``. There are other optional settings that you can specify, such as custom memory allocation functions that Steam Audio should use whenever it needs to allocate or free memory. For the purposes of this example, though, we'll leave everything at their default (zero) values.

.. note::

    Creating any Steam Audio object follows the same pattern: first, you set up a structure with all the relevant settings for the object, then you call a function to create the object.

Next, we create the context by calling ``iplContextCreate``::

    IPLContext context = nullptr;
    iplContextCreate(&contextSettings, &context);

Here, ``context`` is a handle to the Steam Audio context object. You will typically create a single context, which will persist throughout the lifetime of your application.


Initialization: HRTF
~~~~~~~~~~~~~~~~~~~~

After creating the context, we need to load Head-Related Transfer Function (HRTF) data. An HRTF is a set of filters that is applied to audio in order to spatialize it. Steam Audio provides a default HRTF, and that's what we'll use for this example::

    IPLHRTFSettings hrtfSettings{};
    hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

We also need to specify some key properties that define how we will process audio. All audio processing in Steam Audio is done using uncompressed Pulse Code Modulated (PCM) audio data. Audio signals are represented as a series of *samples* measured at discrete, regularly-spaced points in time. Multi-channel audio is represented as multiple audio signals: for example, stereo (2-channel) audio contains 2 audio signals, so there are 2 samples for any given point in time.

The audio processing properties we need are:

    -   **Sampling rate.** This is the frequency (in Hz) at which audio data is sampled. Typical sampling rates on most platforms are 44100 Hz (CD quality) or 48000 Hz.

    -   **Frame size**. Most audio engines process audio in *frames* (also known as *chunks* or *buffers*). The frame size is the number of samples in a single frame of a single channel of audio. Typical values are 512 or 1024 samples per frame.

We specify these properties using a separate structure, since it will be needed in multiple places::

    IPLAudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

Now, we create the HRTF::

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &audioSettings, &hrtfSettings, &hrtf);

This loads and initializes Steam Audio's default HRTF. You can also use custom HRTFs that are loaded from SOFA files. For more information, see :ref:`ref_guide_sofa`.

You will typically load one or more HRTFs, which will persist throughout the lifetime of your application.


Initialization: Binaural Effect
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The actual work of spatializing an audio signal is performed by a *binaural effect*. This is an object that contains all the state that must persist from one audio frame to the next, for a single audio source.

As usual, we first populate a settings structure::

    IPLBinauralEffectSettings effectSettings{};
    effectSettings.hrtf = hrtf;

The only setting we need to specify is the HRTF that we want to use for spatialization. Now we can create the binaural effect::

    IPLBinauralEffect effect = nullptr;
    iplBinauralEffectCreate(context, &audioSettings, &effectSettings, &effect);

We are now ready to spatialize some audio! But first, we need to load our audio signal and allocate some buffers for audio processing.


Initialization: Audio Buffers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To avoid dealing with codecs and audio file loaders in this sample application, we assume that the input audio data is stored in a "raw" audio file. This file contains nothing but the contents of the input audio signal, stored in PCM format with 32-bit single-precision floating point samples. You can use free tools like `Audacity <http://www.audacityteam.org/>`_ to create and listen to such audio files.

::

    std::vector<float> inputaudio = load_input_audio("inputaudio.raw");

This loads the audio data from the file inputaudio.raw and stores it in an ``std::vector<float>``. For more details on how the ``load_input_audio`` function is implemented, see the complete code listing at the end of this page.

We also create a ``std::vector<float>`` to store the final output data::

    std::vector<float> outputaudio;

Next, we create ``IPLAudioBuffer`` structures that define audio buffers that will be used as input to and output from the binaural effect. First, the input buffer::

    float* inData[] = { inputaudio.data() };

    IPLAudioBuffer inBuffer{};
    inBuffer.numChannels = 1;
    inBuffer.numSamples = audioSettings.frameSize;
    inBuffer.data = inData;

The input buffer contains 1 channel, and the number of samples per channel is equal to the frame size. Audio buffers in Steam Audio are always *deinterleaved*, i.e., the samples for the first channel are stored contiguously in one array, the samples for the second channel are store contiguously in a second array, and so on. In the ``IPLAudioBuffer`` structure, the ``data`` field should point to an array of pointers, each of which points to the data for a single channel. Here, the input buffer points to the first 1024 samples of data loaded from the input file.

For the output buffer, we need to allocate memory for 1024 samples of 2-channel data, and set up an ``IPLAudioBuffer`` structure to point to it. We can do this using the ``iplAudioBufferAllocate`` function::

    IPLAudioBuffer outBuffer{};
    iplAudioBufferAllocate(context, 2, audioSettings.frameSize, &outBuffer);

This allocates a deinterleaved audio buffer that can store a single frame of stereo audio. Since we'll want to save *interleaved* audio to disk, we need to allocate memory for that, too::

    std::vector<float> outputaudioframe(2 * audioSettings.frameSize);


Main Loop
~~~~~~~~~

The main processing loop applies 3D audio effects to the input audio one frame at a time, accumulating the results in an output buffer that will be written to disk at the end of the loop.

We first calculate the number of audio frames that we will be processing::

    int numframes = inputaudio.size() / audioSettings.frameSize;

The processing loop is run ``numframes`` times::

    for (int i = 0; i < numframes; ++i)
    {
        // render a frame of spatialized audio and append to the end of outputaudio
        // ...

        // advance the input to the next frame
        inData[0] += audioSettings.frameSize;
    }

Inside the loop, rendering a frame of spatialized audio involves the following steps. First, we use the binaural effect to spatialize the input buffer, storing the results in the deinterleaved output buffer ``outBuffer``::

    IPLBinauralEffectParams effectParams{};
    effectParams.direction = IPLVector3{1.0f, 1.0f, 1.0f};
    effectParams.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
    effectParams.spatialBlend = 1.0f;
    effectParams.hrtf = hrtf;
    effectParams.peakDelays = nullptr;

    iplBinauralEffectApply(effect, &effectParams, inBuffer, outBuffer);

All audio processing effects in Steam Audio follow a similar pattern: you populate a structure containing various parameters for the effect, then you call a function to apply the effect to an audio buffer. For the binaural effect, the parameters are:

-   ``direction`` The direction vector from the listener to the source, relative to the listener's coordinates.
-   ``interpolation`` How to estimate the HRTF to use when the source direction doesn't correspond to any direction for which the HRTF data contains any measurements. ``IPL_HRTFINTERPOLATION_NEAREST`` specifies *nearest-neighbor* interpolation, which just picks the closest direction for which the HRTF contains measured data. This is the most efficient option, but you can also use *bilinear* interpolation for smoother rendering of moving sources. In our case, since the source doesn't move, nearest-neighbor is fine.
-   ``spatialBlend`` This parameter lets you blend between spatialized and unspatialized audio. If its value is 1 (as here), the output will be fully spatialized. If its value is 0, the output will be unspatialized (i.e., identical to the input). Intermediate values result in partial spatialization.
-   ``hrtf`` This is the HRTF to use for spatializing the input. Here, we set it to the same value we passed in the ``IPLBinauralEffectSettings`` structure when creating the effect. Steam Audio lets you change the HRTF on the fly, and the HRTF you pass in the ``IPLBinauralEffectParams`` structure is always the one used for rendering.

Next, we interleave the audio buffer::

    iplAudioBufferInterleave(context, outBuffer, outputaudioframe.data());

At this point, ``outputaudioframe`` contains a single frame of interleaved stereo audio. We then append this to ``outputaudio``, the buffer that will be written to disk::

    std::copy(std::begin(outputaudioframe), std::end(outputaudioframe), std::back_inserter(outputaudio));

Once the loop finishes, we write this buffer to disk::

    save_output_audio("outputaudio.raw", outputaudio);

For details on how ``save_output_audio`` is implemented, see the complete code listing below.


Cleanup
~~~~~~~

Finally, before our program exits, we clean up all the objects we created using the Steam Audio API::

    iplAudioBufferFree(context, &outBuffer);
    iplBinauralEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

And that completes this example program for spatializing an audio file using Steam Audio! For a full listing of this example program, see below.

Full program listing
--------------------

::

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

        IPLHRTFSettings hrtfSettings;
        hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

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
