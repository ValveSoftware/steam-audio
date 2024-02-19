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

#include "error.h"
#include "containers.h"
#include "context.h"
#include "profiler.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"
#include "phonon_interfaces.h"

namespace api {

class CProbeNeighborhood;

// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

class CContext : public IContext
{
public:
    Handle<Context> mHandle;

    static bool isVersionCompatible(IPLuint32 version);

    static IPLerror createContext(IPLContextSettings* settings,
                                  IContext** context);

    CContext(IPLContextSettings* settings);

    virtual IContext* retain() override;

    virtual void release() override;

    virtual void setProfilerContext(void* profilerContext) override;

    virtual IPLVector3 calculateRelativeDirection(IPLVector3 sourcePosition,
                                                  IPLVector3 listenerPosition,
                                                  IPLVector3 listenerAhead,
                                                  IPLVector3 listenerUp) override;

    virtual IPLerror createSerializedObject(IPLSerializedObjectSettings* settings,
                                            ISerializedObject** serializedObject) override;

    virtual IPLerror createEmbreeDevice(IPLEmbreeDeviceSettings* settings,
                                        IEmbreeDevice** device) override;

    virtual IPLerror createOpenCLDeviceList(IPLOpenCLDeviceSettings* settings,
                                            IOpenCLDeviceList** deviceList) override;

    virtual IPLerror createOpenCLDevice(IOpenCLDeviceList* deviceList,
                                        IPLint32 index,
                                        IOpenCLDevice** device) override;

    virtual IPLerror createOpenCLDeviceFromExisting(void* convolutionQueue,
                                                    void* irUpdateQueue,
                                                    IOpenCLDevice** device) override;

    virtual IPLerror createScene(IPLSceneSettings* settings,
                                 IScene** scene) override;

    virtual IPLerror loadScene(IPLSceneSettings* settings,
                               ISerializedObject* serializedObject,
                               IPLProgressCallback progressCallback,
                               void* userData,
                               IScene** scene) override;

    virtual IPLerror allocateAudioBuffer(IPLint32 numChannels,
                                         IPLint32 numSamples,
                                         IPLAudioBuffer* audioBuffer) override;

    virtual void freeAudioBuffer(IPLAudioBuffer* audioBuffer) override;

    virtual void interleaveAudioBuffer(IPLAudioBuffer* src,
                                       IPLfloat32* dst) override;

    virtual void deinterleaveAudioBuffer(IPLfloat32* src,
                                         IPLAudioBuffer* dst) override;

    virtual void mixAudioBuffer(IPLAudioBuffer* in,
                                IPLAudioBuffer* mix) override;

    virtual void downmixAudioBuffer(IPLAudioBuffer* in,
                                    IPLAudioBuffer* out) override;

    virtual void convertAmbisonicAudioBuffer(IPLAmbisonicsType inType,
                                             IPLAmbisonicsType outType,
                                             IPLAudioBuffer* in,
                                             IPLAudioBuffer* out) override;

    virtual IPLerror createHRTF(IPLAudioSettings* audioSettings,
                                IPLHRTFSettings* hrtfSettings,
                                IHRTF** hrtf) override;

    virtual IPLerror createPanningEffect(IPLAudioSettings* audioSettings,
                                         IPLPanningEffectSettings* effectSettings,
                                         IPanningEffect** effect) override;

    virtual IPLerror createBinauralEffect(IPLAudioSettings* audioSettings,
                                          IPLBinauralEffectSettings* effectSettings,
                                          IBinauralEffect** effect) override;

    virtual IPLerror createVirtualSurroundEffect(IPLAudioSettings* audioSettings,
                                                 IPLVirtualSurroundEffectSettings* effectSettings,
                                                 IVirtualSurroundEffect** effect) override;

    virtual IPLerror createAmbisonicsEncodeEffect(IPLAudioSettings* audioSettings,
                                                  IPLAmbisonicsEncodeEffectSettings* effectSettings,
                                                  IAmbisonicsEncodeEffect** effect) override;

    virtual IPLerror createAmbisonicsPanningEffect(IPLAudioSettings* audioSettings,
                                                   IPLAmbisonicsPanningEffectSettings* effectSettings,
                                                   IAmbisonicsPanningEffect** effect) override;

    virtual IPLerror createAmbisonicsBinauralEffect(IPLAudioSettings* audioSettings,
                                                    IPLAmbisonicsBinauralEffectSettings* effectSettings,
                                                    IAmbisonicsBinauralEffect** effect) override;

    virtual IPLerror createAmbisonicsRotationEffect(IPLAudioSettings* audioSettings,
                                                    IPLAmbisonicsRotationEffectSettings* effectSettings,
                                                    IAmbisonicsRotationEffect** effect) override;

    virtual IPLerror createAmbisonicsDecodeEffect(IPLAudioSettings* audioSettings,
                                                  IPLAmbisonicsDecodeEffectSettings* effectSettings,
                                                  IAmbisonicsDecodeEffect** effect) override;

    virtual IPLerror createDirectEffect(IPLAudioSettings* audioSettings,
                                        IPLDirectEffectSettings* effectSettings,
                                        IDirectEffect** effect) override;

    virtual IPLerror createReflectionEffect(IPLAudioSettings* audioSettings,
                                            IPLReflectionEffectSettings* effectSettings,
                                            IReflectionEffect** effect) override;

    virtual IPLerror createReflectionMixer(IPLAudioSettings* audioSettings,
                                           IPLReflectionEffectSettings* effectSettings,
                                           IReflectionMixer** mixer) override;

    virtual IPLerror createPathEffect(IPLAudioSettings* audioSettings,
                                      IPLPathEffectSettings* effectSettings,
                                      IPathEffect** effect) override;

    virtual IPLerror createProbeArray(IProbeArray** probeArray) override;

    virtual IPLerror createProbeBatch(IProbeBatch** probeBatch) override;

    virtual IPLerror loadProbeBatch(ISerializedObject* serializedObject,
                                    IProbeBatch** probeBatch) override;

    virtual void bakeReflections(IPLReflectionsBakeParams* params,
                                 IPLProgressCallback progressCallback,
                                 void* userData) override;

    virtual void cancelBakeReflections() override;

    virtual void bakePaths(IPLPathBakeParams* params,
                           IPLProgressCallback progressCallback,
                           void* userData) override;

    virtual void cancelBakePaths() override;

    virtual IPLerror createSimulator(IPLSimulationSettings* settings,
                                     ISimulator** simulator) override;

    virtual IPLfloat32 calculateDistanceAttenuation(IPLVector3 source,
                                                    IPLVector3 listener,
                                                    IPLDistanceAttenuationModel* model) override;

    virtual void calculateAirAbsorption(IPLVector3 source,
                                        IPLVector3 listener,
                                        IPLAirAbsorptionModel* model,
                                        IPLfloat32* airAbsorption) override;

    virtual IPLfloat32 calculateDirectivity(IPLCoordinateSpace3 source,
                                            IPLVector3 listener,
                                            IPLDirectivity* model) override;
};

}
