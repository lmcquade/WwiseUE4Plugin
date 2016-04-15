// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAmbientSound.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "Net/UnrealNetwork.h"

/*------------------------------------------------------------------------------------
	AAkAmbientSound
------------------------------------------------------------------------------------*/

/**
 * Static callback for ambient sounds
 */
static void AkAmbientSoundCallback( AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo )
{
	AAkAmbientSound * pObj = ( (AAkAmbientSound *) in_pCallbackInfo->pCookie );
	if( pObj && pObj->IsValidLowLevelFast(false) )
	{
		pObj->Playing( false );
	}
}

AAkAmbientSound::AAkAmbientSound(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	// Property initialization
	StopWhenOwnerIsDestroyed = true;
	CurrentlyPlaying = false;
	
	AkComponent = ObjectInitializer.CreateDefaultSubobject<UAkComponent>(this, TEXT("AkAudioComponent0"));
	
	AkComponent->StopWhenOwnerDestroyed = StopWhenOwnerIsDestroyed;

	RootComponent = AkComponent;

	AkComponent->AttenuationScalingFactor = 1.f;

	//bNoDelete = true;
	bHidden = true;
}

void AAkAmbientSound::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	if( AkAudioEvent_DEPRECATED )
	{
		AkComponent->AkAudioEvent = AkAudioEvent_DEPRECATED;
	}
#endif
}

void AAkAmbientSound::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AkComponent->UpdateAkReverbVolumeList(AkComponent->GetComponentLocation());
}

void AAkAmbientSound::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		// We're about to get destroyed, cancel our callbacks...
		AkAudioDevice->CancelEventCallbackCookie(this);
	}

	Super::EndPlay(EndPlayReason);
}


#if WITH_EDITOR
void AAkAmbientSound::CheckForErrors()
{
	Super::CheckForErrors();
}

void AAkAmbientSound::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if( AkComponent )
	{
		// Reset audio component.
		if( IsCurrentlyPlaying() )
		{
			StartPlaying();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AAkAmbientSound::StartAmbientSound()
{
	StartPlaying();
}

void AAkAmbientSound::StopAmbientSound()
{
	StopPlaying();
}

void AAkAmbientSound::StartPlaying()
{
	if( !IsCurrentlyPlaying() && AkComponent->AkAudioEvent )
	{
		FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			Playing( true );
			AkAudioDevice->SetAttenuationScalingFactor(this, AkComponent->AttenuationScalingFactor);
			if (AkAudioDevice->PostEvent( AkComponent->AkAudioEvent, this, AK_EndOfEvent, &AkAmbientSoundCallback, this, StopWhenOwnerIsDestroyed ) == AK_INVALID_PLAYING_ID)
			{
				Playing( false );
			}
		}
	}
}

void AAkAmbientSound::StopPlaying()
{
	if( IsCurrentlyPlaying() )
	{
		// State of CurrentlyPlaying gets updated in UAkComponent::Stop() through the EndOfEvent callback.
		AkComponent->Stop();
	}
}

void AAkAmbientSound::Playing( bool in_IsPlaying )
{
	FScopeLock Lock(&PlayingCriticalSection);
	CurrentlyPlaying = in_IsPlaying;
}

bool AAkAmbientSound::IsCurrentlyPlaying()
{
	FScopeLock Lock(&PlayingCriticalSection);
	return CurrentlyPlaying;
}
