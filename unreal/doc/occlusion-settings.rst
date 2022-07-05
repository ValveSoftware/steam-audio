Steam Audio Occlusion Settings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Provides options for controlling how the sound of an Audio component is modified along the direct propagation path from the source to the listener.

.. image:: media/occlusionsettings_transmission.png

Apply Distance Attenuation
    If checked, distance attenuation will be calculated and applied to the Audio component.

Apply Air Absorption
    If checked, frequency-dependent distance based air absorption will be calculated and applied to the Audio component.

Apply Directivity
    If checked, attenuation based on the source's directivity pattern and orientation will be applied to the Audio component.

Dipole Weight
    Blends between monopole (omnidirectional) and dipole directivity patterns. 0 = pure monopole (sound is emitted in all directions with equal intensity), 1 = pure dipole (sound is focused to the front and back of the source). At 0.5, the source has a cardioid directivity, with most of the sound emitted to the front of the source. Only used if **Apply Directivity** is checked.

Dipole Power
    Controls how focused the dipole directivity is. Higher values result in sharper directivity patterns. Only used if **Apply Directivity** is checked.

Apply Occlusion
    If checked, attenuation based on the occlusion of the source by the scene geometry will be applied to the Audio component. Occlusion must be configured using a Steam Audio Source component.

Apply Transmission
    If checked, a filter based on the transmission of sound through occluding scene geometry will be applied to the Audio component. Occlusion and transmission must be configured using a Steam Audio Source component.

Transmission Type
    Specifies how the transmission filter is applied.

    -  *Frequency Independent*. Transmission is modeled as a single attenuation factor.

    -  *Frequency Dependent*. Transmission is modeled as a 3-band EQ.
