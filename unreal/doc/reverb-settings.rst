Steam Audio Reverb Settings
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Provides options for controlling how source-centric reflections are applied to an Audio component.

.. image:: media/reverbsettings_reflections.png

Apply Reflections
    If checked, reflections reaching the listener from the source will be applied to the Audio component. Reflection simulation must be configured using a Steam Audio Source component.

Apply HRTF To Reflections
    If checked, applies HRTF-based 3D audio rendering to reflections. Results in an improvement in spatialization quality when using convolution or hybrid reverb, at the cost of slightly increased CPU usage. Default: off.

Reflections Mix Level
    The contribution of reflections to the overall mix for this Audio component. Lower values reduce the contribution more.
