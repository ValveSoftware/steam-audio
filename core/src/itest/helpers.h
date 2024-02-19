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

#include <audio_buffer.h>
#include <hrtf_database.h>
#include <scene_factory.h>
using namespace ipl;

std::shared_ptr<HRTFDatabase> loadHRTF(shared_ptr<Context> context,
                                       float volume,
                                       HRTFNormType normType,
                                       int samplingRate,
                                       int frameSize,
                                       const char* sofaFileName = nullptr);

std::shared_ptr<IScene> loadMesh(shared_ptr<Context> context,
                                 const std::string& fileName,
                                 const std::string& materialFileName,
                                 const SceneType& sceneType,
                                 ClosestHitCallback closestHit = nullptr,
                                 AnyHitCallback anyHit = nullptr,
                                 BatchedClosestHitCallback batchedClosestHit = nullptr,
                                 BatchedAnyHitCallback batchedAnyHit = nullptr,
                                 void* userData = nullptr,
                                 shared_ptr<EmbreeDevice> embree = nullptr,
								 shared_ptr<ipl::RadeonRaysDevice> radeonRays = nullptr);

std::vector<std::string> listMeshFileNames(const std::string& subdirectory);