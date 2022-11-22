Menu Commands
-------------

All of these commands are available under the **Steam Audio** menu in Unity’s main menu.

Settings
    Opens the Settings asset in the Inspector.

Export Active Scene
    Exports the static geometry of the currently open and active scene to an asset file. When doing this for the first time, you will be prompted for a file name and directory under which to save the asset.

Export All Open Scenes
    Exports the static geometry of all currently open scenes.

Export All Scenes In Build
    Exports the static geometry of all scenes specified in your build settings (**File** > **Build Settings** in the main menu).

Export Active Scene To OBJ
    Exports the static geometry of the currently open and active scene to an OBJ file. This is intended for debugging purposes.

Export Dynamic Objects In Active Scene
    Exports every dynamic object in the currently open and active scene.

Export Dynamic Objects In All Open Scenes
    Exports every dynamic object in all currently open scenes.

Export Dynamic Objects In All Scenes In Build
    Exports every dynamic object in all scenes specified in your build settings (**File** > **Build Settings** in the main menu).

Export All Dynamic Objects In Project
    Exports every dynamic object in every scene or prefab found in your project’s ``Assets`` directory.

Install FMOD Studio Plugin Files
    Copies the Steam Audio FMOD Studio plugin files to appropriate directory based on the version of FMOD Studio being used by your project.

    FMOD Studio versions 2.0 through 2.1 expect plugin files to be under ``Assets/Plugins/FMOD/lib/(platform)``, whereas FMOD Studio 2.2 and later expect plugin files to be under ``Assets/Plugins/FMOD/platforms/(platform)/lib``. Use this menu command to move the files to the appropriate place.

    This is not necessary unless you are using FMOD Studio as your audio engine.
