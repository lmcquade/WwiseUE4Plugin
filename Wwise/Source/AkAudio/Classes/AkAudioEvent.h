// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkEvent.h:
=============================================================================*/
#pragma once

#include "AkAudioEvent.generated.h"

/*------------------------------------------------------------------------------------
	UAkAudioEvent
------------------------------------------------------------------------------------*/
UCLASS(meta=(BlueprintSpawnableComponent))
class AKAUDIO_API UAkAudioEvent : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Bank to which this event should be added. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bank")
	class UAkAudioBank * RequiredBank;

	/** Maximum attenuation radius for this event */
	UPROPERTY(BlueprintReadOnly, Category="AkAudioEvent")
	float MaxAttenuationRadius;

#if CPP
	/**
	 * Called after load process is complete.
	 */
	virtual void PostLoad() override;

	/**
	 * Load the required bank.
	 *
	 * @return true if the bank was loaded, otherwise false
	 */
	bool LoadBank();
#endif

};
