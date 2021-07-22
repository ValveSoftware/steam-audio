Steam Audio Geometry
~~~~~~~~~~~~~~~~~~~~

Tags the object it’s attached to as acoustic geometry.

Must be attached to a GameObject containing a Mesh Filter or Terrain component, or a GameObject with at least one descendent in the hierarchy that contains a Mesh Filter or Terrain component.

If some ancestor of the GameObject contains a Steam Audio Dynamic Object component, the geometry will exported as part of that dynamic object. Otherwise, it will be exported as part of the static geometry of the scene.

.. image:: media/geometry.png

Material
    Reference to the Steam Audio Material asset that defines the acoustic properties of this geometry. If not set, the project-wide default material is used.

Export All Children
    If checked, the geometry of all children with a Mesh Filter or Terrain attached to them will be exported as acoustic geometry.

Terrain Simplification Level
    Geometry that is represented by a Terrain can be quite complex. This can slow down the calculation of occlusion and sound propagation. To speed things up, you can adjust this slider. As you increase this value, Steam Audio will reduce the level of detail in the terrain. This will result in faster simulation.
