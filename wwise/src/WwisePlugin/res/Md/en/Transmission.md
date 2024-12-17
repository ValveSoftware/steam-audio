## Transmission

Specifies how transmission is simulated and applied to the event.

- **Off**. Transmission is not applied.

- **Simulation-Defined**. Transmission is applied. The transmission filter EQ values must be supplied by the game engine via the `SIMULATION_OUTPUTS` parameter.

- **User-Defined**. Uses the **Transmission Low**, **Transmission Mid**, and **Transmission High** parameters to control transmission. The transmission values will not automatically change based on surrounding geometry. You are expected to control the parameters using RTPC to achieve this effect. This option is intended for integrating your own occlusion and transmission model with Steam Audio.
