// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAmbientSound.h:
=============================================================================*/
#pragma once

#include "AkAmbientSound.generated.h"

/*------------------------------------------------------------------------------------
	FAkAmbSoundCheckpointRecord
------------------------------------------------------------------------------------*/
USTRUCT()
struct FAkAmbSoundCheckpointRecord
{
	GENERATED_USTRUCT_BODY(FAkAmbSoundCheckpointRecord)

	UPROPERTY()
	bool bCurrentlyPlaying;

#if CPP
	FAkAmbSoundCheckpointRecord()
		: bCurrentlyPlaying(false)
	{
	}

#endif
};

/*------------------------------------------------------------------------------------
	AAkAmbientSound
------------------------------------------------------------------------------------*/
UCLASS(config=Engine, hidecategories=Audio, AutoExpandCategories=AkAmbientSound, BlueprintType)
class AKAUDIO_API AAkAmbientSound : public AActor
{
	GENERATED_UCLASS_BODY()


	/** AkAudioEvent to play. Deprecated as UE4.7 integration: Use AkComponent->AkAudioEvent instead */
	UPROPERTY()
	class UAkAudioEvent * AkAudioEvent_DEPRECATED;

	/** AkComponent to handle playback */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AkAmbientSound, meta=(ShowOnlyInnerProperties) )
	class UAkComponent* AkComponent;
	
	/** Stop playback if the owner is destroyed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AkAmbientSound, SimpleDisplay)
	bool StopWhenOwnerIsDestroyed;

	/*
	 * Start an Ak ambient sound.
	 */
	UFUNCTION(BlueprintCallable, Category="Audiokinetic|AkAmbientSound")
	void StartAmbientSound();

	/*
	 * Stop an Ak ambient sound.
	 */
	UFUNCTION(BlueprintCallable, Category="Audiokinetic|AkAmbientSound")
	void StopAmbientSound();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


#if CPP
public:

	/**
	 * Start the ambience playback
	 */
	void StartPlaying();

	/**
	 * Stop the ambience playback
	 */
	void StopPlaying();

	/**
	 * Set whether this ambient sound is playing
	 *
	 * @param in_IsPlaying		New playback status
	 */
	void Playing( bool in_IsPlaying );

	/**
	 * Is whether this ambient sound currently playing
	 *
	 * @return		True if ambient sound is currently playing, false if not.
	 */
	bool IsCurrentlyPlaying();


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

	virtual void PostInitializeComponents() override;
	virtual void PostLoad() override;

#endif

private:
	/** used to update status of toggleable level placed ambient sounds on clients */
	bool CurrentlyPlaying;

	FCriticalSection PlayingCriticalSection;
};
