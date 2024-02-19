.. _ref_simulation:

Simulation
----------

Typedefs
^^^^^^^^

.. doxygentypedef:: IPLSource
.. doxygentypedef:: IPLSimulator

Functions
^^^^^^^^^

.. doxygenfunction:: iplDistanceAttenuationCalculate
.. doxygenfunction:: iplAirAbsorptionCalculate
.. doxygenfunction:: iplDirectivityCalculate
.. doxygenfunction:: iplSourceCreate
.. doxygenfunction:: iplSourceRetain
.. doxygenfunction:: iplSourceRelease
.. doxygenfunction:: iplSourceAdd
.. doxygenfunction:: iplSourceRemove
.. doxygenfunction:: iplSourceSetInputs
.. doxygenfunction:: iplSourceGetOutputs
.. doxygenfunction:: iplSimulatorCreate
.. doxygenfunction:: iplSimulatorRetain
.. doxygenfunction:: iplSimulatorRelease
.. doxygenfunction:: iplSimulatorSetScene
.. doxygenfunction:: iplSimulatorAddProbeBatch
.. doxygenfunction:: iplSimulatorRemoveProbeBatch
.. doxygenfunction:: iplSimulatorSetSharedInputs
.. doxygenfunction:: iplSimulatorCommit
.. doxygenfunction:: iplSimulatorRunDirect
.. doxygenfunction:: iplSimulatorRunReflections
.. doxygenfunction:: iplSimulatorRunPathing

Structures
^^^^^^^^^^

.. doxygenstruct:: IPLDistanceAttenuationModel
.. doxygenstruct:: IPLAirAbsorptionModel
.. doxygenstruct:: IPLDirectivity
.. doxygenstruct:: IPLSimulationSettings
.. doxygenstruct:: IPLSourceSettings
.. doxygenstruct:: IPLSimulationInputs
.. doxygenstruct:: IPLSimulationSharedInputs
.. doxygenstruct:: IPLSimulationOutputs

Enumerations
^^^^^^^^^^^^

.. doxygenenum:: IPLSimulationFlags
.. doxygenenum:: IPLDirectSimulationFlags
.. doxygenenum:: IPLDistanceAttenuationModelType
.. doxygenenum:: IPLAirAbsorptionModelType
.. doxygenenum:: IPLOcclusionType

Callbacks
^^^^^^^^^

.. doxygentypedef:: IPLDistanceAttenuationCallback
.. doxygentypedef:: IPLAirAbsorptionCallback
.. doxygentypedef:: IPLDirectivityCallback
.. doxygentypedef:: IPLPathingVisualizationCallback
