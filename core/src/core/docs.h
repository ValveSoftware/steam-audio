    /*****************************************************************************************************************/
    /* Documentation                                                                                                 */
    /*****************************************************************************************************************/

    /** \mainpage
     *  Steam Audio is a software tool that offers a full-featured, end-to-end spatial audio solution for games and VR.
     *  It renders accurate positional audio using Head-Related Transfer Functions (HRTFs), and uses physics-based
     *  sound propagation to create reverb, reflection, and occlusion effects that recreate how sounds are affected by
     *  the virtual environment. Sound propagation and reverb effects can be calculated on-the-fly during gameplay,
     *  or during level design as part of a process called baking; using baked propagation effects lets you reduce the
     *  CPU overhead at runtime, at the cost of increased memory usage.
     *
     *  The Steam Audio C API lets you integrate Steam Audio into applications built using custom game engines or
     *  audio middleware. Steam Audio supports several game engines and audio middleware out of the box; for a full
     *  list refer to the User's Guide. If your application uses a game engine or audio middleware that is not on the
     *  supported list, you will need to use this C API.
     *
     *  This document describes the concepts required to use the Steam Audio C API, and provides a complete API
     *  reference.
     *
     *  - \subpage page_quickstart <br/>
     *  With Steam Audio, it only takes a few lines of code to apply HRTF-based binaural rendering to an audio clip.
     *  This page describes this process and provides code samples. <br/>&nbsp;
     *
     *  - \subpage page_compiling <br/>
     *  This page describes the C/C++ compiler and linker settings you need to build Steam Audio into your
     *  application. <br/>&nbsp;
     *
     *  - \subpage page_integration <br/>
     *  This page provides an overview of all the concepts you need to understand in order to integrate Steam Audio
     *  into your application, and describes the various parts of your game engine, audio engine, and tools where you
     *  will need to call Steam Audio API functions. <br/>&nbsp;
     *
     *  - \subpage page_tan <br/>
     *  This page provides an overview of how to use AMD TrueAudio Next via the Steam Audio C API. <br/>&nbsp;
     *
     *  - \subpage page_embree <br/>
     *  This page provides an overview of how to use Intel Embree via the Steam Audio C API. <br/>&nbsp;
     *
     *  - \subpage page_radeonrays <br/>
     *  This page provides an overview of how to use AMD Radeon Rays via the Steam Audio C API.
     */

    /** \page page_quickstart Quick Start: Rendering 3D Audio
     *  \tableofcontents
     *
     *  This page provides an example of using Steam Audio for applying HRTF-based 3D audio to mono (single-channel)
     *  audio data. To avoid complicating this example with code related to graphical rendering, audio engines,
     *  and codecs, this example is a C++ command-line tool that loads audio data from a file, applies 3D audio effects
     *  to it, and saves it to another file.
     *
     *  \section quickstart_exploring The Sample Application
     *  Before using any Steam Audio functionality, we must include the necessary headers:
     *
     *  ```cpp
     *   #include <phonon.h>
     *  ```
     *
     *  The rest of this section describes each step that must be carried out for this program to load audio data,
     *  initialize Steam Audio, and use Steam Audio for 3D audio processing.
     *
     *  \subsection quickstart_exploring_loading Loading Input Audio
     *  To avoid dealing with codecs and audio file loaders in this sample application, we assume that the input audio
     *  data is stored in a "raw" audio file. This file contains nothing but the contents of the input audio signal,
     *  stored in PCM format with 32-bit single-precision floating point samples. You can use free tools like
     *  [Audacity](http://www.audacityteam.org/) to create and listen to such audio files.
     *
     *  ```cpp
     *       std::vector<float> inputaudio = load_input_audio("inputaudio.raw");
     *  ```
     *
     *  This loads the audio data from the file `inputaudio.raw` and stores it in an `std::vector<float>`. For more
     *  details on how the `load_input_audio` function is implemented, see the complete code listing at the end of
     *  this page.
     *
     *  \subsection quickstart_exploring_init Steam Audio Initialization
     *  The first step in initializing Steam Audio is specifying a context object. This is mainly intended for
     *  applications that need to override Steam Audio's logging system (to display log messages in an editor window,
     *  for example) or memory allocation system (to use a custom allocator, for example). Since we don't need any of
     *  this functionality, we can just set everything to `nullptr`:
     *
     *  ```cpp
     *       IPLhandle context{nullptr};
     *       iplContextCreate(nullptr, nullptr, nullptr, &context);
     *  ```
     *
     *  The next step is to specify the _sampling rate_ and _frame size_ of our audio processing code. We assume that
     *  input and output audio is sampled at 44.1 kHz, and that audio is processed in frames of 1024 samples:
     *
     *  ```cpp
     *       auto const samplingrate = 44100;
     *       auto const framesize    = 1024;
     *       IPLRenderingSettings settings{ samplingrate, framesize };
     *  ```
     *
     *  See \ref IPLRenderingSettings for details.
     *
     *  \subsection quickstart_exploring_renderer Binaural Renderer
     *  The next step is to create a \ref binauralrenderer object:
     *
     *  ```cpp
     *       IPLhandle renderer{ nullptr };
     *       IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, 0, nullptr, nullptr, nullptr };
     *       iplCreateBinauralRenderer(context, settings, hrtfParams, &renderer);
     *  ```
     *
     *  The `IPLhandle` type is used for opaque handles to various objects created using the Steam Audio API. The
     *  Binaural Renderer object is must not be destroyed until all 3D audio processing is complete. See
     *  \ref iplCreateBinauralRenderer for details. The `IPLHrtfParams` structure lets you specify custom HRTF data;
     *  we will not be using this feature in this sample program, so we initialize it to default values.
     *
     *  \subsection quickstart_exploring_format Audio Formats
     *  Next, we spell out the audio formats that we will be using for our input and output signals. The input is a
     *  single-channel mono file, whereas the output is a 2-channel stereo file, since it contains data that has been
     *  rendered using HRTF-based binaural rendering:
     *
     *  ```cpp
     *       IPLAudioFormat mono;
     *       mono.channelLayoutType  = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
     *       mono.channelLayout      = IPL_CHANNELLAYOUT_MONO;
     *       mono.channelOrder       = IPL_CHANNELORDER_INTERLEAVED;
     *
     *       IPLAudioFormat stereo;
     *       stereo.channelLayoutType  = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
     *       stereo.channelLayout      = IPL_CHANNELLAYOUT_STEREO;
     *       stereo.channelOrder       = IPL_CHANNELORDER_INTERLEAVED;
     *  ```
     *
     *  Both the input and output formats are specified as _interleaved_: this means that data for all channels for
     *  the first sample is stored contiguously, followed by data for all channels for the second sample, and so on.
     *  See \ref IPLAudioFormat for details.
     *
     *  \subsection quickstart_exploring_effect Binaural Effect
     *  Next, we create an \ref binauraleffect object, which is used to apply 3D audio effects to a specific, single
     *  source:
     *
     *  ```cpp
     *       IPLhandle effect{ nullptr };
     *       iplBinauralEffectCreate(renderer, mono, stereo, &effect);
     *  ```
     *
     *  Here, we specify that the input to the Effect object will be mono, and the output will be stereo. See
     *  \ref iplBinauralEffectCreate for details.
     *  
     *  \subsection quickstart_exploring_buffer Audio Buffers
     *  Next, we define \ref IPLAudioBuffer objects that represent the input and output audio buffers. First, the
     *  input buffer, which represents one frame of input data:
     *
     *  ```cpp
     *       IPLAudioBuffer inbuffer{ mono, framesize, inputaudio.data() };
     *  ```
     *
     *  The input buffer points to the start of the data loaded from the input file. This way, the first input audio
     *  frame is set to the first 1024 samples of the input data.
     *
     *  Second, we specify the output buffer:
     *
     *  ```cpp
     *       std::vector<float> outputaudioframe(2 * framesize);
     *       IPLAudioBuffer outbuffer{ stereo, framesize, outputaudioframe.data() };
     *  ```
     *
     *  We first create a `std::vector<float>` that can hold 1024 samples of stereo data, then create an IPLAudioBuffer
     *  object that points to the data contained in the `std::vector<float>`.
     *
     *  \subsection quickstart_exploring_apply Main Processing Loop
     *  The main processing loop applies 3D audio effects to the input audio one frame at a time, accumulating the
     *  results in an output buffer that will be written to disk at the end of the loop. First, we create a
     *  `std::vector<float>` to store the final output data:
     *
     *  ```cpp
     *       std::vector<float> outputaudio;
     *  ```
     *
     *  We then calculate the number of audio frames that we will be processing:
     *
     *  ```cpp
     *       auto numframes = static_cast<int>(inputaudio.size() / framesize);
     *  ```
     *
     *  Finally, we run the processing loop `numframes` times:
     *
     *  ```cpp
     *       for (auto i = 0; i < numframes; ++i)
     *       {
     *           iplBinauralEffectApply(effect, inbuffer, IPLVector3{ 1.0f, 1.0f, 1.0f }, IPL_HRTFINTERPOLATION_NEAREST, outbuffer);
     *           std::copy(std::begin(outputaudioframe), std::end(outputaudioframe), std::back_inserter(outputaudio));
     *           inbuffer.interleavedBuffer += framesize;
     *       }
     *  ```
     *
     *  The loop consists of three steps:
     *
     *  1.  We call \ref iplBinauralEffectApply to tell Steam Audio to render the input buffer using HRTF-based
     *      binaural rendering. We use a fixed direction from the listener to the source, (1, 1, 1): the source is
     *      above, behind, and to the right of the listener. Since the source does not move relative to the listener,
     *      we use nearest-neighbor filtering for maximum performance.
     *  2.  We append the output frame to `outputaudio`, the buffer that will be written to disk.
     *  3.  We advance the input audio by a single frame, by advancing the input buffer's `interleavedBuffer` pointer
     *      by 1024 samples.
     *
     *  \subsection quickstart_exploring_destroy Cleanup
     *  Once we've finished using Steam Audio, we must destroy the objects created using the Steam Audio API, and
     *  perform last-minute cleanup:
     *
     *  ```cpp
     *       iplBinauralEffectRelease(&effect);
     *       iplDestroyBinauralRenderer(&renderer);
     *       iplContextRelease(&context);
     *       iplCleanup();
     *  ```
     *
     *  The last step is to write the processed audio to disk:
     *
     *  ```cpp
     *       save_output_audio("outputaudio.raw", outputaudio);
     *  ```
     *
     *  For details on how `save_output_audio` is implemented, see the complete code listing below.
     *
     *  \section quickstart_complete Complete Code Listing
     *  A complete code listing of the sample application follows. See \ref page_compiling for more information on how
     *  to compile and run this code in your development environment.
     *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~cpp
     *   #include <algorithm>
     *   #include <fstream>
     *   #include <iterator>
     *   #include <vector>
     *
     *   #include <phonon.h>
     *
     *   std::vector<float> load_input_audio(const std::string filename)
     *   {
     *       std::ifstream file(filename.c_str(), std::ios::binary);
     *
     *       file.seekg(0, std::ios::end);
     *       auto filesize = file.tellg();
     *       auto numsamples = static_cast<int>(filesize / sizeof(float));
     *
     *       std::vector<float> inputaudio(numsamples);
     *       file.seekg(0, std::ios::beg);
     *       file.read(reinterpret_cast<char*>(inputaudio.data()), filesize);
     *
     *       return inputaudio;
     *   }
     *
     *   void save_output_audio(const std::string filename, std::vector<float> outputaudio)
     *   {
     *       std::ofstream file(filename.c_str(), std::ios::binary);
     *       file.write(reinterpret_cast<char*>(outputaudio.data()), outputaudio.size() * sizeof(float));
     *   }
     *
     *   int main(int argc, char** argv)
     *   {
     *       auto inputaudio = load_input_audio("inputaudio.raw");
     *
     *       IPLhandle context{nullptr};
     *       iplContextCreate(nullptr, nullptr, nullptr, &context);
     *
     *       auto const samplingrate = 44100;
     *       auto const framesize    = 1024;
     *       IPLRenderingSettings settings{ samplingrate, framesize };
     *
     *       IPLhandle renderer{ nullptr };
     *       IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, 0, nullptr, nullptr, nullptr };
     *       iplCreateBinauralRenderer(context, settings, hrtfParams, &renderer);
     *
     *       IPLAudioFormat mono;
     *       mono.channelLayoutType  = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
     *       mono.channelLayout      = IPL_CHANNELLAYOUT_MONO;
     *       mono.channelOrder       = IPL_CHANNELORDER_INTERLEAVED;
     *
     *       IPLAudioFormat stereo;
     *       stereo.channelLayoutType  = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
     *       stereo.channelLayout      = IPL_CHANNELLAYOUT_STEREO;
     *       stereo.channelOrder       = IPL_CHANNELORDER_INTERLEAVED;
     *
     *       IPLhandle effect{ nullptr };
     *       iplBinauralEffectCreate(renderer, mono, stereo, &effect);
     *
     *       std::vector<float> outputaudioframe(2 * framesize);
     *       std::vector<float> outputaudio;
     *
     *       IPLAudioBuffer inbuffer{ mono, framesize, inputaudio.data() };
     *       IPLAudioBuffer outbuffer{ stereo, framesize, outputaudioframe.data() };
     *
     *       auto numframes = static_cast<int>(inputaudio.size() / framesize);
     *       for (auto i = 0; i < numframes; ++i)
     *       {
     *           iplBinauralEffectApply(effect, inbuffer, IPLVector3{ 1.0f, 1.0f, 1.0f }, IPL_HRTFINTERPOLATION_NEAREST, outbuffer);
     *           std::copy(std::begin(outputaudioframe), std::end(outputaudioframe), std::back_inserter(outputaudio));
     *           inbuffer.interleavedBuffer += framesize;
     *       }
     *
     *       iplBinauralEffectRelease(&effect);
     *       iplDestroyBinauralRenderer(&renderer);
     *       iplContextRelease(&context);
     *       iplCleanup();
     *
     *       save_output_audio("outputaudio.raw", outputaudio);
     *       return 0;
     *   }
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     *
     */

    /** \page page_compiling Compiling and Linking with Steam Audio
     *  This page describes the C/C++ compiler and linker settings you need to build Steam Audio into your
     *  application.
     *
     *  ## Compiler Settings
     *  Before you can access any Steam Audio API functionality, you must point your compiler to the Steam Audio
     *  headers. To do this, add `SDKROOT/include` to your compiler's header file search path.
     *
     *  > **Note** <br/>
     *  > In all instructions on this page, `SDKROOT` refers to the directory in which you extracted the
     *  > Steam Audio C API .zip file.
     *
     *  Next, you must include the necessary header file:
     *
     *  ```
     *  #include <phonon.h>
     *  ```
     *
     *  This directive will pull in all the Steam Audio API functions, and make them available for use in your
     *  program.
     *
     *  ## Linker Settings
     *  Finally, you must point your compiler to the Steam Audio libraries. This involves setting the library
     *  search path, and specifying the libraries to link against:
     *
     *  Platform        | Library Directory           | Library To Link
     *  --------        | -----------------           | ---------------
     *  Windows 32-bit  | `SDKROOT/lib/Windows/x86`   | `phonon.dll`
     *  Windows 64-bit  | `SDKROOT/lib/Windows/x64`   | `phonon.dll`
     *  Linux 32-bit    | `SDKROOT/lib/Linux/x86`     | `libphonon.so`
     *  Linux 64-bit    | `SDKROOT/lib/Linux/x64`     | `libphonon.so`
     *  macOS universal | `SDKROOT/lib/OSX`           | `libphonon.dylib`
     *  Android ARMv7   | `SDKROOT/lib/Android/armv7` | `libphonon.so`
     *  Android ARM64   | `SDKROOT/lib/Android/arm64` | `libphonon.so`
     *  Android x86     | `SDKROOT/lib/Android/x86`   | `libphonon.so`
     *  Android x64     | `SDKROOT/lib/Android/x64`   | `libphonon.so`
     *
     *  For more instructions on how to configure search paths and link libraries in your development environment,
     *  refer to the documentation for your compiler or IDE.
     *
     *  ## Supported Build Tools
     *  Steam Audio supports the following versions of C/C++ compilers on different platforms:
     *
     *  Platform | Minimum Compiler Version
     *  -------- | ------------------------
     *  Windows  | Microsoft Visual Studio 2013 or later
     *  Linux    | GCC 4.8 or later
     *  macOS    | Xcode 6 or later
     *  Android  | Android NDK r10e or later, Clang 3.6 or later
     *
     */

    /** \page page_integration Integration Guide
     *  To integrate Steam Audio into your game, VR application, or toolset, you must carry out the following broad
     *  tasks:
     *
     *  - **Integrate Steam Audio into your audio engine.** This may be either the audio functionality built into your
     *    game engine, or third-party audio middleware. This step allows Steam Audio to process various parts of the
     *    audio played by your application, and apply the necessary 3D audio and sound propagation effects.
     *
     *  - **Integrate Steam Audio into your game engine.** This allows your game engine to tell Steam Audio what the
     *    environment looks like, what materials various objects are made of, what sound propagation data has been
     *    baked (if any), etc. If you are not using sound propagation, reverb, or occlusion features, you may skip
     *    this step.
     *
     *  The following subtopics discuss the above steps in greater detail.
     *
     *  - \subpage page_audio_pipeline <br/>
     *  Describes the various stages of processing that can be applied to a sound by Steam Audio, and the resulting
     *  signal flow through the audio engine's rendering pipeline. Understanding these concepts will help you decide
     *  where to insert DSP plugins/effects that call into the Steam Audio API, and what Steam Audio features may be
     *  unavailable due to limitations of your audio engine. <br/>&nbsp;
     *
     *  - \subpage page_audio_engine <br/>
     *  Provides an overview of the Steam Audio API objects and functions that the audio engine can use to apply
     *  Steam Audio effects to audio data. Includes how to pass audio buffers to and from Steam Audio, using HRTF
     *  rendering, Ambisonics, and applying sound propagation effects. <br/>&nbsp;
     *
     *  - \subpage page_game_editor <br/>
     *  Discusses the components/functionality that must be added to your game engine's editor in order to create
     *  necessary data for use by Steam Audio. This includes exporting scene data, materials, and baked sound
     *  propagation and reverb to Steam Audio. <br/>&nbsp;
     *
     *  - \subpage page_game_engine <br/>
     *  Provides an overview of the Steam Audio API objects and functions that the game engine can use to send
     *  scene, material, and baked data information to the audio engine. The audio engine then uses the Steam Audio
     *  API to apply appropriate 3D audio and sound propagation effects.
     *
     */

    /** \page page_audio_pipeline Steam Audio Pipeline
     *  \tableofcontents
     *
     *  This page describes the ways in which audio data can flow through your audio engine when integrated with
     *  Steam Audio. Understanding the various possible sequences of Steam Audio operations that can be applied to
     *  audio data will help you decide how best to implement the integration with your audio engine. In the following
     *  discussion, the sound source is assumed to be a mono (single-channel) point source; Steam Audio automatically
     *  downmixes multi-channel source audio to mono before proceeding.
     *
     *  \section pipeline_source Applying Steam Audio Effects to Individual Sources
     *  This section discusses the typical audio pipeline when using Steam Audio to apply 3D audio and sound
     *  propagation effects to individual sources. This is the most feature-rich and flexible way of using Steam
     *  Audio, but it involves a CPU overhead that depends on the number of sources that use Steam Audio.
     *
     *  \image html ./pipeline_source.png "Audio signal flow when applying Steam Audio effects to individual sources."
     *
     *  \subsection pipeline_source_direct Direct Sound Path
     *  The simplest uses of Steam Audio involve adding effects to the _direct sound path_: recreating how sound
     *  travels from the source to the listener, without accounting for reflected or diffracted sound. The following
     *  effects can be applied to the direct sound path, in order:
     *
     *  \subsubsection pipeline_source_direct_attenuation Attenuation
     *  This models how the amplitude of a sound source is attenuated over distance, as it travels from the source to
     *  the listener. Steam Audio uses a physics-based attenuation model, with the amplitude decaying proportionally
     *  to \f$ 1/r \f$, where \f$r\f$ is the distance between the source and the listener. See \ref directsound for
     *  details.
     *
     *  \subsubsection pipeline_source_direct_occlusion Occlusion
     *  This models how sound is occluded by solid objects. Steam Audio supports multiple occlusion algorithms, see
     *  \ref directsound for details. Modeling occlusion requires that the game engine send scene
     *  information to the audio engine; see \ref page_game_engine for details.
     *
     *  \subsubsection pipeline_source_direct_hrtf Binaural Rendering
     *  This spatializes the sound based on the direction from the listener to the source. The spatialization is
     *  performed using HRTFs; the output of this step is a 2-channel (stereo) buffer that is best listened to using
     *  headphones. See \ref binauraleffect for details.
     *
     *  \subsection pipeline_source_indirect Indirect Sound
     *  Steam Audio can simulate how sounds reflect off of solid objects, leading to effects like echoes and
     *  reverberation. These effects are collectively termed _indirect sound_. The following is the typical flow of
     *  audio data when indirect sound effects are applied:
     *
     *  \subsubsection pipeline_source_indirect_convolution Sound Propagation
     *  This is the main processing stage where sound propagation effects are applied. This involves applying a
     *  source-dependent convolution reverb to the sound emitted by the source, which accounts for all the interactions
     *  between the sound and the environment as the sound flows from the source to the listener. The output of this
     *  step is a multi-channel Ambisonics buffer that must be decoded in software or hardware. See \ref conveffect for
     *  details.
     *
     *  \subsubsection pipeline_source_indirect_ambisonics Ambisonics Rendering
     *  Some audio engines may be able to process Ambisonics audio as-is, using a built-in software decoder or an
     *  external hardware decoder. Others will require that you decode the Ambisonics data using the software
     *  decoders included with Steam Audio. The output of this step is a multichannel (stereo, quadraphonic, 5.1, or
     *  7.1) audio buffer that can be used with most typical audio engines. See \ref ambisonicspanning and
     *  \ref ambisonics for details.
     *
     *  \subsubsection pipeline_source_indirect_send Mixing Direct and Indirect Sound
     *  You can adjust the relative levels of direct and indirect sound in the final mix. If your audio engine
     *  supports auxiliary buses, submix voices, or something similar, you should be able to instantiate the
     *  \ref conveffect object on the auxiliary bus, and adjust the send level to the auxiliary bus for each source.
     *  If this feature is not supported by your audio engine, you may have to implement it yourself.
     *
     *  \section pipeline_reverb Using Steam Audio for Reverb
     *  You can use Steam Audio to apply listener-dependent reverb to the audio flowing through any part of your
     *  audio engine's signal graph. 
     *
     *  \image html ./pipeline_reverb.png "Audio signal flow when using Steam Audio to apply physics-based reverb."
     *
     *  \subsection pipeline_reverb_effect Physics-Based Reverb
     *  From the point of view of Steam Audio, applying reverb to a (sub-)mixed buffer of audio is more or less the
     *  same as applying sound propagation effects to a source. The only difference is that for sound propagation,
     *  the source and listener are in general at different positions; for reverb they must be at the same position.
     *  The signal flow is otherwise identical, see \ref conveffect for details.
     *
     *  \subsection pipeline_reverb_ambisonics Ambisonics Rendering
     *  As with sound propagation effects, the output of the physics-based reverb step is a multi-channel Ambisonics
     *  buffer. It may be decoded in software or hardware by your audio engine; if this functionality is not available,
     *  you can use Steam Audio's software decoders. See \ref ambisonicspanning and \ref ambisonics for details.
     *
     *  \subsection pipeline_reverb_mix Mixing Reverb
     *  If your audio engine supports auxiliary buses or submix voices, you can use them to create a dedicated signal
     *  path for sounds that have reverb applied to them, and adjust a send level (or wet/dry mix level) to balance
     *  the relative level of reverb in the mix. If your audio engine does not support this functionality, you may
     *  have to implement it yourself.
     *
     *  \section pipeline_spatializemix Spatializing Surround and Ambisonics Tracks
     *  You can use Steam Audio to spatialize any Ambisonics or surround (quadraphonic, 5.1, or 7.1) audio signal
     *  to a binaural (2-channel) signal. This can help keep CPU overheads low when you want to spatialize lots of
     *  sources. It is also useful for quickly adding HRTF-based binaural rendering to an existing project, without
     *  making extensive changes to individual sources.
     *
     *  \image html ./pipeline_ambisonics.png "Audio signal flow when using Steam AUdio to spatialize Ambisonics audio."
     *
     *  \subsection pipeline_spatializemix_ambisonics Ambisonics Rendering
     *  Any Ambisonics audio data can be spatialized using HRTFs. See \ref ambisonics for details.
     *
     *  \subsection pipeline_spatializemix_surround Virtual Surround
     *  You can also apply HRTF-based binaural rendering to any surround-format buffer. For example, with a 5.1
     *  audio buffer, Steam Audio can place virtual sources around the listener corresponding to the speaker
     *  positions in a standard 5.1 layout, and render each channel as a mono point source at its corresponding
     *  position. See \ref virtualsurround for details.
     *
     */

    /** \page page_audio_engine Audio Engine Integration
     *  \tableofcontents
     *
     *  This page provides an overview of the different Steam Audio components that can be used by an audio engine
     *  integration to apply the various Steam Audio effects. For more details on how each of these features fit
     *  with the audio engine's overall signal flow, see \ref page_audio_pipeline.
     *
     *  \section audioengine_data Representing Audio Data
     *  Digital audio systems represent sound using a series of _samples_. A sample measures sound intensity at a
     *  specific moment in time. A complete sound signal is stored by recording samples at regular intervals. The
     *  number of samples needed to represent 1 second of audio is referred to as the _sampling rate_; typical
     *  sampling rates are 44.1 kHz (44100 samples per second), 48 kHz, and so on.
     *
     *  \subsection audioengine_data_format Supported Audio Formats
     *  Audio engines may process data in a variety of formats. Audio data can have multiple _channels_, for example,
     *  stereo (2 channels) and 5.1 surround (6 channels). Each sample comprises of one _element_ for each channel
     *  (see the figure below). All elements for a single sample may be stored consecutively, followed by all elements
     *  for the next sample, and so on. This data layout is called _interleaved_ storage. Alternatively, all
     *  elements for a single channel may be stored consecutively, followed by elements for the next channel, and so
     *  on. This data layout is called _deinterleaved_ storage. Steam Audio lets you specify the format of any data
     *  passed to or from the audio engine; see \ref IPLAudioFormat for details.
     *
     *  In Steam Audio, all elements are represented using 32-bit, single-precision IEEE 754 floating-point numbers.
     *  Some audio engines may represent elements in a different format (for example, 16-bit signed integers). It is
     *  the application's responsibility to ensure that any necessary conversion is carried out before sending data to
     *  Steam Audio.
     *
     *  \subsection audioengine_data_buffer Audio Buffers
     *  Most audio engines perform audio processing in _frames_. A frame is a fixed number of samples that are
     *  processed together. The parameters of any DSP algorithm do not change while processing a single frame; they may
     *  only be changed between frames. Typical frame sizes are 1024 or 512 samples. These audio frames are not related
     *  to the visual frames encountered in graphics. The audio engine is not tied to the graphical frame rate in any
     *  way.
     *
     *  You can pass frames of audio data to and from Steam Audio using \ref IPLAudioBuffer. For details on specifying
     *  audio engine sampling rates and frame sizes, see \ref IPLRenderingSettings.
     *
     *  \section audioengine_direct Direct Sound
     *  Broadly, the two kinds of effects that can be applied to a sound to model the direct path from the source to
     *  listener are spatialization (HRTF) and non-spatialization effects (attenuation, occlusion).
     *
     *  \subsection audioengine_direct_hrtf Spatializing Sound Sources
     *  Before HRTF-based binaural rendering can be applied to any sound source, a \ref binauralrenderer object must
     *  be created using \ref iplCreateBinauralRenderer. There must only be a single Binaural Renderer object; it will
     *  be shared by all sources that will be rendered using HRTFs. The lifetime of a Binaural Renderer object must be
     *  at least as long as the total lifetime of all binaurally-rendered sources. When a Binaural Renderer object is
     *  no longer needed, destroy it using \ref iplDestroyBinauralRenderer.
     *
     *  For every source to which you want to apply binaural rendering, you must create an \ref binauraleffect object,
     *  using \ref iplBinauralEffectCreate. The lifetime of the Object-Based Binaural Effect object must be tied to
     *  the lifetime of the source itself. When the source is to be destroyed, destroy the Object-Based Binaural
     *  Effect object using \ref iplBinauralEffectRelease. Every audio frame, use \ref iplBinauralEffectApply to
     *  apply HRTF-based binaural rendering to the audio flowing through the source's audio graph.
     *
     *  \subsection audioengine_direct_attenuation Attenuation and Occlusion
     *  You can use \ref iplGetDirectSoundPath to query distance attenuation and occlusion parameters for a sound
     *  source. There are multiple occlusion algorithms you can use, see \ref IPLDirectOcclusionMethod and 
     *  \ref IPLDirectOcclusionMode for details.
     *
     *  Before you can query occlusion parameters, you will need an \ref envrenderer object. See below for details.
     *
     *  \section audioengine_indirect Indirect Sound
     *  Indirect sound includes sound propagation effects applied to individual sources, as well as physics-based
     *  reverb effects applied to mixed buffers of audio.
     *
     *  Before indirect sound effects can be applied, you must create an \ref envrenderer object, using
     *  \ref iplCreateEnvironmentalRenderer. Creating this object requires that the game engine specify an
     *  \ref environment object; see \ref page_game_engine for details on how to do this. Only one Environmental
     *  Renderer object may exist at any point in time. It will be shared by all sound sources and buses that need to
     *  apply indirect sound effects. When you no longer need an Environmental Renderer object, you can destroy it
     *  using \ref iplDestroyEnvironmentalRenderer.
     *
     *  For every source (or bus) to which you want to apply sound propagation (or reverb) effects, you must create
     *  a \ref conveffect object, using \ref iplCreateConvolutionEffect. The lifetime of the Convolution Effect
     *  object must be tied to the lifetime of the source itself. When the source is to be destroyed, the Convolution
     *  Effect object can be destroyed using \ref iplDestroyConvolutionEffect. Every audio frame, using
     *  \ref iplSetDryAudioForConvolutionEffect and \ref iplGetWetAudioForConvolutionEffect to apply indirect sound
     *  effects to the incoming audio data.
     *
     *  For increased performance, you can use \ref iplGetMixedEnvironmentalAudio instead of
     *  \ref iplGetWetAudioForConvolutionEffect. This function must be called at a point in the audio graph where
     *  indirect sound is to be mixed. The output of this function is the same as calling
     *  \ref iplGetWetAudioForConvolutionEffect on all Convolution Effect objects that have been created, and
     *  summing the results. Note that this approach prevents additional user-defined effects from being inserted
     *  after the indirect sound effects applied via the Convolution Effect object.
     *
     *  \section audioengine_spatialize Spatializing Surround and Ambisonics Tracks
     *  You can use Steam Audio to spatialize surround or Ambisonics audio flowing through any point in a bus or
     *  submix voice.
     *
     *  Surround audio (quadraphonic, 5.1, or 7.1) can be rendered using HRTFs using a \ref virtualsurround object.
     *  Create the Virtual Surround Effect object when the bus or submix voice is created, using
     *  \ref iplVirtualSurroundEffectCreate. When the bus or submix voice is destroyed, destroy the Virtual
     *  Surround Effect object using \ref iplVirtualSurroundEffectRelease. Every audio frame, call
     *  \ref iplVirtualSurroundEffectApply to apply HRTF-based binaural rendering to the surround audio data.
     *
     *  Ambisonics audio can be rendered using HRTFs (or decoded to a surround format) using an \ref ambisonics
     *  (or \ref ambisonicspanning) object. Create the Ambisonics Binaural Effect (or Ambisonics Panning Effect)
     *  object when the bus or submix voice is created, using \ref iplAmbisonicsBinauralEffectCreate (or
     *  \ref iplAmbisonicsPanningEffectCreate). When the bus or submix voice is destroyed, destroy the Ambisonics
     *  Binaural Effect (or Ambisonics Panning Effect) object using \ref iplAmbisonicsBinauralEffectRelease (or
     *  \ref iplAmbisonicsPanningEffectRelease). Every audio frame, call \ref iplAmbisonicsBinauralEffectApply (or
     *  \ref iplAmbisonicsPanningEffectApply) to apply HRTF-based binaural rendering (or Ambisonics-to-surround
     *  decoding) to the Ambisonics audio data.
     *
     */

    /** \page page_game_editor Editor Integration
     *  \tableofcontents
     *
     *  If you want to use the sound propagation, reverb, or occlusion functionality of Steam Audio, your game engine
     *  must provide information about the objects in your scene, for use by audio engine components that call into
     *  Steam Audio functionality (see \ref page_audio_engine). Before you can do this, you must add necessary
     *  functionality to your game engine's editor so it can generate the data needed by Steam Audio. This page
     *  describes each piece of functionality that must be added, but only provides general guidelines and examples of
     *  how to implement it: the specifics will vary significantly depending on your game engine.
     *
     *  \section editor_scene IScene Export
     *  The game engine needs to tell Steam Audio what geometric objects comprise your scene, and what their acoustic
     *  material properties are. To this end, the game engine's editor must expose controls that let the user
     *  specify this information.
     *
     *  \subsection editor_scene_geometry Selecting Geometry
     *  The editor needs to have some way for the user to specify which geometric objects should be used by Steam
     *  Audio for simulation. There are multiple ways of going about this:
     *
     *  - Add a component or flag to each object that must be exported to Steam Audio. This object may be as large as
     *    a mountain, or as small as a tin can, or even an individual triangle.
     *  - Create a special layer for acoustic objects. All meshes, terrain, or other geometry that is placed in this
     *    layer can be exported to Steam Audio.
     *  - Require users to create special _acoustic geometry_ for export to Steam Audio. While this may require more
     *    up-front setup work, it also brings added flexibility in that users may tune the geometry so that it
     *    generates the desired acoustic effects without being any more complex that it needs to be.
     *  - Simply export everything to Steam Audio. This is the simplest option, but is unlikely to be the best choice.
     *    This is because not all objects influence the acoustics of a space equally: for example, a large shipping
     *    container can modify the reverb of a warehouse, but a small tin can is unlikely to do so.
     *
     *  \subsection editor_scene_materials Specifying Materials
     *  Every object that is exported to Steam Audio must have a corresponding acoustic material. This may be
     *  specified as a separate component added to objects that are exported to Steam Audio.
     *
     *  You may also provide a scene-wide default material setting. This way, if a large portion of the scene is made
     *  of the same material, the user does not have to spend time configuring a large number of geometric objects in
     *  the same way.
     *
     *  If your game engine models a hierarchy of objects in the scene (most do), you can use this to simplify the
     *  specification of materials too. You can implement an acoustic material component that applies to whatever
     *  object it is attached to, and all of its children, unless overridden by another acoustic material component.
     *
     *  Alternatively, you may wish to extend any existing material system your game engine has, so that you can
     *  specify visual, physics, and acoustic materials for objects at the same time.
     *
     *  \section editor_baking Baking
     *  If you plan to use baked sound propagation or reverb effects, the game engine editor must allow the designer
     *  to place probes at which sound propagation and/or reverb data is baked; it must also provide controls for the
     *  designer to bake sound propagation and/or reverb data.
     *
     *  \subsection editor_baking_probes Placing Probes
     *  Probes are placed by specifying an axis-aligned bounding box within which probes are to be generated by
     *  Steam Audio. The editor must allow the user to place and size an axis-aligned box, select a probe generation
     *  algorithm, and use Steam Audio to generate probes within the box.
     *
     *  The editor must also save the probe-related data in an appropriate location, such as in a separate asset file,
     *  or serialized into the scene itself.
     *
     *  \subsection editor_baking_bake Generating Baked Data
     *  The editor must allow the designer to generate baked data for one or more probe boxes. This baked data may
     *  be baked reverb, baked sound propagation from a static source, or baked sound propagation from a moving
     *  source to a static listener. The baked data will be automatically saved to the same location as the rest of
     *  the data associated with the probe boxes.
     *
     *  Since baking is a time-consuming operation, Steam Audio allows the editor to provide callbacks for monitoring
     *  the progress of the bake operation. This can be used to display a progress bar within the editor window.
     *
     */

    /** \page page_game_engine Game Engine Integration
     *  \tableofcontents
     *
     *  If you plan to use the sound propagation, reverb, or occlusion features of Steam Audio, your game engine must
     *  send information to the audio engine components that in turn use Steam Audio to apply these effects. This page
     *  provides an overview of the various pieces of the game engine integration that you must implement. For more
     *  details on what editor modifications this might necessitate, see \ref page_game_editor.
     *
     *  \section game_scene IScene
     *  The \ref scene object contains all of the information that describes the geometry and materials with which
     *  sound interacts. You typically create a scene using \ref iplSceneCreate when a level is loaded, and destroy it
     *  using \ref iplDestroyScene when the data is no longer needed.
     *
     *  Scenes a composed of objects of different types. Currently, only _static meshes_ are supported (see below for
     *  details). All objects in a IScene share the same set of materials.
     *
     *  Many game engines use batching optimizations to reduce the number of draw calls sent to the GPU; this may
     *  prevent you from accessing geometry when the game runs. In such cases, you must access the geometry in the
     *  editor at design time, save a copy of it using \ref iplSceneSave, and then use
     *  \ref iplSceneLoad to load it when the game runs.
     *
     *  \subsection game_scene_staticmesh Static Meshes
     *  A static mesh is a triangle mesh that never moves or changes in any way. Large buildings, corridors, and
     *  landscape terrain can usually be represented as static meshes. To create a Static Mesh object and add it to
     *  the scene, use \ref iplStaticMeshCreate. When you no longer need the Static Mesh data, destroy it using
     *  \ref iplStaticMeshRelease.
     *
     *  You explicitly specify information about the vertices and triangles of a static mesh. You must also specify 
     *  which materials are associated with which triangles.
     *
     *  \section game_probe Probes
     *  If you are using baked sound propagation or baked reverb, you must also provide the baked data to Steam Audio
     *  when the game runs. To do this, you must first create a Probe Manager object using \ref iplCreateProbeManager.
     *  When you no longer need the baked data, you can call \ref iplDestroyProbeManager to destroy the Probe
     *  Manager object.
     *
     *  After creating a Probe Manager object, you must then populate it with information about the probes created
     *  using the editor, and the corresponding baked data.
     *
     *  \subsection game_probe_box Probe Boxes
     *  In the editor, probes are created using Probe Box objects, and saved to disk using \ref iplSaveProbeBox. The
     *  data stored in a Probe Box, which includes probe positions and baked data, can be loaded using
     *  \ref iplLoadProbeBox. However, Probe Box objects cannot directly be added to a Probe Manager; you must first
     *  create one or more Probe Batch objects.
     *
     *  \subsection game_probe_batch Probe Batches
     *  A Probe Batch is an indivisible collection of probes that can be added or removed from a Probe Manager object.
     *  A single Probe Batch may contain probes from multiple Probe Boxes; conversely, the probes from a single Probe
     *  Box may be present in multiple Probe Batches. Care must be taken to ensure that no probe is present in more
     *  than one Probe Batch. You can create a Probe Batch using \ref iplCreateProbeBatch, and destroy a Probe Batch
     *  using \ref iplDestroyProbeBatch. Probes can be added from Probe Boxes using \ref iplAddProbeToBatch. Once
     *  you have finished adding probes to a Probe Batch, you must use \ref iplFinalizeProbeBatch to ensure that
     *  Steam Audio builds necessary data structures for looking up baked data.
     *
     *  You can also assign probes to Probe Batches in the design phase. In this case, you can save Probe Batch
     *  objects to disk using \ref iplSaveProbeBatch, and load them at run-time using \ref iplLoadProbeBatch. You do
     *  not need to call \ref iplFinalizeProbeBatch after loading a Probe Batch using \ref iplLoadProbeBatch.
     *
     *  Probe Batches can be added to a Probe Manager using \ref iplAddProbeBatch. You must do this in order to use
     *  the baked data contained in a Probe Batch.
     *
     *  \section game_environment Environment
     *  Once you have created (or loaded) a IScene object, and optionally a Probe Manager object, you must package all
     *  the data that is required by the audio engine components in an \ref environment object. You can create an
     *  Environment object using \ref iplCreateEnvironment. Once you do not need the Environment object, you can
     *  destroy it using \ref iplDestroyEnvironment.
     *
     *  An Environment object is used by the audio engine integration to create an \ref envrenderer object. Once this
     *  object has been created, it retains a reference to the Environment object even if the game engine calls
     *  \ref iplDestroyEnvironment.
     *
     */

    /** \page page_tan TrueAudio Next Support
     *  \tableofcontents
     *
     *  Steam Audio provides optional support for AMD TrueAudio Next, which allows you to reserve a portion of the GPU 
     *  for accelerating convolution operations. TrueAudio Next requires a supported AMD Radeon GPU.
     *
     *  If you choose to enable TrueAudio Next support via the Steam Audio API, then Steam Audio will try to use the 
     *  GPU to accelerate the rendering of indirect sound, including real-time source-centric propagation, baked static 
     *  source propagation, baked static listener propagation, real-time listener-centric reverb, and baked 
     *  listener-centric reverb.
     *
     *  Enabling TrueAudio Next support via the Steam Audio API involves two main steps: creating an OpenCL compute
     *  device that supports TrueAudio Next, and specifying that TrueAudio Next should be used for convolution.
     *
     *  \section tan_compute_device Creating a Compute Device that supports TrueAudio Next
     *  To create a Compute Device object, you must call the \ref iplCreateComputeDevice function. Before calling this
     *  function, you must populate an \ref IPLComputeDeviceFilter struct. The parameters should be set as follows:
     *
     *  - \ref IPLComputeDeviceFilter::type "type" Must be set to \ref IPL_COMPUTEDEVICE_GPU. TrueAudio Next currently
     *    supports GPU acceleration only.
     *  - \ref IPLComputeDeviceFilter::maxCUsToReserve "maxCUsToReserve" This is the maximum number of GPU Compute Units
     *    (CUs) that will be reserved for audio processing and IR update combined.
     *  - \ref IPLComputeDeviceFilter::fractionCUsForIRUpdate "fractionCUsForIRUpdate" This is the fraction of maximum 
     *    reserved CUs that should be used for impulse response (IR) update. The IR update includes any calculation 
     *    performed by Radeon Rays to calculate IR and/or pre-transformation of the IR for convolution with input audio.
     *
     *  See \ref page_radeonrays for details on Radeon Rays Support.
     *
     *  If TrueAudio Next initialization fails for any reason, your application can either try again with different
     *  values for \c maxCUsToReserve and \c fractionCUsForIRUpdate, or fall back to Steam Audio's built-in CPU-based
     *  convolution.
     *
     *  The Compute Device object created in this step must be passed to all Steam Audio API functions that require
     *  a computeDevice parameter.
     *
     *  \section tan_convolution Using TrueAudio Next for convolution
     *  To configure Steam Audio to use TrueAudio Next for convolution, you must then create an
     *  \ref IPLRenderingSettings struct, with its \ref IPLRenderingSettings::convolutionType "convolutionType"
     *  parameter set to \ref IPL_CONVOLUTIONTYPE_TRUEAUDIONEXT. This \ref IPLRenderingSettings struct must be
     *  passed to the \ref iplCreateEnvironmentalRenderer function.
     */

    /** \page page_embree Embree Support
     *  \tableofcontents
     *
     *  Steam Audio provides optional support for Intel Embree, which is a highly optimized ray tracer that uses
     *  high-performance algorithms and Single Instruction Multiple Data (SIMD) instruction sets of modern x86_64
     *  CPUs. Embree works with a wide range of x86_64 CPUs.
     *
     *  To configure Steam Audio to use Embree for sound propagation, you must create an \ref IPLSimulationSettings
     *  struct, with its \ref IPLSimulationSettings::sceneType "sceneType" parameter set to
     *  \ref IPL_SCENETYPE_EMBREE. This \ref IPLSimulationSettings struct must be passed to the \ref iplSceneCreate
     *  and \ref iplCreateEnvironment functions.
     */

    /** \page page_radeonrays Radeon Rays Support
     *  \tableofcontents
     *
     *  Steam Audio provides optional support for AMD Radeon Rays, which is a highly optimized GPU ray tracer that
     *  uses OpenCL technology to scale across a wide range of hardware. Radeon Rays support in Steam Audio requires
     *  a supported OpenCL device.
     *
     *  To configure Steam Audio to use Radeon Rays for sound propagation, you must:
     *
     *  - Create a Compute Device object using \ref iplCreateComputeDevice.
     *  - Create an \ref IPLSimulationSettings struct, with its \ref IPLSimulationSettings::sceneType "sceneType"
     *    parameter set to \ref IPL_SCENETYPE_RADEONRAYS. This \ref IPLSimulationSettings struct must be passed to
     *    the \ref iplSceneCreate and \ref iplCreateEnvironment functions.
     */
