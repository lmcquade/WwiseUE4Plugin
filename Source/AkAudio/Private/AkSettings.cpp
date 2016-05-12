// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AkAudioDevice.h"
#include "AkSettings.h"

//////////////////////////////////////////////////////////////////////////
// UAkSettings

UAkSettings::UAkSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxSimultaneousReverbVolumes(4)
{
	TCHAR WwiseDir[MAX_PATH];
	FPlatformMisc::GetEnvironmentVariable(TEXT("WWISEROOT"), WwiseDir, MAX_PATH);

	WwiseWindowsInstallationPath.Path = FString(WwiseDir);
    bRequestRefresh = false;
}

#if WITH_EDITOR
#include "SDockTab.h"
//#include "WwisePicker/SWwisePicker.h"

void UAkSettings::PreEditChange(UProperty* PropertyAboutToChange)
{
	PreviousWwiseProjectPath = WwiseProjectPath.FilePath;
	PreviousWwiseMacPath = WwiseMacInstallationPath.FilePath;
	PreviousWwiseWindowsPath = WwiseWindowsInstallationPath.Path;
}

void UAkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if ( PropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, MaxSimultaneousReverbVolumes) )
	{
		MaxSimultaneousReverbVolumes = FMath::Clamp<uint8>( MaxSimultaneousReverbVolumes, 0, AK_MAX_AUX_PER_OBJ );
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			AkAudioDevice->SetMaxAuxBus(MaxSimultaneousReverbVolumes);
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseWindowsInstallationPath))
	{
		WwiseWindowsInstallationPath.Path = WwiseWindowsInstallationPath.Path.Trim();
		WwiseWindowsInstallationPath.Path = WwiseWindowsInstallationPath.Path.TrimTrailing();

		FText FailReason;
		if (!FPaths::ValidatePath(WwiseWindowsInstallationPath.Path, &FailReason))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
			WwiseWindowsInstallationPath.Path = PreviousWwiseWindowsPath;
		}

		if (!FPaths::DirectoryExists(WwiseWindowsInstallationPath.Path))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Please enter a valid Wwise Authoring Windows executable path"));
			WwiseWindowsInstallationPath.Path = PreviousWwiseWindowsPath;
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseMacInstallationPath))
	{
		WwiseMacInstallationPath.FilePath = WwiseMacInstallationPath.FilePath.Trim();
		WwiseMacInstallationPath.FilePath = WwiseMacInstallationPath.FilePath.TrimTrailing();

		FText FailReason;
		if (!FPaths::ValidatePath(WwiseMacInstallationPath.FilePath, &FailReason))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
			WwiseMacInstallationPath.FilePath = PreviousWwiseMacPath;
		}

		if (!FPaths::DirectoryExists(WwiseMacInstallationPath.FilePath))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Please enter a valid Wwise Authoring Mac executable path"));
			WwiseMacInstallationPath.FilePath = PreviousWwiseMacPath;
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseProjectPath))
	{
		WwiseProjectPath.FilePath = WwiseProjectPath.FilePath.Trim();
		WwiseProjectPath.FilePath = WwiseProjectPath.FilePath.TrimTrailing();

		FString TempPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*WwiseProjectPath.FilePath);
		
		FText FailReason;
		if (!FPaths::ValidatePath(TempPath, &FailReason))
		{
            if(EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, FailReason))
            {
                WwiseProjectPath.FilePath = PreviousWwiseProjectPath;
                return;
            }
		}

		if (!FPaths::FileExists(TempPath))
		{
			// Path might be a valid one (relative to game) entered manually. Check that.
			TempPath = FPaths::ConvertRelativePathToFull(FPaths::GameDir(), WwiseProjectPath.FilePath);

			if (!FPaths::FileExists(TempPath))
			{
				if (EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Please enter a valid Wwise project")))
				{
					WwiseProjectPath.FilePath = PreviousWwiseProjectPath;
					return;
				}
			}
		}

		// Make the path relative to the game dir
		FPaths::MakePathRelativeTo(TempPath, *FPaths::GameDir());
        WwiseProjectPath.FilePath = TempPath;

		if (WwiseProjectPath.FilePath != PreviousWwiseProjectPath)
		{
			TSharedRef<SDockTab> WwisePickerTab = FGlobalTabmanager::Get()->InvokeTab(FName("WwisePicker"));
            bRequestRefresh = true;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

