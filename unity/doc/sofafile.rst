SOFA File
~~~~~~~~~

A SOFA file containing the data for a custom HRTF.

To use a SOFA file, first add it to the **SOFA Files** list under :doc:`Steam Audio Settings <settings>`. Then, make sure that the **Current HRTF** property of the global :doc:`Steam Audio Manager <manager>` instance is set to this SOFA file.

.. image:: media/sofa_file.png

HRTF Volume Gain (dB)
    Adjusts the volume of the HRTF filters contained in the SOFA file. Different SOFA files may be normalized to different average volumes, and this parameter can be used to ensure that a given SOFA file works well within your overall mix.

HRTF Normalization Type
    Specifies the algorithm to use (if any) to normalize the volumes of the SOFA file's HRTF filters across all directions. See :doc:`Steam Audio Settings <settings>` for more details.
