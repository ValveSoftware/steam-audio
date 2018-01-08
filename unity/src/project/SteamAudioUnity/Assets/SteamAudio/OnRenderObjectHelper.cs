using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class OnRenderObjectHelper : MonoBehaviour {

	[HideInInspector] public bool drawMeshes = true;
	[HideInInspector] public bool showWireframe = true;
	[HideInInspector] public Mesh[] meshes;
	[HideInInspector] public Matrix4x4[] matrices;

	Material matSolid;
	Material matWireframe;

	void OnRenderObject() {
		if( drawMeshes == false )
			return;
		if( meshes == null || matrices == null )
			return;

		if( matSolid == null ) {
			matSolid = new Material( Shader.Find("Shader Forge/AudioMeshVisualizer") );
			matSolid.hideFlags = HideFlags.HideAndDontSave;
		}
		if( matWireframe == null ) {
			matWireframe = new Material( Shader.Find( "Shader Forge/AudioMeshVisualizerWireframe" ) );
			matWireframe.hideFlags = HideFlags.HideAndDontSave;
		}
		matSolid.SetPass( 0 );
		for( int i = 0; i < meshes.Length; i++ )
			Graphics.DrawMeshNow( meshes[i], matrices[i] );


		if( showWireframe ) {
			GL.wireframe = true;
			matWireframe.SetPass( 0 );
			for( int i = 0; i < meshes.Length; i++ )
				Graphics.DrawMeshNow( meshes[i], matrices[i] );
			GL.wireframe = false;
		}

	}
}
