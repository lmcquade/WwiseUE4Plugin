// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkComponent.cpp:
=============================================================================*/

#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "Net/UnrealNetwork.h"

//static void AkComponentCallback( AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo )
//{
//	UAkComponent * pObj = ( (UAkComponent *) in_pCallbackInfo->pCookie );
//	if( pObj )
//	{
//		pObj->DestroyComponent();
//	}
//}

/*------------------------------------------------------------------------------------
	UAkComponent
------------------------------------------------------------------------------------*/

static void AkComponentCallback( AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo )
{
	UAkComponent * pObj = ( (UAkComponent *) in_pCallbackInfo->pCookie );
	if( pObj )
	{
		pObj->NumActiveEvents.Decrement();
		if( pObj->NumActiveEvents.GetValue() == 0 )
		{
			pObj->bFlaggedForDestroy = true;
		}
	}
}

UAkComponent::UAkComponent(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	// Property initialization
 	StopWhenOwnerDestroyed = true;
	bUseReverbVolumes = true;
	OcclusionRefreshInterval = 0.2f;
	LastOcclusionRefresh = -1;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	bTickInEditor = true;
	
	bAutoActivate = true;
	bNeverNeedsRenderUpdate = true;
	bWantsOnUpdateTransform = true;

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif

	AttenuationScalingFactor = 1.0f;
	bAutoDestroy = false;
	bFlaggedForDestroy = false;
	NumActiveEvents.Reset();
}

void UAkComponent::PostAssociatedAkEvent()
{
	if( AkAudioEvent )
	{
		PostAkEventByName(AkAudioEvent->GetName());
	}
	else
	{
		PostAkEventByName(EventName);
	}
}

void UAkComponent::PostAkEvent( class UAkAudioEvent * AkEvent, const FString& in_EventName )
{
	if ( AkEvent )
	{
		PostAkEventByName(AkEvent->GetName());
	}
	else
	{
		PostAkEventByName(in_EventName);
	}
}

void UAkComponent::PostAkEventByName( const FString& in_EventName )
{
	UWorld* CurrentWorld = GetWorld();

	if (in_EventName.IsEmpty())
	{
		UE_LOG(LogAkAudio, Warning, TEXT("AkComponent: Attempted to post an empty AkEvent name."));
		return;
	}

	if ( CurrentWorld->AllowAudioPlayback() && FAkAudioDevice::Get() )
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szEventName = TCHAR_TO_ANSI(*in_EventName);
#else
		const WIDECHAR * szEventName = *in_EventName;
#endif
		if( OcclusionRefreshInterval > 0.0f )
		{
			CalculateOcclusionValues(false);
		}

		if( bAutoDestroy )
		{
			NumActiveEvents.Increment();
			AK::SoundEngine::PostEvent( szEventName, (AkGameObjectID) this, AK_EndOfEvent, &AkComponentCallback, this);
		}
		else
		{
			AK::SoundEngine::PostEvent( szEventName, (AkGameObjectID) this );
		}
	}
}

void UAkComponent::Stop()
{
	if (  FAkAudioDevice::Get() )
	{
		AK::SoundEngine::StopAll( (AkGameObjectID) this );
	}
}

void UAkComponent::SetRTPCValue( FString RTPC, float Value, int32 InterpolationTimeMs = 0)
{
	if ( FAkAudioDevice::Get() )
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szRTPC = TCHAR_TO_ANSI(*RTPC);
#else
		const WIDECHAR * szRTPC = *RTPC;
#endif
		AK::SoundEngine::SetRTPCValue( szRTPC, Value, (AkGameObjectID) this, InterpolationTimeMs );
	}
}

void UAkComponent::PostTrigger( FString Trigger )
{
	if ( FAkAudioDevice::Get() )
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szTrigger = TCHAR_TO_ANSI(*Trigger);
#else
		const WIDECHAR * szTrigger = *Trigger;
#endif
		AK::SoundEngine::PostTrigger( szTrigger, (AkGameObjectID) this );
	}
}

void UAkComponent::SetSwitch( FString SwitchGroup, FString SwitchState )
{
	if ( FAkAudioDevice::Get() )
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szSwitchGroup = TCHAR_TO_ANSI(*SwitchGroup);
		ANSICHAR* szSwitchState = TCHAR_TO_ANSI(*SwitchState);
#else
		const WIDECHAR * szSwitchGroup = *SwitchGroup;
		const WIDECHAR * szSwitchState = *SwitchState;
#endif
		AK::SoundEngine::SetSwitch( szSwitchGroup, szSwitchState, (AkGameObjectID) this );
	}
}

void UAkComponent::SetStopWhenOwnerDestroyed( bool bStopWhenOwnerDestroyed )
{
	StopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;
}

void UAkComponent::SetActiveListeners( int32 ListenerMask )
{
	if ( FAkAudioDevice::Get() )
	{
		AK::SoundEngine::SetActiveListeners( (AkGameObjectID) this, ListenerMask );
	}
}

void UAkComponent::UseReverbVolumes(bool inUseReverbVolumes)
{
	bUseReverbVolumes = inUseReverbVolumes;
}

float UAkComponent::GetAttenuationRadius() const
{ 
	if( AkAudioEvent )
	{
		return AttenuationScalingFactor * AkAudioEvent->MaxAttenuationRadius;
	}
	
	return 0.f;
}


void UAkComponent::OnRegister()
{
	RegisterGameObject(); // Done before parent so that OnUpdateTransform follows registration and updates position correctly.

	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	UpdateSpriteTexture();
#endif
}

#if WITH_EDITORONLY_DATA
void UAkComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Wwise/S_AkComponent.S_AkComponent")));
	}
}
#endif


void UAkComponent::OnUnregister()
{
	// Route OnUnregister event.
	Super::OnUnregister();

	// Don't stop audio and clean up component if owner has been destroyed (default behaviour). This function gets
	// called from AActor::ClearComponents when an actor gets destroyed which is not usually what we want for one-
	// shot sounds.
	AActor* Owner = GetOwner();
	if( !Owner || StopWhenOwnerDestroyed )
	{
		Stop();
	}
}

void UAkComponent::FinishDestroy( void )
{
	UnregisterGameObject();

	Super::FinishDestroy();
}

void UAkComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	UnregisterGameObject();
}

void UAkComponent::ShutdownAfterError( void )
{
	UnregisterGameObject();

	Super::ShutdownAfterError();
}

void UAkComponent::ApplyAkReverbVolumeList(float DeltaTime)
{
	if( CurrentAkReverbVolumes.Num() > 0 )
	{
		// Fade control
		for( int32 Idx = 0; Idx < CurrentAkReverbVolumes.Num(); Idx++ )
		{
			if( CurrentAkReverbVolumes[Idx].CurrentControlValue != CurrentAkReverbVolumes[Idx].TargetControlValue || CurrentAkReverbVolumes[Idx].bIsFadingOut )
			{
				float Increment = ComputeFadeIncrement(DeltaTime, CurrentAkReverbVolumes[Idx].FadeRate, CurrentAkReverbVolumes[Idx].TargetControlValue);
				if( CurrentAkReverbVolumes[Idx].bIsFadingOut )
				{
					CurrentAkReverbVolumes[Idx].CurrentControlValue -= Increment;
					if( CurrentAkReverbVolumes[Idx].CurrentControlValue <= 0.f )
					{
						CurrentAkReverbVolumes.RemoveAt(Idx);
					}
				}
				else
				{
					CurrentAkReverbVolumes[Idx].CurrentControlValue += Increment;
					if( CurrentAkReverbVolumes[Idx].CurrentControlValue > CurrentAkReverbVolumes[Idx].TargetControlValue )
					{
						CurrentAkReverbVolumes[Idx].CurrentControlValue = CurrentAkReverbVolumes[Idx].TargetControlValue;
					}
				}
			}
		}

		// Sort the list of active AkReverbVolumes by desecnding priority, if necessary
		if(CurrentAkReverbVolumes.Num() > 1 )
		{
			struct FCompareAkReverbVolumeByPriorityAndFade
			{
				FORCEINLINE bool operator()( const AkReverbVolumeFadeControl& A, const AkReverbVolumeFadeControl& B ) const 
				{ 
					// Ensure the fading out buffers are sent to the end of the array
					if( A.bIsFadingOut == B.bIsFadingOut )
					{
						return A.Priority > B.Priority; 
					}
					else
					{
						return A.bIsFadingOut < B.bIsFadingOut;
					}
				}
			};
			CurrentAkReverbVolumes.Sort(FCompareAkReverbVolumeByPriorityAndFade());
		}
	}

	TArray<AkAuxSendValue> AkReverbVolumes;
	AkAuxSendValue	TmpSendValue;
	AkReverbVolumes.Empty();

	// Build a list to set as AuxBusses
	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		for( int32 Idx = 0; Idx < CurrentAkReverbVolumes.Num() && Idx < AkAudioDevice->GetMaxAuxBus(); Idx++ )
		{
			TmpSendValue.auxBusID = CurrentAkReverbVolumes[Idx].AuxBusId;
			TmpSendValue.fControlValue = CurrentAkReverbVolumes[Idx].CurrentControlValue;
			AkReverbVolumes.Add(TmpSendValue);
		}

		AkAudioDevice->SetAuxSends((AkGameObjectID) this, AkReverbVolumes);
	}

}

void UAkComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if ( AK::SoundEngine::IsInitialized() )
	{
		Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

		FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			// Update AkReverbVolume fade in/out
			if( bUseReverbVolumes && AkAudioDevice->GetMaxAuxBus() > 0 )
			{
				ApplyAkReverbVolumeList(DeltaTime);
			}
		}

		// Check Occlusion/Obstruction, if enabled
		if( OcclusionRefreshInterval > 0.f )
		{
			SetOcclusion(DeltaTime);
		}

		if( bAutoDestroy && bFlaggedForDestroy )
		{
			if( NumActiveEvents.GetValue() == 0 )
			{
				DestroyComponent();
			}
			else
			{
				bFlaggedForDestroy = false;
			}
		}
	}
}


void UAkComponent::Activate(bool bReset)
{
	Super::Activate( bReset );

	UpdateGameObjectPosition();

	// If spawned inside AkReverbVolume(s), we do not want the fade in effect to kick in.
	UpdateAkReverbVolumeList(GetComponentLocation());
	for( int32 Idx = 0; Idx < CurrentAkReverbVolumes.Num(); Idx++ )
	{
		CurrentAkReverbVolumes[Idx].CurrentControlValue = CurrentAkReverbVolumes[Idx].TargetControlValue;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		AudioDevice->SetAttenuationScalingFactor(this, AttenuationScalingFactor);
	}
}

void UAkComponent::OnUpdateTransform(bool bSkipPhysicsMove, ETeleportType Teleport)
{
	Super::OnUpdateTransform(bSkipPhysicsMove, Teleport);

	UpdateGameObjectPosition();
}

void UAkComponent::RegisterGameObject()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if ( AkAudioDevice )
	{
		AkAudioDevice->RegisterComponent( this );
	}
}

void UAkComponent::UnregisterGameObject()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if ( AkAudioDevice )
	{
		AkAudioDevice->UnregisterComponent( this );

		if(bAutoDestroy)
		{
			AkAudioDevice->CancelEventCallbackCookie(this);
		}
	}
}

int32 UAkComponent::FindNewAkReverbVolumeInCurrentlist(uint32 AuxBusId)
{
	struct FAkNewReverbVolumeMatcher
	{
	private:
		/** Target for comparison in the TArray */
		const uint32& AuxBusId;

	public:
		FAkNewReverbVolumeMatcher(const uint32& AkAuxBusId) :
			AuxBusId(AkAuxBusId)
		{
		}

		/**
		 * Match a given Aux Bus Id against the one stored in this struct
		 *
		 * @return true if they are an exact match, false otherwise
		 */
		bool operator()(const AkReverbVolumeFadeControl& Candidate) const
 		{
			return AuxBusId == Candidate.AuxBusId;
 		}
	};
	FAkNewReverbVolumeMatcher matcher(AuxBusId);
	return CurrentAkReverbVolumes.IndexOfByPredicate(matcher);
}

static int32 FindCurrentAkReverbVolumeInNewlist(TArray<AAkReverbVolume*> FoundVolumes, AkAuxBusID AuxBusId)
{
	struct FAkCurrentReverbVolumeMatcher
	{
	private:
		/** Target for comparison in the TArray */
		const uint32& AuxBusId;

	public:
		FAkCurrentReverbVolumeMatcher(const uint32& AkAuxBusId) :
			AuxBusId(AkAuxBusId)
		{
		}

		/**
		 * Match a given Aux Bus Id against the one stored in this struct
		 *
		 * @return true if they are an exact match, false otherwise
		 */
		bool operator()(const AAkReverbVolume* const Candidate) const
 		{
			return AuxBusId == Candidate->GetAuxBusId();
 		}
	};

	return FoundVolumes.IndexOfByPredicate( FAkCurrentReverbVolumeMatcher(AuxBusId) );
}

void FindAkReverbVolumesAtLocation(FVector Loc, TArray<AAkReverbVolume*>& FoundVolumes, const UWorld* World);
void UAkComponent::UpdateAkReverbVolumeList( FVector Loc )
{
	TArray<AAkReverbVolume*> FoundVolumes;
	FindAkReverbVolumesAtLocation(Loc, FoundVolumes, GetWorld());


	// Add the new volumes to the current list
	for( int32 Idx = 0; Idx < FoundVolumes.Num(); Idx++ )
	{
		AkAuxBusID	CurrentAuxBusId = FoundVolumes[Idx]->GetAuxBusId();
		int32 FoundIdx = FindNewAkReverbVolumeInCurrentlist( CurrentAuxBusId );
		if( FoundIdx == INDEX_NONE )
		{
			// The volume was not found, add it to the list
			CurrentAkReverbVolumes.Add(AkReverbVolumeFadeControl(CurrentAuxBusId, 0.f, FoundVolumes[Idx]->SendLevel, FoundVolumes[Idx]->FadeRate, false, FoundVolumes[Idx]->Priority));
		}
		else
		{
			// The volume was found. We still have to check if it is currently fading out, in case we are
			// getting back in a volume we just exited.
			if( CurrentAkReverbVolumes[FoundIdx].bIsFadingOut == true )
			{
				CurrentAkReverbVolumes[FoundIdx].bIsFadingOut = false;
			}
		}
	}

	// Fade out the current volumes not found in the new list
	for( int32 Idx = 0; Idx < CurrentAkReverbVolumes.Num(); Idx++ )
	{
		if( FindCurrentAkReverbVolumeInNewlist(FoundVolumes, CurrentAkReverbVolumes[Idx].AuxBusId) == INDEX_NONE )
		{
			// Our current volume was not found in the array of volumes at the current position. Begin fading it out
			CurrentAkReverbVolumes[Idx].bIsFadingOut = true;
		}
	}
}

void UAkComponent::UpdateGameObjectPosition()
{
	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if ( bIsActive && AkAudioDevice )
	{
		AkSoundPosition soundpos;
		FAkAudioDevice::FVectorToAKVector( ComponentToWorld.GetTranslation(), soundpos.Position );
		FAkAudioDevice::FVectorToAKVector( ComponentToWorld.GetUnitAxis( EAxis::X ), soundpos.Orientation );
		AK::SoundEngine::SetPosition( (AkGameObjectID) this, soundpos );

		// Find and apply all AkReverbVolumes at this location
		if( bUseReverbVolumes && AkAudioDevice->GetMaxAuxBus() > 0 )
		{
			UpdateAkReverbVolumeList( GetComponentLocation() );
		}
	}
}

const float UAkComponent::OCCLUSION_FADE_RATE = 2.0f; // from 0.0 to 1.0 in 0.5 seconds
void UAkComponent::SetOcclusion(const float DeltaTime)
{
	// Fade the active occlusions
	for(int32 ListenerIdx = 0; ListenerIdx < ListenerOcclusionInfo.Num(); ListenerIdx++)
	{
		if( ListenerOcclusionInfo[ListenerIdx].CurrentValue != ListenerOcclusionInfo[ListenerIdx].TargetValue )
		{
			bool bIsFadingOut = ListenerOcclusionInfo[ListenerIdx].CurrentValue > ListenerOcclusionInfo[ListenerIdx].TargetValue;
			float sign = bIsFadingOut ? -1.0 : 1.0;
			float maxVal = bIsFadingOut ? ListenerOcclusionInfo[ListenerIdx].CurrentValue : ListenerOcclusionInfo[ListenerIdx].TargetValue;
			
			ListenerOcclusionInfo[ListenerIdx].CurrentValue = FMath::Clamp( 
				ListenerOcclusionInfo[ListenerIdx].CurrentValue + (sign * OCCLUSION_FADE_RATE * DeltaTime), 
				0.0f, 
				maxVal
				);

			FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
			if( AkAudioDevice )
			{
				AkAudioDevice->SetOcclusionObstruction(this, ListenerIdx, 0.0f, ListenerOcclusionInfo[ListenerIdx].CurrentValue);
			}
		}
	}

	// Compute occlusion only when needed.
	// Have to have "LastOcclutionRefresh == -1" because GetWorld() might return nullptr in UAkComponent's constructor,
	// preventing us from initializing it to something smart.
	if( (World->GetTimeSeconds() - LastOcclusionRefresh) < OcclusionRefreshInterval && LastOcclusionRefresh != -1 )
	{
		return;
	}

	CalculateOcclusionValues(true);
}

void UAkComponent::CalculateOcclusionValues(bool CalledFromTick)
{
	LastOcclusionRefresh = GetWorld()->GetTimeSeconds();
	static FName NAME_SoundOcclusion = FName(TEXT("SoundOcclusion"));

	int32 NumListeners = 0;
	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		NumListeners = AkAudioDevice->GetNumListeners();
	}

	for(int32 ListenerIdx = 0; ListenerIdx < NumListeners; ListenerIdx++)
	{
		FHitResult OutHit;

		if( ListenerIdx >= ListenerOcclusionInfo.Num() )
		{
			ListenerOcclusionInfo.Add(FAkListenerOcclusion());
		}

		FVector ListenerPosition;
		AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			ListenerPosition = AkAudioDevice->GetListenerPosition(ListenerIdx);
		}
		FVector SourcePosition = GetComponentLocation();
		APlayerController* PlayerController = World->GetFirstPlayerController();
		APawn* ActorToIgnore = NULL;
		if( PlayerController != NULL )
		{
			ActorToIgnore = PlayerController->GetPawn();
		}

		bool bNowOccluded = GetWorld()->LineTraceSingleByChannel(OutHit, SourcePosition, ListenerPosition, ECC_Visibility, FCollisionQueryParams(NAME_SoundOcclusion, true, ActorToIgnore));
		if( bNowOccluded )
		{
			FBox BoundingBox;

			if( OutHit.Actor.IsValid() )
			{
				BoundingBox = OutHit.Actor->GetComponentsBoundingBox();
			}
			else if( OutHit.Component.IsValid() )
			{
				BoundingBox = OutHit.Component->Bounds.GetBox();
			}

			// Translate the impact point to the bounding box of the obstacle
			TArray<FVector> Points;
			Points.Add(FVector(OutHit.ImpactPoint.X, BoundingBox.Min.Y, BoundingBox.Min.Z));
			Points.Add(FVector(OutHit.ImpactPoint.X, BoundingBox.Min.Y, BoundingBox.Max.Z));
			Points.Add(FVector(OutHit.ImpactPoint.X, BoundingBox.Max.Y, BoundingBox.Min.Z));
			Points.Add(FVector(OutHit.ImpactPoint.X, BoundingBox.Max.Y, BoundingBox.Max.Z));

			Points.Add(FVector(BoundingBox.Min.X, OutHit.ImpactPoint.Y, BoundingBox.Min.Z));
			Points.Add(FVector(BoundingBox.Min.X, OutHit.ImpactPoint.Y, BoundingBox.Max.Z));
			Points.Add(FVector(BoundingBox.Max.X, OutHit.ImpactPoint.Y, BoundingBox.Min.Z));
			Points.Add(FVector(BoundingBox.Max.X, OutHit.ImpactPoint.Y, BoundingBox.Max.Z));

			Points.Add(FVector(BoundingBox.Min.X, BoundingBox.Min.Y, OutHit.ImpactPoint.Z));
			Points.Add(FVector(BoundingBox.Min.X, BoundingBox.Max.Y, OutHit.ImpactPoint.Z));
			Points.Add(FVector(BoundingBox.Max.X, BoundingBox.Min.Y, OutHit.ImpactPoint.Z));
			Points.Add(FVector(BoundingBox.Max.X, BoundingBox.Max.Y, OutHit.ImpactPoint.Z));
				
			// Compute the number of "second order paths" that are also obstructed. This will allow us to approximate
			// "how obstructed" the source is.
			int32 NumObstructedPaths = 0;
			for(int32 PointIdx = 0; PointIdx < Points.Num(); PointIdx++)
			{
				FHitResult TempHit;
				bool bListenerToObstacle = GetWorld()->LineTraceSingleByChannel(TempHit, ListenerPosition, Points[PointIdx], ECC_Visibility, FCollisionQueryParams(NAME_SoundOcclusion, true, ActorToIgnore));
				bool bSourceToObstacle = GetWorld()->LineTraceSingleByChannel(TempHit, SourcePosition, Points[PointIdx], ECC_Visibility, FCollisionQueryParams(NAME_SoundOcclusion, true, ActorToIgnore));
				if(bListenerToObstacle || bSourceToObstacle)
				{
					NumObstructedPaths++;
				}
			}

			// Modulate occlusion by blocked secondary paths. 
			ListenerOcclusionInfo[ListenerIdx].TargetValue = (float)NumObstructedPaths / (float)Points.Num();

#define AK_DEBUG_OCCLUSION 0
#if AK_DEBUG_OCCLUSION
			// Draw bounding box and "second order paths"
			//UE_LOG(LogAkAudio, Log, TEXT("Target Occlusion level: %f"), ListenerOcclusionInfo[ListenerIdx].TargetValue);
			::FlushPersistentDebugLines(GetWorld());
			::FlushDebugStrings(GetWorld());
			::DrawDebugBox(GetWorld(), BoundingBox.GetCenter(), BoundingBox.GetExtent(), FColor::White, false, 4);
			::DrawDebugPoint(GetWorld(), ListenerPosition, 10.0f, FColor(0, 255, 0), false, 4);
			::DrawDebugPoint(GetWorld(), SourcePosition, 10.0f, FColor(0, 255, 0), false, 4);
			::DrawDebugPoint(GetWorld(), OutHit.ImpactPoint, 10.0f, FColor(0, 255, 0), false, 4);

			for(int32 i = 0; i < Points.Num(); i++)
			{
				::DrawDebugPoint(GetWorld(), Points[i], 10.0f, FColor(255, 255, 0), false, 4);
				::DrawDebugString(GetWorld(), Points[i], FString::Printf(TEXT("%d"), i), nullptr, FColor::White, 4);
				::DrawDebugLine(GetWorld(), Points[i], ListenerPosition, FColor::Cyan, false, 4);
				::DrawDebugLine(GetWorld(), Points[i], SourcePosition, FColor::Cyan, false, 4);
			}
			FColor LineColor = FColor::MakeRedToGreenColorFromScalar(1.0f - ListenerOcclusionInfo[ListenerIdx].TargetValue);
			::DrawDebugLine(GetWorld(), ListenerPosition, SourcePosition, LineColor, false, 4);
#endif // AK_DEBUG_OCCLUSION
		}
		else
		{
			ListenerOcclusionInfo[ListenerIdx].TargetValue = 0.0f;
		}

		if( !CalledFromTick )
		{
			ListenerOcclusionInfo[ListenerIdx].CurrentValue = ListenerOcclusionInfo[ListenerIdx].TargetValue;
			AkAudioDevice = FAkAudioDevice::Get();
			if( AkAudioDevice )
			{
				AkAudioDevice->SetOcclusionObstruction(this, ListenerIdx, 0.0f, ListenerOcclusionInfo[ListenerIdx].CurrentValue);
			}
		}
	}
}
