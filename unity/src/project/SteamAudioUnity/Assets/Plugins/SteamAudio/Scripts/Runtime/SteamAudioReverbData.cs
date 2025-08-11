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

using UnityEngine;

namespace SteamAudio
{
    /** Baked reverb data stored for a \c SteamAudioReverbDataPoint.
     */
    public class SteamAudioReverbData : ScriptableObject
    {
        /** Reverb times (in seconds), for use in a parametric reverb effect. One per frequency band. */
        public float[] reverbTimes;

        /** The energy field, stored in a row-major array of size (#channels x #bands x #bins). */
        public float[] reverbEnergyField;

        /** The number of channels in the energy field. */
        public int reverbEnergyFieldNumChannels;

        /** The number of bands in the energy field. */
        public int reverbEnergyFieldNumBands;

        /** The number of bins in the energy field. */
        public int reverbEnergyFieldNumBins;

        /** The impulse response, stored in a row-major array of size (#channels x #samples). */
        public float[] reverbIR;

        /** The number of channels in the impulse response. */
        public int reverbIRNumChannels;

        /** The number of samples in the impulse response. */
        public int reverbIRNumSamples;

        /** Initializes the reverb data to default (empty) values.
         */
        public void Initialize()
        {
            reverbTimes = new float[3];

            reverbEnergyField = null;
            reverbEnergyFieldNumChannels = 0;
            reverbEnergyFieldNumBands = 0;
            reverbEnergyFieldNumBins = 0;

            reverbIR = null;
            reverbIRNumChannels = 0;
            reverbIRNumSamples = 0;
        }

        /** \return The total size of the impulse response (#channels x #samples).
         */
        public int GetImpulseResponseSize()
        {
            return sizeof(float) * reverbIR.Length;
        }

        /** \return The energy value stored for a particular channel, band, and bin.
         * 
         *  \param[in]  channel     The index of the channel.
         *  \param[in]  band        The index of the band.
         *  \param[in]  bin         The index of the bin.
         */
        public float GetEnergyFieldData(int channel, int band, int bin)
        {
            int index = (channel * reverbEnergyFieldNumBands * reverbEnergyFieldNumBins) + band * reverbEnergyFieldNumBins + bin;
            return reverbEnergyField[index];
        }

        /** \return The total size of the energy field (#channels x #bands x #bins).
         */
        public int GetEnergyFieldSize()
        {
            return sizeof(float) * reverbEnergyField.Length;
        }

        /** \return The sample value stored for a particular channel and sample index.
         * 
         *  \param[in]  channel     The index of the channel.
         *  \param[in]  sample      The index of the sample.
         */
        public float GetImpulseResponseData(int channel, int sample)
        {
            int index = (channel * reverbIRNumSamples) + sample;
            return reverbIR[index];
        }
    }
}
