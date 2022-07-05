Steam Audio Spatialization Settings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Provides options for controlling how an Audio component is spatialized.

.. image:: media/spatializationsettings.png

Binaural
    If checked, HRTF-based binaural rendering will be used to spatialize the source. This requires 2-channel (stereo) audio output. If unchecked, panning will be used to the spatialize the source using the user's speaker layout. Binaural rendering provides improved spatialization at the cost of slightly increased CPU usage.

Interpolation
    Controls how HRTFs are interpolated when the source moves relative to the listener.

    -  *Nearest*: Uses the HRTF from the direction nearest to the direction of the source for which HRTF data is available. The fastest option, but can result in audible artifacts for certain kinds of audio clips, such as white noise or engine sounds.

    -  *Bilinear*: Uses an HRTF generated after interpolating from four directions nearest to the direction of the source, for which HRTF data is available. This may result in smoother audio for some kinds of sources when the listener looks around, but has higher CPU usage (up to 2x).

Apply Pathing
    If checked, the results of pathing simulation will be applied when spatializing the Audio component. Pathing simulation must be configured using a Steam Audio Source component.

Apply HRTF To Pathing
    If checked, applies HRTF-based 3D audio rendering to pathing. Results in an improvement in spatialization quality, at the cost of slightly increased CPU usage. Default: off.

Pathing Mix Level
    The contribution of pathing to the overall mix for this Audio component. Lower values reduce the contribution more.
