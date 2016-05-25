// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AudiokineticToolsModule.cpp
=============================================================================*/
#include "AudiokineticToolsPrivatePCH.h"
#include "ModuleManager.h"

// @todo sequencer uobjects: The *.generated.inl should auto-include required headers (they should always have #pragma once anyway)
#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "AkAudioBankFactory.h"
#include "AkAudioEventFactory.h"
#include "AkComponentVisualizer.h"
#include "MatineeModule.h"
#include "MatineeClasses.h"
#include "InterpTrackAkAudioEventHelper.h"
#include "InterpTrackAkAudioRTPCHelper.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "AssetTypeActions_AkAudioBank.h"
#include "AssetTypeActions_AkAudioEvent.h"
#include "AssetTypeActions_AkAuxBus.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "ISettingsModule.h"
#include "AkSettings.h"
#include "AkEventAssetBroker.h"
#include "ComponentAssetBroker.h"
#include "WwisePicker/SWwisePicker.h"
#include "WorkspaceMenuStructureModule.h"
#include "SDockTab.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AkAudio"

extern void AddGenerateAkBanksToBuildMenu(FMenuBuilder& MenuBuilder);
void VerifyAkSettings();

class FAudiokineticToolsModule : public IAudiokineticTools
{
	TSharedRef<SDockTab> CreateWwisePickerWindow(const FSpawnTabArgs& Args)
	{
		return
			SNew(SDockTab)
			.Icon(SWwisePicker::WwiseIcon.Get())
			.Label(LOCTEXT("AkAudioWwisePickerTabTitle", "Wwise Picker"))
			.TabRole(ETabRole::NomadTab)
			.ContentPadding(5)
			[
				SNew(SWwisePicker)
			];
	}

	void OpenOnlineHelp()
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(TEXT("https://www.audiokinetic.com/library/?source=UE4&id=index.html"));
	}

	void AddWwiseHelp(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("AkHelp", LOCTEXT("AkHelpLabel", "Audiokinetic"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AkWwiseHelpEntry", "Wwise Help"),
			LOCTEXT("AkWwiseHelpEntryToolTip", "Shows the online Wwise documentation."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FAudiokineticToolsModule::OpenOnlineHelp)));
		MenuBuilder.EndSection();
	}

	FString GetWwisePluginContentDir()
	{
		FString WwiseContent = TEXT("Wwise/Content");

		if (FPaths::DirectoryExists(FPaths::EnginePluginsDir() / "Wwise") && FPaths::DirectoryExists(FPaths::GamePluginsDir() / "Wwise"))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("InstallConflict", "The Wwise UE4 Integration plug-in is installed in both the UE4 Engine and Game \"Plugins\" folder. This will cause conflicts. Please ensure the Wwise plug-in is installed in only one of the two locations."));
		}

		if (FPaths::DirectoryExists(FPaths::GamePluginsDir() / WwiseContent))
		{
			return FPaths::GamePluginsDir() / WwiseContent;
		}
		else
		{
			return FPaths::EnginePluginsDir() / WwiseContent;
		}
	}

	void VerifyAnimNotifies()
	{
		FString SourceDir = GetWwisePluginContentDir() / TEXT("AnimNotifyRedirectors");
		FString DestDir = FPaths::EngineContentDir() / TEXT("EngineAnimNotifies");
		TArray<FString> SourceFileNames;
		SourceFileNames.Add("AnimNotify_AkEvent");
		SourceFileNames.Add("AnimNotify_AkEventByName");
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FName> FoundReferences;
		bool bFoundAssetsNeedingOldNAnimNotifies = false;
		bool bRedirectorsAreInEngineContentFolder = false;
		bool bRedirectorsAreInPluginContentFolder = false;
		for (int i = 0; i < SourceFileNames.Num(); i++)
		{
			FName AssetName(*(TEXT("/Engine/EngineAnimNotifies/") + SourceFileNames[i]));
			FString FileName = SourceFileNames[i] + TEXT(".uasset");

			// Check if we have animations refecing the old pre-plugin AnimNotifies
			if (AssetRegistryModule.Get().GetReferencers(AssetName, FoundReferences))
			{
				bFoundAssetsNeedingOldNAnimNotifies = true;
			}

			// Check if we have moved the redirectors to the Engine folder
			if (FPaths::FileExists(*(DestDir / FileName)))
			{
				bRedirectorsAreInEngineContentFolder = true;
			}

			// Check if the redirectors are still in the plugin's content folder
			if (FPaths::FileExists(*(SourceDir / FileName)))
			{
				bRedirectorsAreInPluginContentFolder = true;
			}
		}

		// No assets refer to the old AnimNotifies, delete our redirectors and leave
		if (!bFoundAssetsNeedingOldNAnimNotifies)
		{
			// Delete the redirectors
			IFileManager::Get().DeleteDirectory(*SourceDir);
			VerifyAkSettings();
		}
		// We found animations referencing the old AnimNotifies, and we have not put the redirectors in place.
		// Copy the redirectors, and ask for a project reload, so that the references are fixed.
		else if (bFoundAssetsNeedingOldNAnimNotifies && bRedirectorsAreInPluginContentFolder)
		{
			for (int i = 0; i < SourceFileNames.Num(); i++)
			{
				FString FileName = SourceFileNames[i] + TEXT(".uasset");
				IFileManager::Get().Move(*(DestDir / FileName), *(SourceDir / FileName), true);
			}

			// Delete the redirectors
			IFileManager::Get().DeleteDirectory(*SourceDir);

			if (EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("NeedReload", "The Wwise Plugin had to modify some asset references for the update process, and a project re-load is required. Would you like to re-load your project now?")))
			{
				const bool bWarn = false;
				FUnrealEdMisc::Get().RestartEditor(bWarn);
			}
		}
		// We found Animations referenching the old AnimNotifies, and the redirectors have been copied. Fix-up the redirectors.
		else if (bFoundAssetsNeedingOldNAnimNotifies && bRedirectorsAreInEngineContentFolder && !bRedirectorsAreInPluginContentFolder)
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			TArray<UObjectRedirector*> Redirectors;
			for (int i = 0; i < SourceFileNames.Num(); i++)
			{
				Redirectors.Add(LoadObject<UObjectRedirector>(NULL, *(TEXT("/Engine/EngineAnimNotifies/") + SourceFileNames[i]), NULL, 0, NULL));
			}
			AssetTools.FixupReferencers(Redirectors);

			// If the only thing in the EngineAnimNotifies folder was our redirectors (now deleted by FixupReferencers), delete the folder.
			TArray<FString> FilesInEngineAnimNotifiesFolder;
			IFileManager::Get().FindFiles(FilesInEngineAnimNotifiesFolder, *DestDir, true, true);
			if (FilesInEngineAnimNotifiesFolder.Num() != 0)
			{
				IFileManager::Get().DeleteDirectory(*DestDir);
			}
			VerifyAkSettings();
		}

		AssetRegistryModule.Get().OnFilesLoaded().Remove(VerifyAnimNotifiesHandle);
	}

	EAssetTypeCategories::Type AudiokineticAssetCategoryBit;
	virtual void StartupModule() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		AudiokineticAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Audiokinetic")), LOCTEXT("AudiokineticAssetCategory", "Audiokinetic"));

		AkAudioBankAssetTypeActions = MakeShareable( new FAssetTypeActions_AkAudioBank(AudiokineticAssetCategoryBit) );
		AssetTools.RegisterAssetTypeActions( AkAudioBankAssetTypeActions.ToSharedRef() );

		AkAudioEventAssetTypeActions = MakeShareable(new FAssetTypeActions_AkAudioEvent(AudiokineticAssetCategoryBit));
		AssetTools.RegisterAssetTypeActions(AkAudioEventAssetTypeActions.ToSharedRef());

		AkAuxBusAssetTypeActions = MakeShareable(new FAssetTypeActions_AkAuxBus(AudiokineticAssetCategoryBit));
		AssetTools.RegisterAssetTypeActions(AkAuxBusAssetTypeActions.ToSharedRef());

		if ( FModuleManager::Get().IsModuleLoaded( "LevelEditor" ) )
		{
			// Extend the build menu to handle Audiokinetic-specific entries
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
			LevelViewportToolbarBuildMenuExtenderAk = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FAudiokineticToolsModule::ExtendBuildContextMenuForAudiokinetic);
			LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders().Add(LevelViewportToolbarBuildMenuExtenderAk);
			LevelViewportToolbarBuildMenuExtenderAkHandle = LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders().Last().GetHandle();

			// Add Wwise to the help menu
			MainMenuExtender = MakeShareable(new FExtender);
			MainMenuExtender->AddMenuExtension("HelpBrowse", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FAudiokineticToolsModule::AddWwiseHelp));
			LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);

		}

		RegisterSettings();

		AkEventBroker = MakeShareable(new FAkEventAssetBroker);
		FComponentAssetBrokerage::RegisterBroker(AkEventBroker, UAkComponent::StaticClass(), true, true);

		UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
		FDirectoryPath WwiseAudioPath;
		WwiseAudioPath.Path = FString(TEXT("WwiseAudio"));
		int32 i;
        for(i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); i++)
		{
            if(PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == WwiseAudioPath.Path)
			{
				break;
			}
		}

        if(i == PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num())
		{
			PackagingSettings->DirectoriesToAlwaysStageAsUFS.Add(WwiseAudioPath);
			PackagingSettings->UpdateDefaultConfigFile();
		}

		if(GUnrealEd != NULL)
		{
			GUnrealEd->RegisterComponentVisualizer(UAkComponent::StaticClass()->GetFName(), MakeShareable(new FAkComponentVisualizer));
		}

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SWwisePicker::WwisePickerTabName, FOnSpawnTab::CreateRaw(this, &FAudiokineticToolsModule::CreateWwisePickerWindow))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		VerifyAnimNotifiesHandle = AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAudiokineticToolsModule::VerifyAnimNotifies);
	}

	virtual void ShutdownModule() override
	{
		// Only unregister if the asset tools module is loaded.  We don't want to forcibly load it during shutdown phase.
		check( AkAudioBankAssetTypeActions.IsValid() );
		check( AkAudioEventAssetTypeActions.IsValid() );
		if( FModuleManager::Get().IsModuleLoaded( "AssetTools" ) )
		{
			FModuleManager::GetModuleChecked< FAssetToolsModule >( "AssetTools" ).Get().UnregisterAssetTypeActions( AkAudioBankAssetTypeActions.ToSharedRef() );
			FModuleManager::GetModuleChecked< FAssetToolsModule >( "AssetTools" ).Get().UnregisterAssetTypeActions( AkAudioEventAssetTypeActions.ToSharedRef() );
		}
		AkAudioBankAssetTypeActions.Reset();
		AkAudioEventAssetTypeActions.Reset();

		// Remove Audiokinetic build menu extenders
		if ( FModuleManager::Get().IsModuleLoaded( "LevelEditor" ) )
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
			LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders().RemoveAll([=](const FLevelEditorModule::FLevelEditorMenuExtender& Extender) { return Extender.GetHandle() == LevelViewportToolbarBuildMenuExtenderAkHandle; });

			if (MainMenuExtender.IsValid())
			{
				LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MainMenuExtender);
			}
		}

		if(GUnrealEd != NULL)
		{
			GUnrealEd->UnregisterComponentVisualizer(UAkComponent::StaticClass()->GetFName());
		}

		FGlobalTabmanager::Get()->UnregisterTabSpawner("Wwise Picker");
	}

	/**
	* Extends the Build context menu with Audiokinetic-specific menu items
	*/
	TSharedRef<FExtender> ExtendBuildContextMenuForAudiokinetic(const TSharedRef<FUICommandList> CommandList)
	{
		TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);
		Extender->AddMenuExtension("LevelEditorGeometry", EExtensionHook::After, CommandList, FMenuExtensionDelegate::CreateStatic(&AddGenerateAkBanksToBuildMenu));
		return Extender.ToSharedRef();
	}

public:
	static FDelegateHandle VerifySettingsDelegate;

private:
	void RegisterSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Wwise",
				LOCTEXT("RuntimeSettingsName", "Wwise"),
				LOCTEXT("RuntimeSettingsDescription", "Configure the Wwise Integration"),
				GetMutableDefault<UAkSettings>()
				);
		}
	}

	/** Asset type actions for Audiokinetic assets.  Cached here so that we can unregister it during shutdown. */
	TSharedPtr< FAssetTypeActions_AkAudioBank > AkAudioBankAssetTypeActions;
	TSharedPtr< FAssetTypeActions_AkAudioEvent > AkAudioEventAssetTypeActions;
	TSharedPtr< FAssetTypeActions_AkAuxBus > AkAuxBusAssetTypeActions;
	TSharedPtr<FExtender> MainMenuExtender;
	FLevelEditorModule::FLevelEditorMenuExtender LevelViewportToolbarBuildMenuExtenderAk;
	FDelegateHandle LevelViewportToolbarBuildMenuExtenderAkHandle;
	FDelegateHandle VerifyAnimNotifiesHandle;

	/** Allow to create an AkComponent when Drag & Drop of an AkEvent */
	TSharedPtr<IComponentAssetBroker> AkEventBroker;
};

IMPLEMENT_MODULE( FAudiokineticToolsModule, AudiokineticTools );

FDelegateHandle FAudiokineticToolsModule::VerifySettingsDelegate;

void VerifyAkSettings()
{
	UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
	if( AkSettings )
	{
		if( AkSettings->WwiseProjectPath.FilePath.IsEmpty() )
		{
			if( EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, LOCTEXT("SettingsNotSet", "Wwise settings do not seem to be set. Would you like to open the settings window to set them?") ) )
			{
				FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Plugins"), FName("Wwise"));
			}
		}
		else
		{
			// First-time plugin migration: Project might be relative to Engine path. Fix-up the path to make it relative to the game.
			FString FullGameDir = FPaths::ConvertRelativePathToFull(FPaths::GameDir());
			FString TempPath = FPaths::ConvertRelativePathToFull(FullGameDir, AkSettings->WwiseProjectPath.FilePath);
			if (!FPaths::FileExists(TempPath))
			{
				AkSettings->WwiseProjectPath.FilePath.Empty();
				if (EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ResetWwisePath", "The Wwise UE4 Integration plug-in's update process requires the Wwise Project Path to be set in the Project Settings dialog.")))
				{
					FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Plugins"), FName("Wwise"));
				}
			}
			else
			{
				FPaths::MakePathRelativeTo(TempPath, *FPaths::GameDir());
				AkSettings->WwiseProjectPath.FilePath = TempPath;
				AkSettings->UpdateDefaultConfigFile();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
