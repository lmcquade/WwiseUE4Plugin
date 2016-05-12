// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAudioClasses.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "Net/UnrealNetwork.h"

/*-----------------------------------------------------------------------------
	UAkGameplayStatics.
-----------------------------------------------------------------------------*/

UAkGameplayStatics::UAkGameplayStatics(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Property initialization
}

class UAkComponent * UAkGameplayStatics::GetAkComponent( class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, EAttachLocation::Type LocationType )
{
	if ( AttachToComponent == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::GetAkComponent: NULL AttachToComponent specified!"));
		return NULL;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		return AkAudioDevice->GetAkComponent( AttachToComponent, AttachPointName, &Location, LocationType );
	}

	return NULL;
}

void UAkGameplayStatics::PostEventAttached(class UAkAudioEvent* in_pAkEvent, class AActor* in_pActor, FName in_attachPointName, bool in_stopWhenAttachedToDestroyed, FString EventName)
{
	if (in_pAkEvent == NULL && EventName.IsEmpty())
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostEventAttached: No Event specified!"));
		return;
	}

	if ( in_pActor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostEventAttached: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	UWorld* CurrentWorld = in_pActor->GetWorld();
	if( CurrentWorld->AllowAudioPlayback() && AkAudioDevice )
	{
		if (in_pAkEvent != NULL) 
		{
			AkAudioDevice->PostEvent(in_pAkEvent, in_pActor, 0, NULL, NULL, in_stopWhenAttachedToDestroyed);
		}
		else
		{
			AkAudioDevice->PostEvent(EventName, in_pActor, 0, NULL, NULL, in_stopWhenAttachedToDestroyed);
		}
	}
}

void UAkGameplayStatics::PostEvent(class UAkAudioEvent* in_pAkEvent, class AActor* in_pActor, bool in_stopWhenAttachedToDestroyed, FString EventName)
{
	if (in_pAkEvent == NULL && EventName.IsEmpty())
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostEvent: No Event specified!"));
		return;
	}

	if ( in_pActor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostEvent: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	UWorld* CurrentWorld = in_pActor->GetWorld();
	if( CurrentWorld->AllowAudioPlayback() && AkAudioDevice )
	{
		if (in_pAkEvent != NULL)
		{
			AkAudioDevice->PostEvent(in_pAkEvent, in_pActor, 0, NULL, NULL, in_stopWhenAttachedToDestroyed);
		}
		else
		{
			AkAudioDevice->PostEvent(EventName, in_pActor, 0, NULL, NULL, in_stopWhenAttachedToDestroyed);
		}
	}
}

void UAkGameplayStatics::PostEventByName(const FString& EventName, class AActor* in_pActor, bool in_stopWhenAttachedToDestroyed)
{
	if ( in_pActor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostEventByName: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	UWorld* CurrentWorld = in_pActor->GetWorld();
	if( CurrentWorld->AllowAudioPlayback() && AkAudioDevice )
	{
		AkAudioDevice->PostEvent(EventName, in_pActor, 0, NULL, NULL, in_stopWhenAttachedToDestroyed);
	}
}

void UAkGameplayStatics::PostEventAtLocation( class UAkAudioEvent* in_pAkEvent, FVector Location, FRotator Orientation, const FString& EventName, UObject* WorldContextObject )
{
	if ( in_pAkEvent == NULL && EventName.IsEmpty() )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostEventAtLocation: No Event specified!"));
		return;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	UWorld* CurrentWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if( CurrentWorld->AllowAudioPlayback() && AkAudioDevice )
	{
		if (in_pAkEvent != NULL)
		{
			AkAudioDevice->PostEventAtLocation(in_pAkEvent, Location, Orientation.Vector(), GEngine->GetWorldFromContextObject(WorldContextObject));
		}
		else
		{
			AkAudioDevice->PostEventAtLocation(EventName, Location, Orientation.Vector(), GEngine->GetWorldFromContextObject(WorldContextObject));
		}
	}
}

void UAkGameplayStatics::PostEventAtLocationByName( const FString& EventName, FVector Location, FRotator Orientation, UObject* WorldContextObject )
{
	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	UWorld* CurrentWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if( CurrentWorld->AllowAudioPlayback() && AkAudioDevice )
	{
		AkAudioDevice->PostEventAtLocation(EventName, Location, Orientation.Vector(), GEngine->GetWorldFromContextObject(WorldContextObject) );
	}
}

UAkComponent* UAkGameplayStatics::SpawnAkComponentAtLocation(UObject* WorldContextObject, class UAkAudioEvent* AkEvent, FVector Location, FRotator Orientation, bool AutoPost, const FString& EventName, bool AutoDestroy /* = true*/ )
{
	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	UWorld* CurrentWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if( CurrentWorld && CurrentWorld->AllowAudioPlayback() && AkAudioDevice )
	{
		return AkAudioDevice->SpawnAkComponentAtLocation(AkEvent, Location, Orientation, AutoPost, EventName, AutoDestroy, CurrentWorld );
	}

	return nullptr;
}

void UAkGameplayStatics::SetRTPCValue( FName RTPC, float Value, int32 InterpolationTimeMs = 0, class AActor* Actor = NULL )
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice && RTPC.IsValid() )
	{
		AudioDevice->SetRTPCValue( *RTPC.ToString(), Value, InterpolationTimeMs, Actor );
	}
}

void UAkGameplayStatics::SetState( FName stateGroup, FName state )
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice && stateGroup.IsValid() && state.IsValid() )
	{
		AudioDevice->SetState( *stateGroup.ToString() , *state.ToString() );
	}
}

void UAkGameplayStatics::PostTrigger( FName Trigger, class AActor* Actor )
{
	if ( Actor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::PostTrigger: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice && Trigger.IsValid() )
	{
		AudioDevice->PostTrigger( *Trigger.ToString(), Actor );
	}
}

void UAkGameplayStatics::SetSwitch( FName SwitchGroup, FName SwitchState, class AActor* Actor )
{
	if ( Actor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::SetSwitch: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice && SwitchGroup.IsValid() && SwitchState.IsValid() )
	{
		AudioDevice->SetSwitch( *SwitchGroup.ToString(), *SwitchState.ToString(), Actor );
	}
}

void UAkGameplayStatics::UseReverbVolumes(bool inUseReverbVolumes, class AActor* Actor )
{
	if ( Actor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::SetOcclusionRefreshInterval: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		UAkComponent * ComponentToSet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
		if( ComponentToSet != NULL )
		{
			ComponentToSet->UseReverbVolumes(inUseReverbVolumes);
		}
	}
}

void UAkGameplayStatics::SetOcclusionRefreshInterval(float RefreshInterval, class AActor* Actor )
{
	if ( Actor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::SetOcclusionRefreshInterval: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		UAkComponent * ComponentToSet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
		if( ComponentToSet != NULL )
		{
			ComponentToSet->OcclusionRefreshInterval = RefreshInterval;
		}
	}
}

void UAkGameplayStatics::StopActor(class AActor* Actor)
{
	if ( Actor == NULL )
	{
		UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::StopActor: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->StopGameObject(AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset));
	}
}

void UAkGameplayStatics::StopAll()
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->StopAllSounds();
	}
}

void UAkGameplayStatics::StartAllAmbientSounds(UObject* WorldContextObject)
{
	UWorld* CurrentWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	for( FActorIterator It(CurrentWorld);It;++It )
	{
		AAkAmbientSound* pAmbientSound = Cast<AAkAmbientSound>(*It);
		if( pAmbientSound != NULL)
		{
			FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
			if( AudioDevice )
			{
				UAkComponent* pComponent = pAmbientSound->AkComponent;
				if( pComponent && GWorld->Scene == pComponent->GetScene() )
				{
					pAmbientSound->StartPlaying();
				}
			}
		}
	}
}

void UAkGameplayStatics::StopAllAmbientSounds(UObject* WorldContextObject)
{
	UWorld* CurrentWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	for( FActorIterator It(CurrentWorld);It;++It)
	{
		AAkAmbientSound* pAmbientSound = Cast<AAkAmbientSound>(*It);
		if( pAmbientSound != NULL)
		{
			FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
			if( AudioDevice )
			{
				UAkComponent* pComponent = pAmbientSound->AkComponent;
				if( pComponent && GWorld->Scene == pComponent->GetScene() )
				{
					pAmbientSound->StopPlaying();
				}
			}
		}
	}
}

void UAkGameplayStatics::ClearBanks()
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->ClearBanks();
	}
}

void UAkGameplayStatics::LoadBank(UAkAudioBank * bank, const FString& BankName)
{
	if ( bank )
	{
		bank->Load();
	}
	else
	{
		LoadBankByName(BankName);
	}
}

void UAkGameplayStatics::LoadBankByName(const FString& BankName)
{
	
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		if(AudioDevice->GetAkBankManager() != NULL )
		{
			for ( TObjectIterator<UAkAudioBank> Iter; Iter; ++Iter )
			{
				if( Iter->GetName() == BankName )
				{
					Iter->Load();
					return;
				}
			}

			// Bank not found in the assets, load it by name anyway
			AkUInt32 bankID;
			AudioDevice->LoadBank(BankName, AK_DEFAULT_POOL_ID, bankID);
		}
		else
		{
			AkUInt32 bankID;
			AudioDevice->LoadBank(BankName, AK_DEFAULT_POOL_ID, bankID);
		}
	}
}

void UAkGameplayStatics::LoadBanks(const TArray<UAkAudioBank *>& SoundBanks, bool SynchronizeSoundBanks)
{
	if( SynchronizeSoundBanks )
	{
		TSet<UAkAudioBank*> BanksToUnload;
		TSet<UAkAudioBank*> BanksToLoad;
		TSet<UAkAudioBank*> InputBankSet(SoundBanks);
		FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			FAkBankManager* BankManager = AkAudioDevice->GetAkBankManager();
			if( BankManager )
			{
				FScopeLock Lock(&BankManager->m_BankManagerCriticalSection);
				const TSet<UAkAudioBank *>* LoadedBanks = BankManager->GetLoadedBankList();

				// We load what's in the input set, but not in the already loaded set
				BanksToLoad = InputBankSet.Difference(*LoadedBanks);

				// We unload what's in the loaded set but not in the input set
				BanksToUnload = LoadedBanks->Difference(InputBankSet);
			}
			else
			{
				UE_LOG(LogScript, Warning, TEXT("UAkGameplayStatics::LoadBanks: Bank Manager unused, and CleanUpBanks set to true!"));
			}
		}
		for(TSet<UAkAudioBank*>::TConstIterator LoadIter(BanksToLoad); LoadIter; ++LoadIter)
		{
			if( *LoadIter != NULL )
			{
				(*LoadIter)->Load();
			}
		}

		for(TSet<UAkAudioBank*>::TConstIterator UnloadIter(BanksToUnload); UnloadIter; ++UnloadIter)
		{
			if( *UnloadIter != NULL )
			{
				(*UnloadIter)->Unload();
			}
		}
	}
	else
	{
		for(TArray<UAkAudioBank*>::TConstIterator LoadIter(SoundBanks); LoadIter; ++LoadIter)
		{
			if( *LoadIter != NULL )
			{
				(*LoadIter)->Load();
			}
		}
	}


}

void UAkGameplayStatics::LoadInitBank()
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->LoadInitBank();
	}
}


void UAkGameplayStatics::UnloadBank(class UAkAudioBank * bank, const FString& BankName)
{
	if ( bank )
	{
		bank->Unload();
	}
	else
	{
		UnloadBankByName(BankName);
	}
}

void UAkGameplayStatics::UnloadBankByName(const FString& BankName)
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		if(AudioDevice->GetAkBankManager() != NULL )
		{
			for ( TObjectIterator<UAkAudioBank> Iter; Iter; ++Iter )
			{
				if( Iter->GetName() == BankName )
				{
					Iter->Unload();
					return;
				}
			}

			
			// Bank not found in the assets, unload it by name anyway
			AudioDevice->UnloadBank(BankName);
		}
		else
		{
			AudioDevice->UnloadBank(BankName);
		}
	}
}

void UAkGameplayStatics::StartOutputCapture(const FString& Filename)
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		FString name = Filename;
		if( !name.EndsWith(".wav") )
		{
			name += ".wav";
		}
		AudioDevice->StartOutputCapture(name);
	} 
}

void UAkGameplayStatics::AddOutputCaptureMarker(const FString& MarkerText)
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->AddOutputCaptureMarker(MarkerText);
	} 
}

void UAkGameplayStatics::StopOutputCapture()
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->StopOutputCapture();
	}
}

void UAkGameplayStatics::StartProfilerCapture(const FString& Filename)
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		FString name = Filename;
		if( !name.EndsWith(".prof") )
		{
			name += ".prof";
		}
		AudioDevice->StartProfilerCapture(name);
	} 
}

void UAkGameplayStatics::StopProfilerCapture()
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->StopProfilerCapture();
	}
}
