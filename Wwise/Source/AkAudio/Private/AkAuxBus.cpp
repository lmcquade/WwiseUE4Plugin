// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkReverbVolume.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "Net/UnrealNetwork.h"

/*------------------------------------------------------------------------------------
	UAkAuxBus
------------------------------------------------------------------------------------*/

UAkAuxBus::UAkAuxBus(const class FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// Property initialization
	AuxBusId = AK::SoundEngine::GetIDFromString(TCHAR_TO_ANSI(*GetName()));
}

