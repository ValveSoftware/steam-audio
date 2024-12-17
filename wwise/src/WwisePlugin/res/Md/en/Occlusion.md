## Occlusion

Specifies how occlusion is simulated and applied to the event.

- **Off**. Occlusion is not applied.

- **Simulation-Defined**. Occlusion is applied. The occlusion value must be supplied by the game engine via the `SIMULATION_OUTPUTS` parameter.

- **User-Defined**. Uses the **Occlusion Value** parameter to control occlusion. The occlusion value will not automatically change based on surrounding geometry. You are expected to control the **Occlusion Value** parameter using RTPC to achieve this effect. This option is intended for integrating your own occlusion model with Steam Audio.
