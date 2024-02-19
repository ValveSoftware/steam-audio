.. _ref_scene:

Scene
-----

Typedefs
^^^^^^^^

.. doxygentypedef:: IPLScene
.. doxygentypedef:: IPLStaticMesh
.. doxygentypedef:: IPLInstancedMesh

Functions
^^^^^^^^^

.. doxygenfunction:: iplSceneCreate
.. doxygenfunction:: iplSceneRetain
.. doxygenfunction:: iplSceneRelease
.. doxygenfunction:: iplSceneLoad
.. doxygenfunction:: iplSceneSave
.. doxygenfunction:: iplSceneSaveOBJ
.. doxygenfunction:: iplSceneCommit
.. doxygenfunction:: iplStaticMeshCreate
.. doxygenfunction:: iplStaticMeshRetain
.. doxygenfunction:: iplStaticMeshRelease
.. doxygenfunction:: iplStaticMeshLoad
.. doxygenfunction:: iplStaticMeshSave
.. doxygenfunction:: iplStaticMeshAdd
.. doxygenfunction:: iplStaticMeshRemove
.. doxygenfunction:: iplInstancedMeshCreate
.. doxygenfunction:: iplInstancedMeshRetain
.. doxygenfunction:: iplInstancedMeshRelease
.. doxygenfunction:: iplInstancedMeshAdd
.. doxygenfunction:: iplInstancedMeshRemove
.. doxygenfunction:: iplInstancedMeshUpdateTransform

Structures
^^^^^^^^^^

.. doxygenstruct:: IPLTriangle
.. doxygenstruct:: IPLMaterial
.. doxygenstruct:: IPLRay
.. doxygenstruct:: IPLHit
.. doxygenstruct:: IPLSceneSettings
.. doxygenstruct:: IPLStaticMeshSettings
.. doxygenstruct:: IPLInstancedMeshSettings

Enumerations
^^^^^^^^^^^^

.. doxygenenum:: IPLSceneType

Callbacks
^^^^^^^^^

.. doxygentypedef:: IPLClosestHitCallback
.. doxygentypedef:: IPLAnyHitCallback
.. doxygentypedef:: IPLBatchedClosestHitCallback
.. doxygentypedef:: IPLBatchedAnyHitCallback
