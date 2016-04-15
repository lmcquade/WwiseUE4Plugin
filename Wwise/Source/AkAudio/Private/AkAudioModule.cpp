// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

#include "AkAudioDevice.h"
#include "AkAudioModule.h"

IMPLEMENT_MODULE( FAkAudioModule, AkAudio )

void FAkAudioModule::StartupModule()
{
	AkAudioDevice = new FAkAudioDevice;

	if (!AkAudioDevice->Init())
	{
		delete AkAudioDevice;
		AkAudioDevice = NULL;
		return;
	}

	OnTick = FTickerDelegate::CreateRaw( AkAudioDevice, &FAkAudioDevice::Update);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker( OnTick );

}

void FAkAudioModule::ShutdownModule()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if (AkAudioDevice) 
	{
		AkAudioDevice->Teardown();
		delete AkAudioDevice;
		AkAudioDevice = NULL;
	}
}

FAkAudioDevice * FAkAudioModule::GetAkAudioDevice()
{ 
	return AkAudioDevice;
}
