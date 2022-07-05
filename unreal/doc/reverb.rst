Steam Audio Reverb
~~~~~~~~~~~~~~~~~~

Applies reverb based on the listener position to any audio flowing through the Sound Submix in whose effect chain this effect is attached. The type of reverb applied (convolution, parametric, or hybrid) can be configured in the Steam Audio Settings.

Reverb simulation must be configured using a Steam Audio Listener component.

.. image:: media/reverbpreset_reverb.png

Apply Reverb
    If checked, reverb is applied to the audio received as input to the Sound Submix.

Apply HRTF
    If checked, applies HRTF-based 3D audio rendering to reverb. Results in an improvement in spatialization quality when using convolution or hybrid reverb, at the cost of slightly increased CPU usage. Default: off.
