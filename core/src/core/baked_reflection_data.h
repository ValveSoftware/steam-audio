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

#include "energy_field.h"
#include "probe_batch.h"
#include "probe_data.h"
#include "reflection_simulator.h"
#include "reverb_estimator.h"

#include "baked_reflection_data.fbs.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// IBakedReflectionsLookup
// ---------------------------------------------------------------------------------------------------------------------

class IBakedReflectionsLookup : public IBakedData
{
public:
    virtual void evaluateEnergyField(const ProbeNeighborhood& neighborhood, EnergyField& energyField) = 0;

    virtual void evaluateReverb(const ProbeNeighborhood& neighborhood, Reverb& reverb) = 0;
};


// ---------------------------------------------------------------------------------------------------------------------
// BakedReflectionsData
// ---------------------------------------------------------------------------------------------------------------------

class BakedReflectionsData : public IBakedReflectionsLookup
{
public:
    BakedReflectionsData(const BakedDataIdentifier& identifier,
                         int numProbes,
                         bool hasConvolution,
                         bool hasParametric);

    BakedReflectionsData(const BakedDataIdentifier& identifier,
                         int numProbes,
                         const Serialized::BakedReflectionsData* serializedObject);

    virtual void updateProbePosition(int index,
                                     const Vector3f& position) override;

    virtual void addProbe(const Sphere& influence) override;

    virtual void removeProbe(int index) override;

    virtual void updateEndpoint(const BakedDataIdentifier& identifier,
                                const Probe* probes,
                                const Sphere& endpointInfluence) override;

    virtual uint64_t serializedSize() const override;

    virtual void evaluateEnergyField(const ProbeNeighborhood& neighborhood, EnergyField& energyField) override;

    virtual void evaluateReverb(const ProbeNeighborhood& neighborhood, Reverb& reverb) override;

    flatbuffers::Offset<Serialized::BakedReflectionsData> serialize(SerializedObject& serializedObject) const;

    int numProbes() const;

    void setHasConvolution(bool hasConvolution);

    void setHasParametric(bool hasParametric);

    bool needsUpdate(int index) const;

    void set(int index,
             unique_ptr<EnergyField> value);

    void set(int index,
             const Reverb& value);

    EnergyField* lookupEnergyField(int index);

    Reverb* lookupReverb(int index);

    vector<unique_ptr<EnergyField>>& getEnergyFields() { return mEnergyFields; }
    vector<Reverb>& getReverbs() { return mReverbs; }

private:
    BakedDataIdentifier mIdentifier;
    bool mHasConvolution;
    bool mHasParametric;
    vector<unique_ptr<EnergyField>> mEnergyFields;
    vector<Reverb> mReverbs;
    vector<uint8_t> mNeedsUpdate;
};

}
