//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public class Simulator
    {
        IntPtr mSimulator = IntPtr.Zero;

        public Simulator(Context context, SimulationSettings simulationSettings)
        {
            var status = API.iplSimulatorCreate(context.Get(), ref simulationSettings, out mSimulator);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create simulator. [{0}]", status));
        }

        public Simulator(Simulator simulator)
        {
            mSimulator = API.iplSimulatorRetain(simulator.mSimulator);
        }

        ~Simulator()
        {
            Release();
        }

        public void Release()
        {
            API.iplSimulatorRelease(ref mSimulator);
        }

        public IntPtr Get()
        {
            return mSimulator;
        }

        public void SetScene(Scene scene)
        {
            API.iplSimulatorSetScene(mSimulator, scene.Get());
        }

        public void AddProbeBatch(ProbeBatch probeBatch)
        {
            API.iplSimulatorAddProbeBatch(mSimulator, probeBatch.Get());
        }

        public void RemoveProbeBatch(ProbeBatch probeBatch)
        {
            API.iplSimulatorRemoveProbeBatch(mSimulator, probeBatch.Get());
        }

        public void SetSharedInputs(SimulationFlags flags, SimulationSharedInputs sharedInputs)
        {
            API.iplSimulatorSetSharedInputs(mSimulator, flags, ref sharedInputs);
        }

        public void Commit()
        {
            API.iplSimulatorCommit(mSimulator);
        }

        public void RunDirect()
        {
            API.iplSimulatorRunDirect(mSimulator);
        }

        public void RunReflections()
        {
            API.iplSimulatorRunReflections(mSimulator);
        }

        public void RunPathing()
        {
            API.iplSimulatorRunPathing(mSimulator);
        }
    }

    public class Source
    {
        IntPtr mSource = IntPtr.Zero;

        public Source(Simulator simulator, SimulationSettings simulationSettings)
        {
            var sourceSettings = new SourceSettings { };
            sourceSettings.flags = simulationSettings.flags;

            var status = API.iplSourceCreate(simulator.Get(), ref sourceSettings, out mSource);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create source for simulation. [{0}]", status));
        }

        public Source(Source source)
        {
            mSource = API.iplSourceRetain(source.mSource);
        }

        ~Source()
        {
            Release();
        }

        public void Release()
        {
            API.iplSourceRelease(ref mSource);
        }

        public IntPtr Get()
        {
            return mSource;
        }

        public void AddToSimulator(Simulator simulator)
        {
            API.iplSourceAdd(mSource, simulator.Get());
        }

        public void RemoveFromSimulator(Simulator simulator)
        {
            API.iplSourceRemove(mSource, simulator.Get());
        }

        public void SetInputs(SimulationFlags flags, SimulationInputs inputs)
        {
            API.iplSourceSetInputs(mSource, flags, ref inputs);
        }

        public SimulationOutputs GetOutputs(SimulationFlags flags)
        {
            var outputs = new SimulationOutputs { };
            API.iplSourceGetOutputs(mSource, flags, ref outputs);
            return outputs;
        }
    }
}
