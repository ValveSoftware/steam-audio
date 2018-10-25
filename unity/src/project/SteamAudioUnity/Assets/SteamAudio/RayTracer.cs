// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html

//#define IPL_USE_RAYCASTALL

using System;
using UnityEngine;

namespace SteamAudio 
{
    public class RayTracer 
    {
        public static void ClosestHit(float[] rayOrigin,
                                      float[] rayDirection,
                                      float minDistance,
                                      float maxDistance,
                                      ref float hitDistance,
                                      ref Vector3 hitNormal,
                                      ref IntPtr hitMaterial,
                                      IntPtr userData)
        {
            UnityEngine.Vector3 origin = Common.ConvertVector(new Vector3 {
                x = rayOrigin[0],
                y = rayOrigin[1],
                z = rayOrigin[2]
            });

            UnityEngine.Vector3 direction = Common.ConvertVector(new Vector3 {
                x = rayDirection[0],
                y = rayDirection[1],
                z = rayDirection[2]
            });

            origin += minDistance * direction;

#if IPL_USE_RAYCASTALL
            var hits = Physics.RaycastAll(origin, direction, maxDistance);

            hitDistance = Mathf.Infinity;
            var closestHit = new RaycastHit();
            var found = false;
            foreach (var hit in hits) {
                if (Scene.HasSteamAudioGeometry(hit.collider.transform)) {
                    if (hit.distance < hitDistance) {
                        hitDistance = hit.distance;
                        closestHit = hit;
                        found = true;
                    }
                }
            }

            if (found) {
                hitNormal = Common.ConvertVector(closestHit.normal); // FIXME: not the correct transform?
                hitMaterial = Scene.GetSteamAudioMaterialBuffer(closestHit.collider.transform);
            }
#else
            LayerMask layerMask = Scene.GetSteamAudioLayerMask();

            RaycastHit hit;
            var hitValid = Physics.Raycast(origin, direction, out hit, maxDistance, layerMask);
            if (!hitValid) {
                hitDistance = Mathf.Infinity;
                return;
            }

            hitDistance = hit.distance;
            hitNormal = Common.ConvertVector(hit.normal);
            hitMaterial = Scene.GetSteamAudioMaterialBuffer(hit.collider.transform);
#endif
        }

        public static void AnyHit(float[] rayOrigin,
                                  float[] rayDirection,
                                  float minDistance,
                                  float maxDistance,
                                  ref int hitExists,
                                  IntPtr userData)
        {
            UnityEngine.Vector3 origin = Common.ConvertVector(new Vector3 {
                x = rayOrigin[0],
                y = rayOrigin[1],
                z = rayOrigin[2]
            });

            UnityEngine.Vector3 direction = Common.ConvertVector(new Vector3 {
                x = rayDirection[0],
                y = rayDirection[1],
                z = rayDirection[2]
            });

            origin += minDistance * direction;

#if IPL_USE_RAYCASTALL
            var hits = Physics.RaycastAll(origin, direction, maxDistance);

            hitExists = 0;
            foreach (var hit in hits) {
                if (Scene.HasSteamAudioGeometry(hit.collider.transform)) {
                    hitExists = 1;
                    return;
                }
            }
#else
            LayerMask layerMask = Scene.GetSteamAudioLayerMask();

            var hitValid = Physics.Raycast(origin, direction, maxDistance, layerMask);
            hitExists = (hitValid) ? 1 : 0;
#endif
        }
    }
}
