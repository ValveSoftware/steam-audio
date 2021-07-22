Steam Audio Material
~~~~~~~~~~~~~~~~~~~~

A collection of properties that define how an object reflects, absorbs, and transmits sound. You can create a separate Material asset for each surface type in your project, then reference it (using the **Material** property of the Steam Audio Geometry components in your scenes and prefabs) from all objects that have the corresponding material type.

Steam Audio provides a small library of built-in materials to get you started.

If the **Material** property of a Steam Audio Geometry component is set to **None**, it will use the project-wide default material, which is specified in the Settings asset.

If you have a Steam Audio Geometry component with **Export All Children** checked, all of its descendants are considered to have the material assigned to the Steam Audio Geometry.

.. image:: media/material.png

Low Freq Absorption
    How much sound the material absorbs at low frequencies (up to 800 Hz). 0 = absorbs none of the low frequency sound, 1 = absorbs all of it.

Mid Freq Absorption
    How much sound the material absorbs at middle frequencies (800 Hz - 8 kHz). 0 = absorbs none of the middle frequency sound, 1 = absorbs all of it.

High Freq Absorption
    How much sound the material absorbs at high frequencies (8 kHz and above). 0 = absorbs none of the high frequency sound, 1 = absorbs all of it.

Scattering
    How “rough” the surface is when reflecting sound. 0 = sound is reflected in a perfectly mirror-like manner (“pure specular”), 1 = sound is reflected randomly in all directions (“pure diffuse”).

Low Freq Transmission
    How much sound the material transmits at low frequencies (up to 800 Hz). 0 = no low frequency sound passes through, 1 = all of it passes through.

Mid Freq Transmission
    How much sound the material transmits at middle frequencies (800 Hz - 8 kHz). 0 = no middle frequency sound passes through, 1 = all of it passes through.

High Freq Transmission
    How much sound the material transmits at high frequencies (8 kHz and above). 0 = no high frequency sound passes through, 1 = all of it passes through.
