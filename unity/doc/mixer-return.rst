Steam Audio Mixer Return
~~~~~~~~~~~~~~~~~~~~~~~~

When **Reflections** are enabled for multiple Steam Audio Sources in a scene, this mixer effect can be used to reduce the CPU cost of audio processing. When this mixer effect is added to a mixer group, the indirect portion of every audio source’s output is taken out of Unity’s audio pipeline, mixed, spatialized, and re-inserted at the mixer group to which this effect is attached. The direct sound path of audio sources is not affected.

.. note::

    Since indirect sound is taken out of Unity’s audio pipeline at the Audio Source, any effects applied between the Audio Source and the Steam Audio Mixer Return effect will not apply to the indirect sound.

.. note::

    This effect should only be used if **Reflection Effect Type** is set to **Convolution** or **TrueAudio Next** in the Settings asset. It is *required* when using **TrueAudio Next**.

.. image:: media/mixer_return.png

Apply HRTF
    If checked, applies HRTF-based 3D audio rendering to mixed indirect sound. Results in an improvement in spatialization quality, at the cost of slightly increased CPU usage. Default: off.
