// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkReverbVolume.h:
=============================================================================*/
#pragma once

#include "AkReverbVolume.generated.h"

/*------------------------------------------------------------------------------------
	AAkReverbVolume
------------------------------------------------------------------------------------*/
UCLASS(hidecategories=(Advanced, Attachment, Volume), BlueprintType)
class AKAUDIO_API AAkReverbVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	/** Whether this volume is currently enabled and able to affect sounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, replicated, Category=Toggle)
	uint32 bEnabled:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category =AKReverbVolume)
	class UAkAuxBus * AuxBus;

	/** Wwise Auxiliary Bus associated to this AkReverbVolume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=AKReverbVolume)
	FString AuxBusName;

	/** Maximum send level to the Wwise Auxiliary Bus associated to this AkReverbVolume */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=AKReverbVolume)
	float SendLevel;

	/** Rate at which to fade in/out the SendLevel of the current Reverb Volume when entering/exiting it, in percentage per second (0.2 will make the fade time 5 seconds) */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=AKReverbVolume)
	float FadeRate;

	/**
	 * The precedence in which the AkReverbVolumes will be applied. In the case of overlapping volumes, only the ones 
	 * with the highest priority are chosen (the number of simultaneous AkReverbVolumes is configurable in the Unreal 
	 * Editor Project Settings under Plugins > Wwise). If two or more overlapping AkReverbVolumes have the same 
	 * priority, the chosen AkReverbVolume is unpredictable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AKReverbVolume)
	float Priority;

	/** Get the AkAuxBusId associated to AuxBusName */
	uint32 GetAuxBusId() const;

	/** We keep a  linked list of ReverbVolumes sorted by priority for faster finding of reverb volumes at a specific location.
	 *	This points to the next volume in the list.
	 */
	UPROPERTY(transient)
	class AAkReverbVolume* NextLowerPriorityAkReverbVolume;

	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;

#if CPP
public:

protected:
	/*------------------------------------------------------------------------------------
		AActor interface.
	------------------------------------------------------------------------------------*/
#if WITH_EDITOR
	/**
	 * Check for errors
	 */
	virtual void CheckForErrors() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#endif
};
