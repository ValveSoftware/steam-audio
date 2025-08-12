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

using System.Data;
using System.Linq;
using UnityEditor;
using UnityEngine;

namespace SteamAudio
{

    [CustomEditor(typeof(SteamAudioReverbData)), CanEditMultipleObjects]
    public class ReverbDataEditor : Editor
    {
        private string mReverbTimeFoldoutKey = "SteamAudioProbeInspector_ReverbTimeFoldoutState";
        private string mEnergyFieldFoldoutKey = "SteamAudioProbeInspector_EnergyFieldFoldoutState";
        private string mImpulseResponseFoldoutKey = "SteamAudioProbeInspector_ImpulseResponseFoldoutState";
        public bool mOwnerIsMultiEditing = false; // Typically set by parent.


        public override void OnInspectorGUI()
        {
            var tgt = target as SteamAudioReverbData;
            bool bReverbTimesFoldoutExpanded = SessionState.GetBool(mReverbTimeFoldoutKey, false);
            bReverbTimesFoldoutExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(bReverbTimesFoldoutExpanded, "Reverb Times");
            if (bReverbTimesFoldoutExpanded)
            {
                if (serializedObject.isEditingMultipleObjects | mOwnerIsMultiEditing)
                {
                    EditorGUILayout.HelpBox("Multiple objects selected.", MessageType.Info);
                }
                else if (tgt.reverbTimes == null || tgt.reverbTimes.Length == 0)
                {
                    EditorGUILayout.HelpBox("No reverb time data to display.", MessageType.Info);
                }
                else
                {
                    for (int i = 0; i < tgt.reverbTimes.Length; ++i)
                    {
                        EditorGUILayout.LabelField("Band " + i + " (seconds)", tgt.reverbTimes[i].ToString("0.000"));
                    }
                }
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
            SessionState.SetBool(mReverbTimeFoldoutKey, bReverbTimesFoldoutExpanded);

            EditorGUILayout.Space();
            bool bEnergyFieldFoldoutExpanded = SessionState.GetBool(mEnergyFieldFoldoutKey, false);
            bEnergyFieldFoldoutExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(bEnergyFieldFoldoutExpanded, "Reverb Energy Field Stats");
            if (bEnergyFieldFoldoutExpanded)
            {
                if (serializedObject.isEditingMultipleObjects || mOwnerIsMultiEditing)
                {
                    EditorGUILayout.HelpBox("Multiple objects selected.", MessageType.Info);
                }
                else if (tgt.reverbEnergyField == null || tgt.reverbEnergyField.Length == 0)
                {
                    EditorGUILayout.HelpBox("No energy field data to display.", MessageType.Info);
                }
                else
                {
                    string sampleCountString = tgt.reverbEnergyField.Length.ToString();
                    sampleCountString += " (" + tgt.reverbEnergyFieldNumChannels + (tgt.reverbEnergyFieldNumChannels > 1 ? " channels" : " channel");
                    sampleCountString += " x " + tgt.reverbEnergyFieldNumBands + " bands";
                    sampleCountString += " x " + tgt.reverbEnergyFieldNumBins + " bins)";
                    EditorGUILayout.LabelField("Sample Count", sampleCountString);
                    EditorGUILayout.LabelField("Total Size", Common.HumanReadableDataSize(tgt.GetEnergyFieldSize()));

                    int numChannels = tgt.reverbEnergyFieldNumChannels;
                    int numBands = tgt.reverbEnergyFieldNumBands;
                    int numSamples = tgt.reverbEnergyFieldNumBins;
                    float[] channelEnergy = new float[numChannels];
                    for (int i = 0; i < numChannels; ++i)
                    {
                        channelEnergy[i] = .0f;
                        for (int j = 0; j < numBands; ++j)
                        {
                            for (int k = 0; k < numSamples; ++k)
                            {
                                channelEnergy[i] += tgt.GetEnergyFieldData(i, j, k);
                            }
                        }
                    }
                    EditorGUILayout.LabelField("Per Channel Energy", string.Join(", ", channelEnergy.Select(value => value.ToString("e2"))));
                }
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
            SessionState.SetBool(mEnergyFieldFoldoutKey, bEnergyFieldFoldoutExpanded);

            EditorGUILayout.Space();
            bool bImpulseResponseFoldoutExpanded = SessionState.GetBool(mImpulseResponseFoldoutKey, false);
            bImpulseResponseFoldoutExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(bImpulseResponseFoldoutExpanded, "Reverb Impulse Response Stats");
            if (bImpulseResponseFoldoutExpanded)
            {
                if (serializedObject.isEditingMultipleObjects || mOwnerIsMultiEditing)
                {
                    EditorGUILayout.HelpBox("Multiple objects selected.", MessageType.Info);
                }
                else if (tgt.reverbIR == null || tgt.reverbIR.Length == 0)
                {
                    EditorGUILayout.HelpBox("No impulse response data to display.", MessageType.Info);
                }
                else
                {
                    string sampleCountString = tgt.reverbIR.Length.ToString();
                    sampleCountString += " (" + tgt.reverbIRNumChannels + (tgt.reverbIRNumChannels > 1 ? " channels" : " channel");
                    sampleCountString += " x " + tgt.reverbIRNumSamples + " samples)";
                    EditorGUILayout.LabelField("Sample Count", sampleCountString);
                    EditorGUILayout.LabelField("Total Size", Common.HumanReadableDataSize(tgt.GetImpulseResponseSize()));

                    int numChannels = tgt.reverbIRNumChannels;
                    int numSamples = tgt.reverbIRNumSamples;
                    float[] channelMax = new float[numChannels];
                    float[] channelMin = new float[numChannels];
                    for (int i = 0; i < numChannels; ++i)
                    {
                        channelMax[i] = float.MinValue;
                        channelMin[i] = float.MaxValue;
                        for (int k = 0; k < numSamples; ++k)
                        {
                            channelMax[i] = Mathf.Max(tgt.GetImpulseResponseData(i, k), channelMax[i]);
                            channelMin[i] = Mathf.Min(tgt.GetImpulseResponseData(i, k), channelMin[i]);
                        }
                    }

                    EditorGUILayout.LabelField("Per Channel Min", string.Join(", ", channelMin.Select(value => value.ToString("e2"))));
                    EditorGUILayout.LabelField("Per Channel Max", string.Join(", ", channelMax.Select(value => value.ToString("e2"))));
                }
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
            SessionState.SetBool(mImpulseResponseFoldoutKey, bImpulseResponseFoldoutExpanded);
        }
    }
}