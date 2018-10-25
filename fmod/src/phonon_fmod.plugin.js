//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

studio.plugins.registerPluginDescription("Steam Audio Spatializer", {
	companyName: "Valve",
	productName: "Steam Audio Spatializer",
	parameters: {
		"DirectBinaural":  {displayName: "Direct Binaural"},
		"Interpolation":   {displayName: "HRTF Interpolation"},
		"Distance":        {displayName: "Physics-Based Attenuation"},
		"AirAbsorption":   {displayName: "Air Absorption"},
		"OcclusionMode":   {displayName: "Occlusion Mode"},
		"OcclusionMethod": {displayName: "Occlusion Method"},
		"SourceRadius":    {displayName: "Source Radius"},
		"DirectLevel":     {displayName: "Direct Mix Level"},
		"Indirect":        {displayName: "Indirect"},
		"IndirBinaural":   {displayName: "Indirect Binaural"},
		"IndirLevel":      {displayName: "Indirect Mix Level"},
		"IndirType":       {displayName: "Simulation Type"},
		"StaticListener":  {displayName: "Static Listener"},
		"DipoleWeight":    {displayName: "Dipole Weight"},
		"DipolePower":     {displayName: "Dipole Power"},
	},
	deckUi: {

		deckWidgetType: studio.ui.deckWidgetType.Layout,
		layout: studio.ui.layoutType.HBoxLayout,
		minimumWidth: 700,
		items: [
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				spacing: 6,
				maximumWidth: 160,
				contentsMargins: {left: 4, right: 4},
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "DirectBinaural",
						text: "Enable",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "Interpolation"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "Distance",
						text: "Enable",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "AirAbsorption",
						text: "Enable",
						buttonWidth: 64
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				spacing: 6,
				maximumWidth: 224,
				alignment: studio.ui.alignment.AlignTop,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "OcclusionMode"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "OcclusionMethod"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dial,
						binding: "SourceRadius"
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Fader,
				binding: "DirectLevel",
				maximumWidth: 64,
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				spacing: 6,
				maximumWidth: 160,
				alignment: studio.ui.alignment.AlignTop,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "Indirect",
						text: "Enable",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "IndirBinaural",
						text: "Enable",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "IndirType"
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "StaticListener",
						text: "Enable",
						buttonWidth: 64
					}
				]
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Fader,
				binding: "IndirLevel",
				maximumWidth: 64,
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Spacer,
			},
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.PolarDirectivityGraph,
						directivityBinding: 'DipoleWeight',
						sharpnessBinding: 'DipolePower',
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Layout,
						layout: studio.ui.layoutType.HBoxLayout,
						spacing: 10,
						items: [
						{
							deckWidgetType: studio.ui.deckWidgetType.NumberBox,
							binding: 'DipoleWeight',
						},
						{
							deckWidgetType: studio.ui.deckWidgetType.NumberBox,
							binding: 'DipolePower',
						},
					]
					}
				]
			}
		]
	}
});

studio.plugins.registerPluginDescription("Steam Audio Mixer Return", {
	companyName: "Valve",
	productName: "Steam Audio Mixer Return",
	parameters: {
		"IndirBinaural": {displayName: "Indirect Binaural"}
	},
	deckUi: {
		deckWidgetType: studio.ui.deckWidgetType.Layout,
		layout: studio.ui.layoutType.HBoxLayout,
		minimumWidth: 150,
		items: [
			{
				deckWidgetType: studio.ui.deckWidgetType.Layout,
				layout: studio.ui.layoutType.VBoxLayout,
				spacing: 6,
				contentsMargins: {left: 4, right: 4},
				items: [
					{
						deckWidgetType: studio.ui.deckWidgetType.Button,
						binding: "IndirBinaural",
						text: "Enable",
						buttonWidth: 64
					}
				]
			}
		]
	}
});

studio.plugins.registerPluginDescription("Steam Audio Reverb", {
	companyName: "Valve",
	productName: "Steam Audio Reverb",
	parameters: {
		"IndirBinaural": {displayName: "Indirect Binaural"},
		"IndirType":     {displayName: "Simulation Type"},
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
						binding: "IndirBinaural",
						text: "Enable",
						buttonWidth: 64
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.Dropdown,
						binding: "IndirType"
					},
			]
			}
		]
	}
});