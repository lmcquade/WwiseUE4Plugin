// Copyright 1998-2012 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudiokineticTools : ModuleRules
{
    public AudiokineticTools(TargetInfo Target)
	{
		PrivateIncludePaths.Add("AudiokineticTools/Private");
        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "TargetPlatform",
                "MainFrame",
                "LevelEditor"
            });

        PublicIncludePathModuleNames.AddRange(
            new string[] 
            { 
                "AssetTools",
                "ContentBrowser",
                "Matinee"
            });

        PublicDependencyModuleNames.AddRange(
            new string[] 
            { 
                "AkAudio",
                "Core",
                "InputCore",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "Matinee",
                "EditorStyle",
				"Json",
				"XmlParser",
				"WorkspaceMenuStructure",
				"DirectoryWatcher",
                "PropertyEditor"
            });

        CircularlyReferencedDependentModules.Add("AkAudio");

	}
}
