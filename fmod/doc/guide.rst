User's Guide
============

Spatialize an event
-------------------

To spatialize an event:

1. Select the event's master track.
2. In the effects deck, right-click and select **Add Effect** > **Plug-in Effects** > **Valve** > **Steam Audio Spatializer**.

Steam Audio will apply HRTF-based binaural rendering to the event, using default settings. You can control many properties of the spatialization using the :doc:`Steam Audio Spatializer <spatializer>` effect, including occlusion, reflections, and more.


Apply distance falloff
----------------------

You can continue to use FMOD Studio's built-in tools to control how the volume of an event falls off with distance. To do so:

1. Select the event's master track.
2. In the effects deck, right-click and select **Add Effect** > **Plug-in Effects** > **Valve** > **Steam Audio Spatializer**. (Skip this step if a Steam Audio Spatializer effect has already been added to the event.)
3. Use the **Distance Attenuation** curve controls to specify the distance falloff. These controls are identical to the controls available in FMOD Studio's built-in spatializer.

Steam Audio will automatically use this curve to apply distance attenuation.


Apply frequency-dependent air absorption
----------------------------------------

You can control how different sound frequencies fall off with distance. For example, when playing a distant explosion sound, higher frequencies can fall off more than lower frequencies, leading to a more muffled sound. To do this:

1. Select the event's master track.
2. In the effects deck, right-click and select **Add Effect** > **Plug-in Effects** > **Valve** > **Steam Audio Spatializer**. (Skip this step if a Steam Audio Spatializer effect has already been added to the event.)
3. Set **Air Absorption** to **Simulation-Defined**.

Steam Audio will now use its default air absorption model to apply frequency-dependent distance falloff to the event. You can also use the :doc:`Steam Audio Spatializer <spatializer>` to directly control the filter, either manually or via scripting.


Specify a source directivity pattern
------------------------------------

You can configure a source that emits sound with different intensities in different directions. For example, a megaphone that projects sound mostly to the front. To do this:

1. Select the event's master track.
2. In the effects deck, right-click and select **Add Effect** > **Plug-in Effects** > **Valve** > **Steam Audio Spatializer**. (Skip this step if a Steam Audio Spatializer effect has already been added to the event.)
3. Set **Directivity** to **Simulation-Defined**.
4. Use the **Weight** and **Power** dials to control the directivity pattern.

For more information, see :doc:`Steam Audio Spatializer <spatializer>`.


Use a custom HRTF
-----------------

You (or your players) can replace Steam Audio's built-in HRTF with any HRTF of your choosing. This is useful for comparing different HRTF databases, measurement or simulation techniques, or even allowing players to use a preferred HRTF with your game or app.

Steam Audio loads custom HRTFs from SOFA files. These files have a ``.sofa`` extension.

.. note::

    The Spatially-Oriented Format for Acoustics (SOFA) file format is defined by an Audio Engineering Society (AES) standard. For more details, click `here <https://www.sofaconventions.org>`_.

Custom HRTFs are loaded and set by the game engine. No special configuration is needed within FMOD Studio.

.. tabs::

    .. group-tab:: Unity

        Follow the instructions in the Steam Audio Unity integration documentation to specify custom HRTFs.

    .. group-tab:: Unreal Engine

        The Steam Audio Unreal Engine plugin does not currently support custom HRTFs.

    .. group-tab:: C++

        1.  In the game engine, use ``iplHRTFCreate`` to load one or more SOFA files.
        2.  Call ``iplFMODSetHRTF`` to specify the HRTF to use for spatialization.

.. warning::

    The SOFA file format allows for very flexible ways of defining HRTFs, but Steam Audio only supports a restricted subset. The following restrictions apply (for more information, including definitions of the terms below, click `here <https://www.sofaconventions.org>`_:

    -   SOFA files must use the ``SimpleFreeFieldHRIR`` convention.
    -   The ``Data.SamplingRate`` variable may be specified only once, and may contain only a single value. Steam Audio will automatically resample the HRTF data to the user's output sampling rate at run-time.
    -   The ``SourcePosition`` variable must be specified once for each measurement.
    -   Each source must have a single emitter, with ``EmitterPosition`` set to ``[0 0 0]``.
    -   The ``ListenerPosition`` variable may be specified only once (and not once per measurement). Its value must be ``[0 0 0]``.
    -   The ``ListenerView`` variable is optional. If specified, its value must be ``[1 0 0]`` (in Cartesian coordinates) or ``[0 0 1]`` (in spherical coordinates).
    -   The ``ListenerUp`` variable is optional. If specified, its value must be ``[0 0 1]`` (in Cartesian coordinates) or ``[0 90 1]`` (in spherical coordinates).
    -   The listener must have two receivers. The receiver positions are ignored.
    -   The ``Data.Delay`` variable may be specified only once. Its value must be 0.


Model occlusion by geometry
---------------------------

You can configure an event to be occluded by scene geometry. To do this:

1. Select the event.
2. Make sure a Steam Audio Spatializer effect is added to its master track.
3. Set **Occlusion** to **Simulation-Defined**.

You must now configure your game engine to enable occlusion simulation for this event.

.. tabs::

    .. group-tab:: Unity

        1.  Select the FMOD Event Emitter.
        2.  Make sure a Steam Audio Source component is attached to it.
        3.  In the Inspector, check **Occlusion**.

        Steam Audio will now use raycast occlusion to check if the source is occluded from the listener by any geometry. This assumes that the source is a single point. You can also model sources with larger spatial extent. For more information, refer to the Steam Audio Unity integration documentation.

    .. group-tab:: Unreal Engine

        1.  Select the actor containing the FMOD Audio component.
        2.  Make sure a Steam Audio Source component is attached to it.
        3.  In the Details tab, check **Simulate Occlusion**.

        Steam Audio will now use raycast occlusion to check if the source is occluded from the listener by any geometry. This assumes that the source is a single point. You can also model sources with larger spatial extent. For more information, refer to the Steam Audio Unreal Engine integration documentation.

    .. group-tab:: C++

        1.  Call ``iplSimulatorRunDirect`` to run occlusion simulations.
        2.  Call ``iplSourceGetOutputs`` to retrieve the results of the simulation for a given source in an ``IPLSimulationOutputs`` structure.
        3.  Call ``FMOD::DSP::setParameterData`` to set the following parameters of the Steam Audio Spatializer DSP attached to the event corresponding to the source:

            1.   Set ``APPLY_OCCLUSION`` to ``1``.
            2.   Set ``SIMULATION_OUTPUTS`` to the address of the ``IPLSimulationOutputs`` structure.

You can also explicitly control occlusion manually or via scripting. For more information, see :doc:`Steam Audio Spatializer <spatializer>`.


Model transmission through geometry
-----------------------------------

You can configure an event to be transmitted through occluding geometry, with the sound attenuated based on material properties. To do this:

1. Select the event.
2. Make sure a Steam Audio Spatializer effect is added to its master track.
3. Make sure **Occlusion** is set to **Simulation-Defined**, then set **Transmission** to **Simulation-Defined**.

You must now configure your game engine to enable transmission simulation for this event.

.. tabs::

    .. group-tab:: Unity

        1.  Select the FMOD Event Emitter.
        2.  Make sure a Steam Audio Source component is attached to it.
        3.  In the Inspector, make sure **Occlusion** is checked, then check **Transmission**.

        Steam Audio will now model how sound travels through occluding geometry, based on the acoustic material properties of the geometry.

    .. group-tab:: Unreal Engine

        1.  Select the actor containing the FMOD Audio component.
        2.  Make sure a Steam Audio Source component is attached to it.
        3.  In the Details tab, make sure **Simulate Occlusion** is checked, then check **Simulate Transmission**.

        Steam Audio will now model how sound travels through occluding geometry, based on the acoustic material properties of the geometry.

    .. group-tab:: C++

       1.  Call ``iplSimulatorRunDirect`` to run occlusion and transmission simulations.
       2.  Call ``iplSourceGetOutputs`` to retrieve the results of the simulation for a given source in an ``IPLSimulationOutputs`` structure.
       3.  Call ``FMOD::DSP::setParameterData`` to set the following parameters of the Steam Audio Spatializer DSP attached to the event corresponding to the source:

           1.   Set ``APPLY_OCCLUSION`` to ``1``.
           2.   Set ``APPLY_TRANSMISSION`` to ``1``.
           3.   Set ``SIMULATION_OUTPUTS`` to the address of the ``IPLSimulationOutputs`` structure.

You can also control whether the transmission effect is frequency-dependent, or explicitly control transmission manually or via scripting. For more information, see :doc:`Steam Audio Spatializer <spatializer>`.


Model reflection by geometry
----------------------------

You can configure an event to be reflected by surrounding geometry, with the reflected sound attenuated based on material properties. Reflections often enhance the sense of presence when used with spatial audio. To do this:

1. Select the event.
2. Make sure a Steam Audio Spatializer effect is added to its master track.
3. Enable **Reflections**.

You must now configure your game engine to enable reflections simulation for this event.

.. tabs::

    .. group-tab:: Unity

        1.  Select the FMOD Event Emitter.
        2.  Make sure a Steam Audio Source component is attached to it.
        3.  In the Inspector, check **Reflections**.

        Steam Audio will now use real-time ray tracing to model how sound is reflected by geometry, based on the acoustic material properties of the geometry. You can control many aspects of this process, including how many rays are traced, how many successive reflections are modeled, how reflected sound is rendered, and much more. Since modeling reflections is CPU-intensive, you can pre-compute reflections for a static sound source, or even offload the work to the GPU. For more information, refer to the Steam Audio Unity integration documentation.

    .. group-tab:: Unreal Engine

        1.  Select the actor containing the FMOD Audio component.
        2.  Make sure a Steam Audio Source component is attached to it.
        3.  In the Details tab, check **Simulate Reflections**.

        Steam Audio will now use real-time ray tracing to model how sound is reflected by geometry, based on the acoustic material properties of the geometry. You can control many aspects of this process, including how many rays are traced, how many successive reflections are modeled, how reflected sound is rendered, and much more. Since modeling reflections is CPU-intensive, you can pre-compute reflections for a static sound source, or even offload the work to the GPU. For more information, refer to the Steam Audio Unreal Engine integration documentation.

    .. group-tab:: C++

       1.  Call ``iplSimulatorRunReflections`` to run reflection simulations.
       2.  Call ``iplSourceGetOutputs`` to retrieve the results of the simulation for a given source in an ``IPLSimulationOutputs`` structure.
       3.  Call ``FMOD::DSP::setParameterData`` to set the following parameters of the Steam Audio Spatializer DSP attached to the event corresponding to the source:

            1.   Set ``APPLY_REFLECTIONS`` to ``1``.
            2.   Set ``SIMULATION_OUTPUTS`` to the address of the ``IPLSimulationOutputs`` structure.

For more information, see :doc:`Steam Audio Spatializer <spatializer>`.


Apply physics-based reverb to a mixer bus
-----------------------------------------

You can also use ray tracing to automatically calculate physics-based reverb at the listener's position. Physics-based reverbs are *directional*, which means they can model the direction from which a distant echo can be heard, and keep it consistent as the player looks around. Physics-based reverbs also model smooth transitions between different spaces in your scene, which is crucial for maintaining immersion as the player moves. To set up physics-based reverb:

1.  In FMOD Studio's main menu, click **Window** > **Mixer**.
2.  In the **Routing** tab, select the **Reverb** bus.
3.  In the effects deck, right-click and select **Add Effect** > **Plug-in Effect** > **Valve** > **Steam Audio Reverb**.
4.  Add a *send* from one or more events to the **Reverb** bus. For more information, see the `FMOD Studio documentation <https://www.fmod.com/resources/documentation-studio?version=2.0&page=mixing.html#sends-and-return-buses>`_.

You must now configure your game engine to enable reverb simulation.

.. tabs::

    .. group-tab:: Unity

        1.  Select the FMOD Listener.
        2.  Make sure a Steam Audio Listener component is attached to it.
        3.  In the Inspector, check **Apply Reverb**.

        Steam Audio will now use real-time ray tracing to simulate physics-based reverb. You can control many aspects of this simulation, including how many rays are traced, the length of the reverb tail, whether the reverb is rendered a convolution reverb, and much more. Since modeling physics-based reverb is CPU-intensive, you can (and typically will) pre-compute reverb throughout your scene. You can even offload simulation as well as rendering work to the GPU. For more information, refer to the Steam Audio Unity integration documentation.

    .. group-tab:: Unreal Engine

        1.  Select the actor containing the Steam Audio Listener component.
        2.  In the Details tab, check **Simulate Reverb**.

        Steam Audio will now use real-time ray tracing to simulate physics-based reverb. You can control many aspects of this simulation, including how many rays are traced, the length of the reverb tail, whether the reverb is rendered a convolution reverb, and much more. Since modeling physics-based reverb is CPU-intensive, you can (and typically will) pre-compute reverb throughout your scene. You can even offload simulation as well as rendering work to the GPU. For more information, refer to the Steam Audio Unreal Engine integration documentation.

    .. group-tab:: C++

        1.  Call ``iplSimulatorRunReflections`` to run reflection simulations for a source located at the listener position.
        2.  Call ``iplFMODSetReverbSource`` to specify the ``IPLSource`` used to simulate reverb.

For more information, see :doc:`Steam Audio Reverb <reverb>`.


Model sound paths from a moving source to a moving listener
-----------------------------------------------------------

You may want to model sound propagation from a source to the listener, along a long, complicated path, like a twisting corridor. The main goal is often to ensure that the sound is positioned as if it’s coming from the correct door, window, or other opening. This is known as the *pathing* or *portaling* problem.

While you can solve this by enabling reflections on an event, it would require too many rays (and so too much CPU) to simulate accurately. Instead, you can use Steam Audio to bake pathing information in a probe batch, and use it to efficiently find paths from a moving source to a moving listener. To do this:

1. Select the event.
2. Make sure a Steam Audio Spatializer effect is added to its master track.
3. Enable **Pathing**.

You must now configure your game engine to enable reflections simulation for this event.

.. tabs::

    .. group-tab:: Unity

        1.  Make sure that pathing information is baked for one or more probe batches in your scene. For more information, refer to the Steam Audio Unity integration documentation.
        2.  Select the FMOD Event Emitter.
        3.  Make sure a Steam Audio Source component is attached to it.
        4.  Check **Pathing**.

        You can control many aspects of the baking process, as well as the run-time path finding algorithm. For more information, refer to the Steam Audio Unity integration documentation.

    .. group-tab:: Unreal Engine

        1.  Make sure that pathing information is baked for one or more probe batches in your scene. For more information, refer to the Steam Audio Unreal Engine integration documentation.
        2.  Select the actor containing the FMOD Audio component.
        3.  Make sure a Steam Audio Source component is attached to it.
        4.  Check **Simulate Pathing**.

        You can control many aspects of the baking process, as well as the run-time path finding algorithm. For more information, refer to the Steam Audio Unreal Engine integration documentation.

    .. group-tab:: C++

       1.  Call ``iplSimulatorRunPathing`` to run pathing simulations.
       2.  Call ``iplSourceGetOutputs`` to retrieve the results of the simulation for a given source in an ``IPLSimulationOutputs`` structure.
       3.  Call ``FMOD::DSP::setParameterData`` to set the following parameters of the Steam Audio Spatializer DSP attached to the event corresponding to the source:

            1.   Set ``APPLY_PATHING`` to ``1``.
            2.   Set ``SIMULATION_OUTPUTS`` to the address of the ``IPLSimulationOutputs`` structure.

For more information, see :doc:`Steam Audio Spatializer <spatializer>`.
