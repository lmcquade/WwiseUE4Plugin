Welcome to the Wwise Unreal Engine 4 plug-in!
=================================================

This is the source code page for the Wwise plug-in for Unreal Engine 4.

It is compitable with the Windows and Mac OS X editors, and and with Windows, Mac OS X, Linux, Playstation 4, Xbox One, Android, and iOS games.

If you need any help, please refer to the [online documentation](https://www.audiokinetic.com/library/2015.1.6_5553/?source=UE4&id=index.html) for the Wwise Unreal integration. You can also visit the [Q&A section](https://www.audiokinetic.com/qa/) on the Audiokinetic website.


Branches
--------

Official release branches are named after the Unreal Engine version and the associated Wwise version. For example, the branch named "UE4.11_Wwise2015.1.6" requires both Unreal Engine 4.11, and Wwise 2015.1.6. These branches are fully QA tested.

The "Master" branch is the main development branch. It is not QA tested, and requirements might change often in this branch. Use at your own risk!

The "*_Preview" branches are branches compatible with the various UE4 previews. They are minimally tested. Use at your own risk!


Source releases
---------------

This integration is compatible with Unreal Engine 4.11 and its patch releases, and is based on Wwise 2015.1.6. You can download the latest Wwise version [here](http://www.audiokinetic.com/downloads/).

This repository only contains the source code for the plug-in. If you wish to download a pre-built version of the plug-in, please visit our [downloads page](http://www.audiokinetic.com/downloads/).

Installation
------------

**NOTE:** There are two ways of installing the Unreal Wwise plug-in: either as an engine plug-in or as an installed plug-in - not both.

For more information on the difference between engine and installed plug-ins, please refer to the [Unreal Engine Wiki](https://wiki.unrealengine.com/An_Introduction_to_UE4_Plugins#Engine_vs._Installed).

1. Copy the "Wwise" folder to the "Plugins" folder. To install the UnrealEngine4 Wwise plug-in as an:
	* Engine plug-in, copy the "Wwise" UE4 Integration folder to `…/<UE4 installation directory>/Engine/Plugins`. 
	* Installed plug-in, copy the "Wwise" UE4 Integration folder to `…/<UE4 project directory>/Plugins`.
2. Since the Wwise UE4 integration may need to be rebuilt on the fly by the Unreal Editor during the packaging process, several folders from the Wwise SDK installation folder must be copied into the "Wwise" plug-in folder hierarchy. On Windows, the Wwise SDK will be found at `C:\Program Files (x86)\Audiokinetic\Wwise v2016.1.6 build 5553\SDK`, and on Mac, the Wwise SDK is found at `~/Wwise/Wwise v2016.1.6 build 5553/SDK`. The following folders are required:
	* Include files:
		* Source: `WWISESDK\include\*.*`
		* Destination: `.../Plugins/Wwise/ThirdParty/include`
	* SoundEngine samples
		* Source: `WWISESDK\samples\SoundEngine\*.*`
		* Destination: `.../Plugins/Wwise/ThirdParty/samples/SoundEngine`
	* Windows SDK
		* Source: `WWISESDK\x64_*\*.*`
		* Destination: `.../Plugins/Wwise/ThirdParty/x64_*`
	* Mac SDK
		* Source: `WWISESDK\Mac\*.*`
		* Destination: `.../Plugins/Wwise/ThirdParty/Mac`

For Android, iOS, Linux, PlayStation 4 and Xbox One, the libraries must also be copied:

* Source: `WWISESDK\<Your Platform>\*.*`
* Destination: `.../Plugins/Wwise/ThirdParty/<Your Platform>`



Example Game
------------

This integration showcases its fundamental concepts in a bundled demo game. For more information, please refer to the [Sample Game](https://www.audiokinetic.com/library/2015.1.4_5497/?source=UE4&id=using__samplegame.html) section of the documentation.