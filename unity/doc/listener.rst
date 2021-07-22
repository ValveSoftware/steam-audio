Steam Audio Listener
~~~~~~~~~~~~~~~~~~~~

Provides options for controlling how an Audio Listener is incorporated into sound propagation simulation.

Must be attached to a GameObject containing an Audio Listener (if using Unity's built-in audio engine) or an FMOD Listener (if using FMOD Studio).

.. image:: media/listener.png

Current Baked Listener
    When simulating reflections for a source whose **Reflections Type** is set to **Baked Static Listener**, the position and orientation of the GameObject specified in this field will be used as the position and orientation of the listener.

Apply Reverb
    If checked, listener-centric reverb will be simulated. You can use the Steam Audio Reverb effect somewhere in your mixer hierarchy to apply the reverb to some portion of your audio mix.

Reverb Type
    Specifies how listener-centric reverb is simulated.

    -   *Realtime*. Rays are traced from the listener in real-time, and bounced around the scene to simulate reverberation. This allows reverb to vary smoothly and account for dynamic geometry, at the cost of significant CPU usage.

    -   *Baked*. Baked reverb data is used to interpolate the reverberation at the listener position. This prevents reverb from accounting for dynamic geometry and results in relatively low CPU usage, at the cost of increased memory and disk space usage.

Use All Probe Batches
    If checked, reverb data will be baked into every probe batch in the scene.

Probe Batches
    If **Use All Probe Batches** is not checked, this is a list of probe batches into which reverb data will be baked.

Bake
    Starts baking reverb for this scene.

Baked Data Statistics
    Shows the size (in bytes) of baked reverb data stored in every probe batch.
