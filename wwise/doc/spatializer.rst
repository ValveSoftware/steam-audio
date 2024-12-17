Spatializer
~~~~~~~~~~~

Provides options for controlling how an event is spatialized.

.. image:: media/spatializer.png

Apply HRTF
    If checked, Steam Audio uses HRTF-based binaural rendering. Results in improved localization, including vertical and front-back spatialization, at the cost of slightly increased CPU usage. Requires stereo headphones.

    If unchecked, uses constant-power panning. Can be used to render to any channel configuration.

Position (X)
    The x-coordinate of the source relative to the listener. Only used when previewing in the Wwise authoring app.

Position (Y)
    The y-coordinate of the source relative to the listener. Only used when previewing in the Wwise authoring app.

Position (Z)
    The z-coordinate of the source relative to the listener. Only used when previewing in the Wwise authoring app.

HRTF Interpolation
    Controls how HRTFs are interpolated when the source moves relative to the listener.

    -  *Nearest*: Uses the HRTF from the direction nearest to the direction of the source for which HRTF data is available. The fastest option, but can result in audible artifacts for certain kinds of audio clips, such as white noise or engine sounds.

    -  *Bilinear*: Uses an HRTF generated after interpolating from four directions nearest to the direction of the source, for which HRTF data is available. This may result in smoother audio for some kinds of sources when the listener looks around, but has higher CPU usage (up to 2x).

Distance Attenuation
    If checked, a physics-based distance attenuation model is used. This is an inverse distance falloff.

    If unchecked, distance attenuation is not applied by Steam Audio. You can continue to use Wwise's built-in distance attenuation controls to apply distance attenuation.

Air Absorption
    If checked, a physics-based frequency-dependent distance-based air absorption model is used. This is an exponential falloff, with higher frequencies falling off faster with distance than lower frequencies.

    If unchecked, air absorption is not applied by Steam Audio. You can continue to tuse Wwise's built-in air distance-based low-pass and high-pass filter controls to apply air absorption.

Directivity
    If checked, a dipole directivity model is used. You can control the dipole shape using the **Dipole Weight** and **Dipole Power** parameters.

Dipole Weight
    Blends between monopole (omnidirectional) and dipole directivity patterns. 0 = pure monopole (sound is emitted in all directions with equal intensity), 100% = pure dipole (sound is focused to the front and back of the source). At 50%, the source has a cardioid directivity, with most of the sound emitted to the front of the source. Only used if **Directivity** is checked.

Dipole Power
    Controls how focused the dipole directivity is. Higher values result in sharper directivity patterns. Only used if **Directivity** is checked.

Occlusion
    Specifies how occlusion is simulated and applied to the event.

    -   *Off*. Occlusion is not applied.

    -   *Simulation-Defined*. Occlusion is applied. The occlusion value must be supplied by the game engine via the ``SIMULATION_OUTPUTS`` parameter.

    -   *User-Defined*. Uses the **Occlusion Value** parameter to control occlusion. The occlusion value will *not* automatically change based on surrounding geometry. You are expected to control the **Occlusion Value** parameter using RTPC to achieve this effect. This option is intended for integrating your own occlusion model with Steam Audio.

Occlusion Value
    The occlusion attenuation value. Only used if **Occlusion** is set to **User-Defined**. 0% = sound is completely attenuated, 100% = sound is not attenuated at all.

Transmission
    Specifies how transmission is simulated and applied to the event.

    -   *Off*. Transmission is not applied.

    -   *Simulation-Defined*. Transmission is applied. The transmission filter EQ values must be supplied by the game engine via the ``SIMULATION_OUTPUTS`` DSP parameter.

    -   *User-Defined*. Uses the **Transmission Low**, **Transmission Mid**, and **Transmission High** parameters to control transmission. The transmission values will *not* automatically change based on surrounding geometry. You are expected to control the parameters using RTPC to achieve this effect. This option is intended for integrating your own occlusion and transmission model with Steam Audio.

Transmission Type
    Specifies how the transmission filter is applied.

    -  *Frequency Independent*. Transmission is modeled as a single attenuation factor.

    -  *Frequency Dependent*. Transmission is modeled as a 3-band EQ.

Transmission Low
    The low frequency (up to 800 Hz) EQ value for transmission. Only used if **Transmission** is set to **User-Defined**. 0% = low frequencies are completely attenuated, 100% = low frequencies are not attenuated at all.

Transmission Mid
    The middle frequency (800 Hz to 8 kHz) EQ value for transmission. Only used if **Transmission** is set to **User-Defined**. 0% = middle frequencies are completely attenuated, 100% = middle frequencies are not attenuated at all.

Transmission High
    The high frequency (8 kHz and above) EQ value for transmission. Only used if **Transmission** is set to **User-Defined**. 0% = high frequencies are completely attenuated, 100% = high frequencies are not attenuated at all.

Direct Mix Level
    The contribution of the direct sound path to the overall mix for this event. Lower values reduce the contribution more.

Reflections
    If enabled, reflections reaching the listener from the source will be applied to the event.

Apply HRTF To Reflections
    If checked, applies HRTF-based 3D audio rendering to reflections. Results in an improvement in spatialization quality when using convolution or hybrid reverb, at the cost of slightly increased CPU usage. Default: off.

Reflections Mix Level
    The contribution of reflections to the overall mix for this event. Lower values reduce the contribution more.

Pathing
    If checked, shortest paths taken by sound as it propagates from the source to the listener will be simulated, and appropriate spatialization will be applied to the event for these indirect paths.

Apply HRTF To Pathing
    If checked, applies HRTF-based 3D audio rendering to pathing. Results in an improvement in spatialization quality, at the cost of slightly increased CPU usage. Default: off.

Pathing Mix Level
    The contribution of pathing to the overall mix for this event. Lower values reduce the contribution more.
