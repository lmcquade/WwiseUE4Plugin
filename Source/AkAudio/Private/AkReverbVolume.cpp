// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkReverbVolume.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "Net/UnrealNetwork.h"

/*------------------------------------------------------------------------------------
	AAkReverbVolume
------------------------------------------------------------------------------------*/

AAkReverbVolume::AAkReverbVolume(const class FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// Property initialization
	static FName CollisionProfileName(TEXT("OverlapAll"));
	BrushComponent->SetCollisionProfileName(CollisionProfileName);

	bColored = true;
	BrushColor = FColor(0, 255, 255, 255);

	SendLevel = 1.0f;
	FadeRate = 0.5f;
	Priority = 1.0f;

	NextLowerPriorityAkReverbVolume = NULL;

	bEnabled = true;
}

void AAkReverbVolume::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );
	DOREPLIFETIME( AAkReverbVolume, bEnabled );
}

uint32 AAkReverbVolume::GetAuxBusId() const
{
	if (AuxBus)
	{
		return AuxBus->GetAuxBusId();
	}
	else
	{
		return AK::SoundEngine::GetIDFromString(TCHAR_TO_ANSI(*AuxBusName));
	}
}


#if WITH_EDITOR
void AAkReverbVolume::CheckForErrors()
{
	Super::CheckForErrors();
}

void AAkReverbVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SendLevel = FMath::Clamp<float>( SendLevel, 0.0f, 1.0f );
	if( FadeRate < 0.f )
	{
		FadeRate = 0.f;
	}
}
#endif // WITH_EDITOR

void AAkReverbVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		AkAudioDevice->AddAkReverbVolumeInList(this);
	}
}

void AAkReverbVolume::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		AkAudioDevice->RemoveAkReverbVolumeFromList(this);
	}
}



