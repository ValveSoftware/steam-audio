Mixer Return
~~~~~~~~~~~~

When **Reflections** are enabled for multiple events in a scene, this mixer effect can be used to reduce the CPU cost of audio processing. When this mixer effect is added to a bus, the reflected portion of every event's output is taken out of FMOD Studio's audio pipeline, mixed, spatialized, and re-inserted at the mixer group to which this effect is attached. The direct sound path of events is not affected.

.. note::
    
    Since reflected sound is taken out of FMOD Studio's audio pipeline at the Steam Audio Spatializer effect, any effects applied between the Steam Audio Spatializer effect and the Steam Audio Mixer Return effect will not apply to the reflected sound.

.. note::
    
    This effect should only be used if using **Convolution** or **TrueAudio Next** for reflections. It is *required* when using **TrueAudio Next**.

.. image:: media/mixer_return.png

Apply HRTF
    If checked, applies HRTF-based 3D audio rendering to mixed reflected sound. Results in an improvement in spatialization quality, at the cost of slightly increased CPU usage. Default: off.
