Steam Audio Geometry
~~~~~~~~~~~~~~~~~~~~

Tags the actor it's attached to as acoustic geometry.

Must be attached to an actor containing a Static Mesh component, a Landscape actor, or an actor with at least one descendent in the hierarchy that either contains a Static Mesh component or is a Landscape actor.

If some ancestor of the actor contains a Steam Audio Dynamic Object component, the geometry will exported as part of that dynamic object. Otherwise, it will be exported as part of the static geometry of the level.

.. image:: media/geometry.png

Material
    Reference to the Steam Audio Material asset that defines the acoustic properties of this geometry. If not set, the project-wide default material is used.

Export All Children
    If checked, the geometry of all Static Mesh or Landscape children will be exported as acoustic geometry.
