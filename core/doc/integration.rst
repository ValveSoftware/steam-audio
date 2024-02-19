Integrating Steam Audio
=======================

This section outlines the main steps required to integrate Steam Audio with a new game engine or audio middleware, and the key decisions you will have to make along the way.

Game engine integration
-----------------------

When integrating Steam Audio with your game engine, consider the following:

-   Initialize Steam Audio objects like the context and simulator at app startup.

-   Create Steam Audio scene objects whenever your game engine loads a scene at run-time. Destroy the Steam Audio scene object when the game engine unloads a scene.

-   Load one or more HRTFs at app startup.

-   If you are using a Steam Audio feature that requires OpenCL (i.e., Radeon Rays or TrueAudio Next), initialize OpenCL at app startup. If your game engine already uses OpenCL for some other functionality, you can instruct Steam Audio to use an existing OpenCL context and command queues.

-   Consider whether and/or how games built with the game engine intend to provide user-facing options to configure Steam Audio. For example, developers may want to provide an option to the user for switching between convolution reverb and parametric reverb.

-   If you have a high performance ray tracer available in your game engine (most game engines do have some ray tracing functionality, though performance varies), you can consider configuring Steam Audio to use it. This can simplify the process of marking up scenes for Steam Audio.

-   Consider what kinds of sound propagation simulation should run on which threads. For example, you may want occlusion to be simulated on the main update thread, to prevent noticeable lag between a source moving on-screen and its occlusion state audibly changing. On the other hand, you may want reflections or pathing simulation to run in one or more separate threads, to prevent them from unpredictably affecting the frame rate.

-   Create a Steam Audio source object whenever a sound-emitting in-game object is created, and destroy it when the object is destroyed.


Game engine editor integration
------------------------------

If you are using Steam Audio functionality that requires geometry information, you will also need to integrate with your game engine's editor tools. Consider the following:

-   Consider how developers can specify which geometry should be sent to Steam Audio. You can do this by attaching components, setting flags, or any other appropriate means.

-   Consider how developers can specify the acoustic material properties of geometry. It may be possible to extend any existing material system in the game engine for this purpose.

-   If you intend to bake reflections or pathing data, consider how developers should create and place probes in scenes.

-   Consider how and when bakes are run, and how progress feedback is provided to the user.

-   If the game engine includes command line tools for running preprocessing work in batches, you may consider integrating Steam Audio baking with the command line tooling.


Audio engine integration
------------------------

When integrating Steam Audio with a game engine's built-in audio engine, or with third-party audio middleware, consider the following:

-   The audio engine will need access to a Steam Audio context before creating any other Steam Audio objects during initialization. This will typically have been created by the game engine during app startup.

-   When integrating with audio middleware that includes a standalone editor tool, the context will also have to be created when the editor tool starts up.

-   Create one or more Steam Audio effects when a source/effect/processor instance is created by the audio engine, and destroy them when the instance is destroyed.

-   Consider which effects you want to implement, and how direct and indirect sound propagation are mixed. For example, you can allow developers to create a separate audio effect for spatializing the direct path, and a separate effect for rendering reflections, and use existing audio graph mixing tools to control how the direct and indirect audio is combined.

-   Consider which effects you want to implement on submixes (or buses). This is most commonly used for implementing listener-centric reverb.

-   If the audio engine supports Ambisonic outputs from effects, and Ambisonic buses, you can consider encoding audio sources into Ambisonics, mixing multiple Ambisonic sources into a submix, and applying spatialization towards the end of the audio pipeline, to reduce CPU usage.

-   Most crucially, you will need to implement a way of sending simulation results from the game engine's update thread (or some other thread you use for simulation) to the audio mixer thread. You can use existing functionality in your audio engine for sending parameters to effects, or implement your own approach.