// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html

using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;

namespace SteamAudio 
{
    public class RayTracer 
    {
        static RaycastHit[] s_hits = null;
        static float[] s_origin = null;
        static float[] s_direction = null;

        [MonoPInvokeCallback(typeof(PhononCore.ClosestHitCallback))]
        public static void ClosestHit(IntPtr rayOrigin,
                                      IntPtr rayDirection,
                                      float minDistance,
                                      float maxDistance,
                                      ref float hitDistance,
                                      ref Vector3 hitNormal,
                                      ref IntPtr hitMaterial,
                                      IntPtr userData)
        {
            if (s_origin == null) 
            {
                s_origin = new float[3];
            }

            if (s_direction == null) 
            {
                s_direction = new float[3];
            }

            Marshal.Copy(rayOrigin, s_origin, 0, 3);
            Marshal.Copy(rayDirection, s_direction, 0, 3);

            Vector3 o = new Vector3 { x = s_origin[0], y = s_origin[1], z = s_origin[2] };
            Vector3 d = new Vector3 { x = s_direction[0], y = s_direction[1], z = s_direction[2] };

            UnityEngine.Vector3 origin = Common.ConvertVector(o);
            UnityEngine.Vector3 direction = Common.ConvertVector(d);

            origin += minDistance * direction;

            LayerMask layerMask = Scene.GetSteamAudioLayerMask();

            if (s_hits == null) 
            {
                s_hits = new RaycastHit[1];
            }

            int numHits = Physics.RaycastNonAlloc(origin, direction, s_hits, maxDistance, layerMask);
            if (numHits == 0) 
            {
                hitDistance = Mathf.Infinity;
                return;
            }

            hitDistance = s_hits[0].distance;
            hitNormal = Common.ConvertVector(s_hits[0].normal);
            hitMaterial = Scene.GetSteamAudioMaterialBuffer(s_hits[0].collider.transform);
        }

        [MonoPInvokeCallback(typeof(PhononCore.AnyHitCallback))]
        public static void AnyHit(IntPtr rayOrigin,
                                  IntPtr rayDirection,
                                  float minDistance,
                                  float maxDistance,
                                  ref int hitExists,
                                  IntPtr userData)
        {
            if (s_origin == null) 
            {
                s_origin = new float[3];
            }

            if (s_direction == null)
            {
                s_direction = new float[3];
            }

            Marshal.Copy(rayOrigin, s_origin, 0, 3);
            Marshal.Copy(rayDirection, s_direction, 0, 3);

            Vector3 o = new Vector3 { x = s_origin[0], y = s_origin[1], z = s_origin[2] };
            Vector3 d = new Vector3 { x = s_direction[0], y = s_direction[1], z = s_direction[2] };

            UnityEngine.Vector3 origin = Common.ConvertVector(o);
            UnityEngine.Vector3 direction = Common.ConvertVector(d);

            origin += minDistance * direction;

            LayerMask layerMask = Scene.GetSteamAudioLayerMask();

            if (s_hits == null)
            {
                s_hits = new RaycastHit[1];
            }

            int numHits = Physics.RaycastNonAlloc(origin, direction, s_hits, maxDistance, layerMask);
            hitExists = (numHits > 0) ? 1 : 0;
        }
    }
}
