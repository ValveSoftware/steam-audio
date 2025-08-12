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

studio.plugins.registerPluginDescription("Steam Audio Spatializer", {
	companyName: "Valve",
	productName: "Steam Audio Spatializer",
	parameters: {
		"DirectBinaural": {displayName: "Apply HRTF To Direct"},
		"ApplyDA": {displayName: "Distance Attenuation"},
		"ApplyAA": {displayName: "Air Absorption"},
		"ApplyDir": {displayName: "Directivity"},
		"ApplyOccl": {displayName: "Occlusion"},
		"ApplyTrans": {displayName: "Transmission"},
		"ApplyRefl": {displayName: "Reflections"},
		"ApplyPath": {displayName: "Pathing"},
		"Interpolation": {displayName: "HRTF Interpolation"},
		"DistAtt": {displayName: "Value"},
		"AirAbsLow": {displayName: "AA Low"},
		"AirAbsMid": {displayName: "AA Mid"},
		"AirAbsHigh": {displayName: "AA High"},
		"Directivity": {displayName: "Dir. Value"},
		"DipoleWeight": {displayName: "Weight"},
		"DipolePower": {displayName: "Power"},
		"Occlusion": {displayName: "Occl. Value"},
		"TransType": {displayName: "Transmission Type"},
		"TransLow": {displayName: "Trans. Low"},
		"TransMid": {displayName: "Trans. Mid"},
		"TransHigh": {displayName: "Trans. High"},
		"DirMixLevel": {displayName: "Direct Mix Level"},
		"ReflBinaural": {displayName: "Apply HRTF To Reflections"},
		"ReflMixLevel": {displayName: "Reflections Mix Level"},
		"PathBinaural": {displayName: "Apply HRTF To Pathing"},
		"PathMixLevel": {displayName: "Pathing Mix Level"},
		"OutputFormat": {displayName: "Output Format"},
		"PathNormEQ": {displayName: "Normalize Pathing EQ"},
	},
	deckUi: {
		deckWidgetType: studio.ui.deckWidgetType.Layout,
		layout: studio.ui.layoutType.HBoxLayout,
		spacing: 8,
		items: [
			{
				deckWidgetType: studio.ui.deckWidgetType.InputMeter
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 150,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "Interpolation"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "ApplyDA"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "ApplyAA"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "ApplyDir"
					},
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 250,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.DistanceRolloffGraph,
						rolloffTypeBinding: "DAType",
						minimumDistanceBinding: "DAMinDist",
						maximumDistanceBinding: "DAMaxDist",
						rolloffTypes: {
							0: studio.project.distanceRolloffType.LinearSquared,
							1: studio.project.distanceRolloffType.Linear,
							2: studio.project.distanceRolloffType.Inverse,
							3: studio.project.distanceRolloffType.InverseSquared,
							4: studio.project.distanceRolloffType.Custom,
						}
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.MinMaxFader,
						text: "Min & Max Distances",
						minimumBinding: "DAMinDist",
						maximumBinding: "DAMaxDist"
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 150,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Label,
						text: "Directivity"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.PolarDirectivityGraph,
						directivityBinding: "DipoleWeight",
						sharpnessBinding: "DipolePower"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Layout,
						layout: studio.ui.layoutType.HBoxLayout,
						items: [
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "DipoleWeight"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "DipolePower"
							}
						]
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 150,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "ApplyOccl"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "ApplyTrans"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "TransType"
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 250,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Layout,
						layout: studio.ui.layoutType.HBoxLayout,
						spacing: 8,
						items: [
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "AirAbsLow"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "AirAbsMid"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "AirAbsHigh"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "Directivity"
							}
						]
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Layout,
						layout: studio.ui.layoutType.HBoxLayout,
						spacing: 8,
						items: [
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "Occlusion"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "TransLow"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "TransMid"
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "TransHigh"
							}
						]
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 150,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "DirectBinaural",
						text: "On"
                    },
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "ApplyRefl",
						text: "On"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "ReflBinaural",
						text: "On"
					},
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 128,
				maximumWidth: 150,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "ApplyPath",
						text: "On"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "PathBinaural",
						text: "On"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "PathNormEQ",
						text: "On"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "OutputFormat"
					},
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				minimumWidth: 225,
				maximumWidth: 350,
				spacing: 8,
				contentsMargins: {left: 4, right: 4},
				alignment: studio.ui.alignment.AlignTop,
				isFramed: true,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Dial,
						binding: "DirMixLevel",
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Layout,
						layout: studio.ui.layoutType.HBoxLayout,
						items: [
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "ReflMixLevel",
							},
							{
								deckWidgetType: studio.ui.deckWidgetType.Dial,
								binding: "PathMixLevel",
							}
						]
					}
				]
			},
			{
				deckWidgetType:  studio.ui.deckWidgetType.OutputMeter
			}
		]
	}
});

studio.plugins.registerPluginDescription("Steam Audio Mixer Return", {
	companyName: "Valve",
	productName: "Steam Audio Mixer Return",
	parameters: {
		"Binaural": {displayName: "Apply HRTF"}
	},
	deckUi: {
		deckWidgetType: studio.ui.deckWidgetType.Layout,
		layout: studio.ui.layoutType.HBoxLayout,
		minimumWidth: 256,
		items: [
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				spacing: 6,
				contentsMargins: {left: 4, right: 4},
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "Binaural",
						text: "On",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "OutputFormat"
					},
				]
			}
		]
	}
});

studio.plugins.registerPluginDescription("Steam Audio Reverb", {
	companyName: "Valve",
	productName: "Steam Audio Reverb",
	parameters: {
		"Binaural": {displayName: "Apply HRTF"},
	},
	deckUi: {
		deckWidgetType: studio.ui.deckWidgetType.Layout,
		layout: studio.ui.layoutType.HBoxLayout,
		minimumWidth: 256,
		items: [
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				spacing: 6,
				contentsMargins: {left: 4, right: 4},
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "Binaural",
						text: "Enable",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "OutputFormat"
					},
				]
			}
		]
	}
});
