SOFA File
~~~~~~~~~

A SOFA file containing the data for a custom HRTF.

To use a SOFA file, first add it to the **SOFA Files** list under :doc:`Steam Audio Settings <settings>`. Then, make sure that the **Current HRTF** property of the global :doc:`Steam Audio Manager <manager>` instance is set to this SOFA file.

.. image:: media/sofa_file.png

Volume
    Adjusts the volume of the HRTF filters contained in the SOFA file. Different SOFA files may be normalized to different average volumes, and this parameter can be used to ensure that a given SOFA file works well within your overall mix.
