Steam Audio Manager
~~~~~~~~~~~~~~~~~~~

Maintains global state for Steam Audio, manages simulations, and passes simulation results to the audio engine.

.. warning::

    This object is automatically created by Steam Audio at app startup. You should not manually create it.

.. image:: media/manager.png

Current HRTF
    Specifies the SOFA file containing the HRTF to use for spatializing direct and indirect sound. The options include Steam Audio's built-in HRTF, and any SOFA files listed in the :doc:`Steam Audio Settings <settings>` asset.
