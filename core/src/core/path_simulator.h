//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include "path_data.h"
#include "probe_manager.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// PathSimulator
// --------------------------------------------------------------------------------------------------------------------

typedef void (IPL_CALLBACK* ValidationRayVisualizationCallback)(Vector3f from,
                                                                Vector3f to,
                                                                bool occluded,
                                                                void* userData);

// Finds paths from the source to the listener, using the baked data describing paths between pairs of probes.
class PathSimulator
{
public:
    static bool sEnablePathsFromAllSourceProbes;

    // Initializes the simulator.
    PathSimulator(const ProbeBatch& probes,
                  int numSamples,
                  bool asymmetricVisRange,
                  const Vector3f& down);

    // Calculates an Ambisonics sound field describing one or more paths from the source to the listener. The sound
    // field is described using two components: SH coefficients describing the directional distribution of sound, and
    // EQ coefficients describing the overall low-pass filtering effect due to diffraction.
    bool findPaths(const Vector3f& source,
                   const Vector3f& listener,
                   const IScene& scene,
                   const ProbeBatch& probes,
                   const ProbeNeighborhood& sourceProbes,
                   const ProbeNeighborhood& listenerProbes,
                   float radius,
                   float threshold,
                   float visRange,
                   int order,
                   bool enableValidation,
                   bool findAlternatePaths,
                   bool simplifyPaths,
                   bool realTimeVis,
                   float* eqGains,
                   float* coeffs,
                   Vector3f* avgDirection = nullptr,
                   float* distanceRatio = nullptr,
                   ValidationRayVisualizationCallback validationRayVisualization = nullptr,
                   void* userData = nullptr,
                   bool forceDirectOcclusion = false);

    SoundPath findShortestPathFromSourceProbeToListenerProbe(const IScene& scene, const ProbeBatch& probes,
        int sourceProbeIndex, int listenerProbeIndex, const BakedPathData& bakedPathData, float radius, float threshold,
        float visRange, bool enableValidation, bool findAlternatePaths, bool simplifyPaths, bool realTimeVis,
        ProbePath& probePath, ValidationRayVisualizationCallback validationRayVisualization, void* userData);

private:
    ProbeVisibilityTester mVisTester; // A visibility tester.
    PathFinder mPathFinder; // A path finder, used to find alternate paths at run-time if needed.

    void findPathsFromSourceProbe(const IScene& scene,
                                  const ProbeBatch& probes,
                                  const ProbeNeighborhood& sourceProbes,
                                  const ProbeNeighborhood& listenerProbes,
                                  const BakedPathData& bakedPathData,
                                  int sourceProbeNeighborhoodIndex,
                                  float sourceProbeWeight,
                                  float radius,
                                  float threshold,
                                  float visRange,
                                  bool enableValidation,
                                  bool findAlternatePaths,
                                  bool simplifyPaths,
                                  bool realTimeVis,
                                  ValidationRayVisualizationCallback validationRayVisualization,
                                  void* userData,
                                  int& numPaths,
                                  SoundPath* paths,
                                  float* pathWeights,
                                  int* starts,
                                  int* ends);

    void findPathsFromSourceProbeToListenerProbe(const IScene& scene,
                                                 const ProbeBatch& probes,
                                                 const ProbeNeighborhood& listenerProbes,
                                                 const BakedPathData& bakedPathData,
                                                 int sourceProbeIndex,
                                                 float sourceProbeWeight,
                                                 int listenerProbeNeighborhoodIndex,
                                                 float radius,
                                                 float threshold,
                                                 float visRange,
                                                 bool enableValidation,
                                                 bool findAlternatePaths,
                                                 bool simplifyPaths,
                                                 bool realTimeVis,
                                                 ValidationRayVisualizationCallback validationRayVisualization,
                                                 void* userData,
                                                 int& numPaths,
                                                 SoundPath* paths,
                                                 float* pathWeights,
                                                 int* starts,
                                                 int* ends);
    // Validates a baked path.
    bool isPathOccluded(const SoundPath& path,
                        const IScene& scene,
                        const ProbeBatch& probes,
                        float radius,
                        float threshold,
                        int start,
                        int end,
                        bool enableValidation,
                        ValidationRayVisualizationCallback validationRayVisualization,
                        void* userData) const;

    // Given a set of SoundPaths describing multiple paths that reach the listener, and corresponding weights,
    // calculates the SH and EQ coefficients describing the total sound field.
    static void calcAmbisonicsCoeffsForPaths(const Vector3f& source,
                                             const Vector3f& listener,
                                             const ProbeBatch& probes,
                                             int* starts,
                                             int* ends,
                                             int numPaths,
                                             const SoundPath* paths,
                                             const float* weights,
                                             int order,
                                             float* eqGains,
                                             float* coeffs);

    // Given a set of SoundPaths describing multiple paths that reach the listener, and corresponding weights,
    // calculates average direction of the paths.
    static void calcAverageDirectionForPaths(const Vector3f& source,
                                             const Vector3f& listener,
                                             const ProbeBatch& probes,
                                             int* starts,
                                             int* ends,
                                             int numPaths,
                                             const SoundPath* paths,
                                             const float* weights,
                                             Vector3f* avgDirection);

    // Given a set of SoundPaths describing multiple paths that reach the listener, and corresponding weights,
    // calculates average ratio between direct path and pathing path.
    static void calcDistanceRatioForPaths(const Vector3f& source,
                                          const ProbeBatch& probes,
                                          int* starts,
                                          int* ends,
                                          int numPaths,
                                          const SoundPath* paths,
                                          const float* weights,
                                          float* avgDistanceRatio);

    // Calculates EQ coefficients corresponding to a given total deviation angle.
    static void calcDeviationTerm(float deviation,
                                  float* deviationTerm);
};

}
