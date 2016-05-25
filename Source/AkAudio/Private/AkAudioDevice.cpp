// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAudioDevice.cpp: Audiokinetic Audio interface object.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "AkAudioDevice.h"
#include "AkAudioModule.h"
#include "Engine.h"
#include "AkAudioClasses.h"
#include "EditorSupportDelegates.h"
#include "ISettingsModule.h"
	
// OCULUS_START
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"
// OCULUS_END

#include <AK/Plugin/AllPluginsRegistrationHelpers.h>

#if PLATFORM_XBOXONE
	#include <apu.h>
#endif

DEFINE_LOG_CATEGORY(LogAkAudio);	

/*------------------------------------------------------------------------------------
	Statics and Globals
------------------------------------------------------------------------------------*/

#ifdef AK_USE_UNREAL_IO
	CAkUnrealIOHookDeferred g_lowLevelIO;
#else
	// Then, we're using the default CAkDefaultIOHookBlocking implementation that's part
	// of the SDK's sample code
	CAkDefaultIOHookDeferred g_lowLevelIO;
#endif

bool FAkAudioDevice::m_bSoundEngineInitialized = false;

/*------------------------------------------------------------------------------------
	Defines
------------------------------------------------------------------------------------*/

#define INITBANKNAME (TEXT("Init"))
#define GAME_OBJECT_MAX_STRING_SIZE 512
#define AK_READ_SIZE DVD_MIN_READ_SIZE

/*------------------------------------------------------------------------------------
	Memory hooks
------------------------------------------------------------------------------------*/

namespace AK
{
	void * AllocHook( size_t in_size )
	{
		return FMemory::Malloc( in_size );
	}
	void FreeHook( void * in_ptr )
	{
		FMemory::Free( in_ptr );
	}

#ifdef _WIN32 // only on PC and XBox360
	void * VirtualAllocHook(
		void * in_pMemAddress,
		size_t in_size,
		unsigned long in_dwAllocationType,
		unsigned long in_dwProtect
		)
	{
		return VirtualAlloc( in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect );
	}
	void VirtualFreeHook( 
		void * in_pMemAddress,
		size_t in_size,
		unsigned long in_dwFreeType
		)
	{
		VirtualFree( in_pMemAddress, in_size, in_dwFreeType );
	}
#endif // only on PC and XBox360

#if PLATFORM_XBOXONE
	void * APUAllocHook( 
		size_t in_size,				///< Number of bytes to allocate.
		unsigned int in_alignment	///< Alignment in bytes (must be power of two, greater than or equal to four).
		)
	{
		void * pReturn = nullptr;
		ApuAlloc( &pReturn, NULL, (UINT32) in_size, in_alignment );
		return pReturn;
	}

	void APUFreeHook( 
		void * in_pMemAddress	///< Virtual address as returned by APUAllocHook.
		)
	{
		ApuFree( in_pMemAddress );
	}
#endif
}

/*------------------------------------------------------------------------------------
	Helpers
------------------------------------------------------------------------------------*/

static inline void AkVectorToFVector( const AkVector & in_vect, FVector & out_vect )
{
	out_vect.X = -in_vect.X;
	out_vect.Y = in_vect.Z;
	out_vect.Z = in_vect.Y;
}

/*------------------------------------------------------------------------------------
	Implementation
------------------------------------------------------------------------------------*/

static inline void RegisterGameObj_WithName( AkGameObjectID in_gameObj, const TCHAR * in_tszName )
{
	if ( in_tszName )
	{
		AK::SoundEngine::RegisterGameObj( in_gameObj, TCHAR_TO_ANSI(in_tszName) );
	}
	else
	{
		AK::SoundEngine::RegisterGameObj( in_gameObj, "" );
	}
}

/**
 * Initializes the audio device and creates sources.
 *
 * @return true if initialization was successful, false otherwise
 */
bool FAkAudioDevice::Init( void )
{
#if DEDICATED_SERVER
	return false;
#endif
	AkBankManager = NULL;
	if(!EnsureInitialized()) // ensure audiolib is initialized
	{
		UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device initialization failed."));
		return false;
	}

	// Initialize SoundFrame
#ifdef AK_SOUNDFRAME
	m_pSoundFrame = NULL;
	if( AK::SoundFrame::Create( this, &m_pSoundFrame ) )
	{
		m_pSoundFrame->Connect();
	}
#endif

	// OCULUS_START - vhamm - suspend audio when not in focus
	m_isSuspended = false;
	// OCULUS_END

	FWorldDelegates::LevelRemovedFromWorld.AddRaw(this, &FAkAudioDevice::OnLevelRemoved);


	UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device initialized."));

	return 1;
}

void FAkAudioDevice::OnLevelRemoved(ULevel* InLevel, UWorld* InWorld)
{
	Flush(NULL);
}

/**
 * Update the audio device and calculates the cached inverse transform later
 * on used for spatialization.
 */
bool FAkAudioDevice::Update( float DeltaTime )
{
	if ( m_bSoundEngineInitialized )
	{
		// OCULUS_START - vhamm - suspend audio when not in focus
		if (FApp::UseVRFocus())
		{
			if (FApp::HasVRFocus())
			{
				if (m_isSuspended)
				{
					AK::SoundEngine::WakeupFromSuspend();
					m_isSuspended = false;
				}
			}
			else
			{
				if (!m_isSuspended)
				{
					AK::SoundEngine::Suspend(true);
					m_isSuspended = true;
				}
			}
		}
		// OCULUS_END

		AK::SoundEngine::RenderAudio();
		UpdateListeners();
	}

	return true;
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void FAkAudioDevice::Teardown()
{
	if (m_bSoundEngineInitialized == true)
	{
		// Unload all loaded banks before teardown
		if( AkBankManager )
		{
			const TSet<UAkAudioBank*>* LoadedBanks = AkBankManager->GetLoadedBankList();
			TSet<UAkAudioBank*> LoadedBanksCopy(*LoadedBanks);
			for(TSet<UAkAudioBank*>::TConstIterator LoadIter(LoadedBanksCopy); LoadIter; ++LoadIter)
			{
				if( (*LoadIter) != NULL && (*LoadIter)->IsValidLowLevel() )
				{
					(*LoadIter)->Unload();
				}
			}
			delete AkBankManager;
		}

#ifndef AK_OPTIMIZED
#if !PLATFORM_LINUX
		//
		// Terminate Communication Services
		//
		AK::Comm::Term();
#endif
#endif // AK_OPTIMIZED

		AK::SoundEngine::UnregisterGameObj( DUMMY_GAMEOBJ );

		//
		// Terminate the music engine
		//
		AK::MusicEngine::Term();

		//
		// Unregister game objects. Since we're about to terminate the sound engine
		// anyway, we don't really have to unregister those game objects here. But
		// in general it is good practice to unregister game objects as soon as they
		// become obsolete, to free up resources.
		//
		if ( AK::SoundEngine::IsInitialized() )
		{
			//
			// Terminate the sound engine
			//
			AK::SoundEngine::Term();
		}
		
		g_lowLevelIO.Term();

		// Terminate the streaming manager
		if ( AK::IAkStreamMgr::Get() )
		{   
			AK::IAkStreamMgr::Get()->Destroy();
		}

		// Terminate the Memory Manager
		AK::MemoryMgr::Term();

		m_bSoundEngineInitialized = false;
	}

	// Terminate SoundFrame
#ifdef AK_SOUNDFRAME
	if ( m_pSoundFrame )
	{
		m_pSoundFrame->Release();
		m_pSoundFrame = NULL;
	}
#endif

	FWorldDelegates::LevelRemovedFromWorld.RemoveAll( this );

	UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device terminated."));
}

/**
 * Stops all game sounds (and possibly UI) sounds
 *
 * @param bShouldStopUISounds If true, this function will stop UI sounds as well
 */
void FAkAudioDevice::StopAllSounds( bool bShouldStopUISounds )
{
	AK::SoundEngine::StopAll( DUMMY_GAMEOBJ );
	AK::SoundEngine::StopAll();
}

/**
 * Sets all listeners
 */
void FAkAudioDevice::UpdateListeners()
{
	if( GEngine )
	{
		TArray<APlayerController*> PlayerControllers;
		GEngine->GetAllLocalPlayerControllers(PlayerControllers);

		for(int32 i = 0; i < PlayerControllers.Num(); i++)
		{
			if (PlayerControllers[i] != nullptr)
			{
				FVector Location;
				FVector Front;
				FVector Right;
				PlayerControllers[i]->GetAudioListenerPosition(Location, Front, Right);

				FVector Up = FVector::CrossProduct(Front, Right);

				SetListener(i, Location, Up, Front);
			}
		}
	}
}


/**
 * Sets the listener's location and orientation for the Player Character
 *
 * @param PlayerCharacterIndex		Current player
 * @param Location			Listener location
 * @param Up				Listeners up vector
 * @param Front				Listeners front vector
 */
void FAkAudioDevice::SetListener( int32 PlayerCharacterIndex, const FVector& Location, const FVector& Up, const FVector& Front )
{
	AkListenerPosition position;

	// Make sure we have the space in our listener array for all listeners
	while( PlayerCharacterIndex >= m_listenerPositions.Num() )
	{
		// Add the new listener
		m_listenerPositions.Add( FVector::ZeroVector );
	}

	m_listenerPositions[PlayerCharacterIndex] = Location;
	FVectorToAKVector( Location, position.Position );
	FVectorToAKVector( Front, position.OrientationFront );
	FVectorToAKVector( Up, position.OrientationTop );

	if ( position.OrientationFront.X == 0.0 && position.OrientationFront.Y == 0.0 && position.OrientationFront.Z == 0.0 )
	{
		UE_LOG(LogAkAudio, Fatal, TEXT("Orientation Front vector invalid!") );
	}

	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::SetListenerPosition( position, PlayerCharacterIndex );
		
		// Set the dummy object to always be at the listener position, otherwise you cannot preview 3D sounds in the editor.
		if ( GIsEditor && PlayerCharacterIndex == 0)
		{
			AkSoundPosition sound_position;
			FVectorToAKVector( Location, sound_position.Position );
			FVectorToAKVector( Front, sound_position.Orientation );
			AK::SoundEngine::SetPosition( DUMMY_GAMEOBJ, sound_position );
		}
	}
}

FVector FAkAudioDevice::GetListenerPosition( int32 ViewportIndex )
{
	check( ViewportIndex < m_listenerPositions.Num() );
	return m_listenerPositions[ViewportIndex];
}

/**
 * Stop all audio associated with a scene
 *
 * @param SceneToFlush		Interface of the scene to flush
 */
void FAkAudioDevice::Flush(UWorld* WorldToFlush)
{
	AK::SoundEngine::StopAll( DUMMY_GAMEOBJ );
	AK::SoundEngine::StopAll();
}

/**
 * Clears all loaded soundbanks
 *
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::ClearBanks()
{
	if ( m_bSoundEngineInitialized )
	{
		AKRESULT eResult = AK::SoundEngine::ClearBanks();
		if( eResult == AK_Success && AkBankManager != NULL )
			{
				FScopeLock Lock(&AkBankManager->m_BankManagerCriticalSection);
				AkBankManager->ClearLoadedBanks();
		}

		return eResult;
	}
	else
	{
		return AK_Success;
	}
}

/**
 * Load a soundbank
 *
 * @param in_Bank		The bank to load
 * @param in_memPoolId		Memory pool ID (media is stored in the sound engine's default pool if AK_DEFAULT_POOL_ID is passed)
 * @param out_bankID		Returned bank ID
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::LoadBank(
	class UAkAudioBank *     in_Bank,
	AkMemPoolId         in_memPoolId,
	AkBankID &          out_bankID
	)
{
	AKRESULT eResult = LoadBank(in_Bank->GetName(), in_memPoolId, out_bankID);
	if( eResult == AK_Success && AkBankManager != NULL)
	{
		FScopeLock Lock(&AkBankManager->m_BankManagerCriticalSection);
		AkBankManager->AddLoadedBank(in_Bank);
	}
	return eResult;
}

/**
 * Load a soundbank by name
 *
 * @param in_BankName		The name of the bank to load
 * @param in_memPoolId		Memory pool ID (media is stored in the sound engine's default pool if AK_DEFAULT_POOL_ID is passed)
 * @param out_bankID		Returned bank ID
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::LoadBank(
	const FString&      in_BankName,
	AkMemPoolId         in_memPoolId,
	AkBankID &          out_bankID
	)
{
	AKRESULT eResult = AK_Fail;
	if( EnsureInitialized() ) // ensure audiolib is initialized
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szString = TCHAR_TO_ANSI(*in_BankName);
#else
		const WIDECHAR * szString = *in_BankName;
#endif		
		eResult = AK::SoundEngine::LoadBank( szString, in_memPoolId, out_bankID );
	}
	return eResult;
}

static void AkAudioDeviceBankLoadCallback(	
	AkUInt32		in_bankID,
	const void *	in_pInMemoryBankPtr,
	AKRESULT		in_eLoadResult,
	AkMemPoolId		in_memPoolId,
	void *			in_pCookie
)
{
	FAkAudioDevice * akAudioDevice = FAkAudioDevice::Get();
	AkBankCallbackFunc cbFunc = NULL;
	if( akAudioDevice )
	{
		FAkBankManager * BankManager = akAudioDevice->GetAkBankManager();
		if( BankManager != NULL )
		{
			FScopeLock Lock(&BankManager->m_BankManagerCriticalSection);

			FAkBankManager::AkBankCallbackInfo * cbInfo = BankManager->GetBankLoadCallbackInfo(in_pCookie);
			if( cbInfo != NULL )
			{
				cbFunc = cbInfo->CallbackFunc;
			}

			if( in_eLoadResult == AK_Success )
			{
				// Load worked; put the bank in the list.
				BankManager->AddLoadedBank(cbInfo->pBank);
			}

			BankManager->RemoveBankLoadCallbackInfo(in_pCookie);
		}
	}

	if( cbFunc != NULL )
	{
		// Call the user's callback function
		cbFunc(in_bankID, in_pInMemoryBankPtr, in_eLoadResult, in_memPoolId, in_pCookie);
	}
}

/**
 * Load a soundbank asynchronously
 *
 * @param in_Bank		The bank to load
 * @param in_pfnBankCallback Callback function
 * @param in_pCookie		Callback cookie (reserved to user, passed to the callback function)
 * @param in_memPoolId		Memory pool ID (media is stored in the sound engine's default pool if AK_DEFAULT_POOL_ID is passed)
 * @param out_bankID		Returned bank ID
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::LoadBank(
	class UAkAudioBank *     in_Bank,
	AkBankCallbackFunc  in_pfnBankCallback,
	void *              in_pCookie,
    AkMemPoolId         in_memPoolId,
	AkBankID &          out_bankID
    )
{
	if( EnsureInitialized() ) // ensure audiolib is initialized
	{
		FString name = in_Bank->GetName();
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szString = TCHAR_TO_ANSI(*name);
#else
		const WIDECHAR * szString = *name;
#endif

		if( AkBankManager != NULL )
		{
			FAkBankManager::AkBankCallbackInfo cbInfo(in_pfnBankCallback, in_Bank);

			// We need a unique cookie for the map. If none, use the bank as cookie
			if( in_pCookie == NULL )
			{
				in_pCookie = in_Bank;
			}

			// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
			AkBankManager->m_BankManagerCriticalSection.Lock();
			AkBankManager->AddBankLoadCallbackInfo(in_pCookie, cbInfo);
			AkBankManager->m_BankManagerCriticalSection.Unlock();

			return AK::SoundEngine::LoadBank( szString, AkAudioDeviceBankLoadCallback, in_pCookie, in_memPoolId, out_bankID );
		}
		else
		{
			return AK::SoundEngine::LoadBank( szString, in_pfnBankCallback, in_pCookie, in_memPoolId, out_bankID );
		}
	}
	return AK_Fail;
}

/**
 * Unload a soundbank
 *
 * @param in_Bank		The bank to unload
 * @param out_pMemPoolId	Returned memory pool ID used with LoadBank() (can pass NULL)
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::UnloadBank(
	class UAkAudioBank *     in_Bank,
    AkMemPoolId *       out_pMemPoolId		    ///< Returned memory pool ID used with LoadBank() (can pass NULL)
    )
{
	AKRESULT eResult = UnloadBank(in_Bank->GetName(), out_pMemPoolId);
	if( eResult == AK_Success && AkBankManager != NULL)
	{
		FScopeLock Lock(&AkBankManager->m_BankManagerCriticalSection);
		AkBankManager->RemoveLoadedBank(in_Bank);
	}
	return eResult;
}

/**
 * Unload a soundbank by its name
 *
 * @param in_BankName		The name of the bank to unload
 * @param out_pMemPoolId	Returned memory pool ID used with LoadBank() (can pass NULL)
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::UnloadBank(
	const FString&      in_BankName,
    AkMemPoolId *       out_pMemPoolId		    ///< Returned memory pool ID used with LoadBank() (can pass NULL)
    )
{
	AKRESULT eResult = AK_Fail;
	if ( m_bSoundEngineInitialized )
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szString = TCHAR_TO_ANSI(*in_BankName);
#else
		const WIDECHAR * szString = *in_BankName;
#endif
		eResult = AK::SoundEngine::UnloadBank( szString, out_pMemPoolId );
	}
	return eResult;
}

static void AkAudioDeviceBankUnloadCallback(	
	AkUInt32		in_bankID,
	const void *	in_pInMemoryBankPtr,
	AKRESULT		in_eLoadResult,
	AkMemPoolId		in_memPoolId,
	void *			in_pCookie
)
{
	FAkAudioDevice * akAudioDevice = FAkAudioDevice::Get();
	AkBankCallbackFunc cbFunc = NULL;
	if( akAudioDevice )
	{
		FAkBankManager * BankManager = akAudioDevice->GetAkBankManager();
		if( BankManager )
		{
			FScopeLock Lock(&BankManager->m_BankManagerCriticalSection);
			FAkBankManager::AkBankCallbackInfo * cbInfo = BankManager->GetBankUnloadCallbackInfo(in_pCookie);
			if( cbInfo != NULL )
			{
				cbFunc = cbInfo->CallbackFunc;
			}

			if( in_eLoadResult == AK_Success )
			{
				// Load worked; put the bank in the list.
				BankManager->RemoveLoadedBank(cbInfo->pBank);
			}

			BankManager->RemoveBankUnloadCallbackInfo(in_pCookie);
		}
	}

	if( cbFunc != NULL )
	{
		// Call the user's callback function
		cbFunc(in_bankID, in_pInMemoryBankPtr, in_eLoadResult, in_memPoolId, in_pCookie);
	}
	
}

/**
 * Unload a soundbank asynchronously
 *
 * @param in_Bank		The bank to unload
 * @param in_pfnBankCallback Callback function
 * @param in_pCookie		Callback cookie (reserved to user, passed to the callback function)
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::UnloadBank(
	class UAkAudioBank *     in_Bank,
	AkBankCallbackFunc  in_pfnBankCallback,
	void *              in_pCookie
    )
{
	if ( m_bSoundEngineInitialized )
	{
		FString name = in_Bank->GetName();
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szString = TCHAR_TO_ANSI(*name);
#else
		const WIDECHAR * szString = *name;
#endif
		if( AkBankManager != NULL )
		{
			FAkBankManager::AkBankCallbackInfo cbInfo(in_pfnBankCallback, in_Bank);

			// We need a unique cookie for the map. If none, use the bank as cookie
			if( in_pCookie == NULL )
			{
				in_pCookie = in_Bank;
			}

			// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
			AkBankManager->m_BankManagerCriticalSection.Lock();
			AkBankManager->AddBankUnloadCallbackInfo(in_pCookie, cbInfo);
			AkBankManager->m_BankManagerCriticalSection.Unlock();

			return AK::SoundEngine::UnloadBank(szString, NULL, AkAudioDeviceBankUnloadCallback, in_pCookie);
		}
		else
		{
			return AK::SoundEngine::UnloadBank(szString, NULL, in_pfnBankCallback, in_pCookie);
		}
	}
	return AK_Fail;
}

/**
 * Load the audiokinetic 'init' bank
 *
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::LoadInitBank(void)
{
	AkBankID BankID;
#ifndef AK_SUPPORT_WCHAR
	ANSICHAR* szString = TCHAR_TO_ANSI(INITBANKNAME);
#else
	const WIDECHAR * szString = INITBANKNAME;
#endif		
	return AK::SoundEngine::LoadBank( szString, AK_DEFAULT_POOL_ID, BankID );
}

/**
 * Unload the audiokinetic 'init' bank
 *
 * @return Result from ak sound engine 
 */
AKRESULT FAkAudioDevice::UnloadInitBank(void)
{
#ifndef AK_SUPPORT_WCHAR
	ANSICHAR* szString = TCHAR_TO_ANSI(INITBANKNAME);
#else
	const WIDECHAR * szString = INITBANKNAME;
#endif
	return AK::SoundEngine::UnloadBank( szString, NULL );
}

/**
 * Load all banks currently being referenced
 */
void FAkAudioDevice::LoadAllReferencedBanks()
{
	LoadInitBank();

	// Load any banks that are in memory that haven't been loaded yet
	for( TObjectIterator<UAkAudioBank> It; It; ++It )
	{
		if ( (*It)->AutoLoad )
			(*It)->Load();
	}
}

/**
 * Reload all banks currently being referenced
 */
void FAkAudioDevice::ReloadAllReferencedBanks()
{
	if ( m_bSoundEngineInitialized )
	{
		StopAllSounds();
		AK::SoundEngine::RenderAudio();
		FPlatformProcess::Sleep(0.1f);
		ClearBanks();

		LoadAllReferencedBanks();
	}
}

/**
 * Post an event to ak soundengine
 *
 * @param in_pEvent			Event to post
 * @param in_pComponent		AkComponent on which to play the event
 * @param in_uFlags			Bitmask: see \ref AkCallbackType
 * @param in_pfnCallback	Callback function
 * @param in_pCookie		Callback cookie that will be sent to the callback function along with additional information.
 * @param in_bStopWhenOwnerDestroyed If true, then the sound should be stopped if the owning actor is destroyed
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEvent(
	UAkAudioEvent * in_pEvent, 
	AActor * in_pActor,
	AkUInt32 in_uFlags /*= 0*/,
	AkCallbackFunc in_pfnCallback /*= NULL*/,
	void * in_pCookie /*= NULL*/,
	bool in_bStopWhenOwnerDestroyed /*= false*/
    )
{
	return PostEvent(in_pEvent->GetName(), in_pActor, in_uFlags, in_pfnCallback, in_pCookie, in_bStopWhenOwnerDestroyed);
}

/**
 * Post an event to ak soundengine by name
 *
 * @param in_EventName		Name of the event to post
 * @param in_pComponent		AkComponent on which to play the event
 * @param in_uFlags			Bitmask: see \ref AkCallbackType
 * @param in_pfnCallback	Callback function
 * @param in_pCookie		Callback cookie that will be sent to the callback function along with additional information.
 * @param in_bStopWhenOwnerDestroyed If true, then the sound should be stopped if the owning actor is destroyed
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName, 
	AActor * in_pActor,
	AkUInt32 in_uFlags /*= 0*/,
	AkCallbackFunc in_pfnCallback /*= NULL*/,
	void * in_pCookie /*= NULL*/,
	bool in_bStopWhenOwnerDestroyed /*= false*/
    )
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if ( m_bSoundEngineInitialized )
	{
		// PostEvent must be bound to a game object. Passing DUMMY_GAMEOBJ as default game object.
		AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
		if( GetGameObjectID( in_pActor, GameObjID, in_bStopWhenOwnerDestroyed ) == AK_Success )
		{
			playingID = PostEventInternal( *in_EventName, (UAkComponent *)GameObjID, in_uFlags, in_pfnCallback, in_pCookie );
		}
	}
	return playingID;
}

/** Find AAkReverbVolumes at a given location
 *
 * @param							Loc	Location at which to find Reverb Volumes
 * @param FoundVolumes		Array containing all found volumes at this location
 */
void FindAkReverbVolumesAtLocation(FVector Loc, TArray<AAkReverbVolume*>& FoundVolumes, const UWorld* World)
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	FoundVolumes.Empty();

	if( AkAudioDevice )
	{
		uint32 NumVolumesAdded = 0;
		AAkReverbVolume** TopVolume = AkAudioDevice->HighestPriorityAkReverbVolumeMap.Find(World);

		if( TopVolume )
		{
			AAkReverbVolume* CurrentVolume = *TopVolume;
			while( CurrentVolume )
			{
  				if( CurrentVolume->EncompassesPoint(Loc) && CurrentVolume->bEnabled )
				{
					FoundVolumes.Add(CurrentVolume);
				}

				CurrentVolume = CurrentVolume->NextLowerPriorityAkReverbVolume;
			}
		}
	}
}

/** Add a AkReverbVolume in the active volumes linked list. */
void FAkAudioDevice::AddAkReverbVolumeInList(class AAkReverbVolume* in_VolumeToAdd)
{
	UWorld* World = in_VolumeToAdd->GetWorld();
	AAkReverbVolume*& HighestPriorityAkReverbVolume = HighestPriorityAkReverbVolumeMap.FindOrAdd(World);

	if( HighestPriorityAkReverbVolume == NULL )
	{
		// First volume in the list. Set head.
		HighestPriorityAkReverbVolume = in_VolumeToAdd;
		in_VolumeToAdd->NextLowerPriorityAkReverbVolume = NULL;
	}
	else
	{
		AAkReverbVolume* CurrentVolume = HighestPriorityAkReverbVolume;
		AAkReverbVolume* PreviousVolume = NULL;

		while( CurrentVolume && CurrentVolume != in_VolumeToAdd ) // Don't add twice to the list!
		{
			if( in_VolumeToAdd->Priority > CurrentVolume->Priority )
			{
				// Found our spot in the list!
				if ( PreviousVolume )
				{
					PreviousVolume->NextLowerPriorityAkReverbVolume = in_VolumeToAdd;
				}
				else
				{
					// No previous, so we are at the top.
					HighestPriorityAkReverbVolume = in_VolumeToAdd;
				}

				in_VolumeToAdd->NextLowerPriorityAkReverbVolume = CurrentVolume;
				return;
			}

			// List traversal.
			PreviousVolume = CurrentVolume;
			CurrentVolume = CurrentVolume->NextLowerPriorityAkReverbVolume;
		}

		// We're at the end!
		if( !CurrentVolume )
		{
			// Just to make sure...
			if( PreviousVolume )
			{
				PreviousVolume->NextLowerPriorityAkReverbVolume = in_VolumeToAdd;
				in_VolumeToAdd->NextLowerPriorityAkReverbVolume = NULL;
			}
		}
	}
}

/** Remove a AkReverbVolume from the active volumes linked list. */
void FAkAudioDevice::RemoveAkReverbVolumeFromList(class AAkReverbVolume* in_VolumeToRemove)
{
	UWorld* World = in_VolumeToRemove->GetWorld();
	AAkReverbVolume** HighestPriorityAkReverbVolume = HighestPriorityAkReverbVolumeMap.Find(World);

	if( HighestPriorityAkReverbVolume )
	{
		AAkReverbVolume* CurrentVolume = *HighestPriorityAkReverbVolume;
		AAkReverbVolume* PreviousVolume = NULL;
		while( CurrentVolume )
		{
			if( CurrentVolume == in_VolumeToRemove )
			{
				// Found our volume, remove it from the list
				if( PreviousVolume )
				{
					PreviousVolume->NextLowerPriorityAkReverbVolume = CurrentVolume->NextLowerPriorityAkReverbVolume;
				}
				else
				{
					// The one to remove was the highest, reset the head.
					*HighestPriorityAkReverbVolume = CurrentVolume->NextLowerPriorityAkReverbVolume;
				}

				break;
			}

			PreviousVolume = CurrentVolume;
			CurrentVolume = CurrentVolume->NextLowerPriorityAkReverbVolume;
		}

		// Don't leave dangling pointers.
		in_VolumeToRemove->NextLowerPriorityAkReverbVolume = NULL;

		if( *HighestPriorityAkReverbVolume == NULL )
		{
			HighestPriorityAkReverbVolumeMap.Remove(World);
		}
	}
}


/** Get a sorted list of AkAuxSendValue at a location
 *
 * @param					Loc	Location at which to find Reverb Volumes
 * @param AkReverbVolumes	Array of AkAuxSendValue at this location
 */
void GetReverbVolumesOnTempEvent(FVector Loc, TArray<AkAuxSendValue>& AkReverbVolumes, const UWorld* World)
{
	// Check if there are AkReverbVolumes at this location
	TArray<AAkReverbVolume*> FoundVolumes;
	FindAkReverbVolumesAtLocation(Loc, FoundVolumes, World);

	// Sort the found Volumes
	if(FoundVolumes.Num() > 1 )
	{
		struct FCompareAkReverbVolumeByPriority
		{
			FORCEINLINE bool operator()( const AAkReverbVolume& A, const AAkReverbVolume& B ) const 
			{ 
				return A.Priority > B.Priority; 
			}
		};
		FoundVolumes.Sort(FCompareAkReverbVolumeByPriority());
	}

	// Apply the found Aux Sends
	AkAuxSendValue	TmpSendValue;
	// Build a list to set as AuxBusses
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	uint8 MaxAuxBus = AK_MAX_AUX_PER_OBJ;
	if( AkAudioDevice )
	{
		MaxAuxBus = AkAudioDevice->GetMaxAuxBus();
	}
	for( uint8 Idx = 0; Idx < FoundVolumes.Num() && Idx < MaxAuxBus; Idx++ )
	{
		TmpSendValue.auxBusID = FoundVolumes[Idx]->GetAuxBusId();
		TmpSendValue.fControlValue = FoundVolumes[Idx]->SendLevel;
		AkReverbVolumes.Add(TmpSendValue);
	}
}

/**
 * Post an event and location to ak soundengine
 *
 * @param in_pEvent			Name of the event to post
 * @param in_Location		Location at which to play the event
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEventAtLocation(
	UAkAudioEvent * in_pEvent,
	FVector in_Location,
	FVector in_Orientation,
	UWorld* World )
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if ( in_pEvent )
	{
		playingID = PostEventAtLocation(in_pEvent->GetName(), in_Location, in_Orientation, World);
	}

	return playingID;
}

/**
 * Post an event by name at location to ak soundengine
 *
 * @param in_pEvent			Name of the event to post
 * @param in_Location		Location at which to play the event
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEventAtLocation(
	const FString& in_EventName,
	FVector in_Location,
	FVector in_Orientation,
	UWorld* World)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if ( m_bSoundEngineInitialized )
	{
		AkGameObjectID objId = (AkGameObjectID)&in_EventName;

#ifndef AK_OPTIMIZED
		RegisterGameObj_WithName( objId, *in_EventName );
#else // AK_OPTIMIZED
		AK::SoundEngine::RegisterGameObj( objId );
#endif // AK_OPTIMIZED

		TArray<AkAuxSendValue> AkReverbVolumes;
		GetReverbVolumesOnTempEvent(in_Location, AkReverbVolumes, World);
		SetAuxSends(objId, AkReverbVolumes);

		AkSoundPosition soundpos;
		FVectorToAKVector( in_Location, soundpos.Position );
		FVectorToAKVector( in_Orientation, soundpos.Orientation );

		if ( soundpos.Orientation.X == 0.0 && soundpos.Orientation.Y == 0.0 && soundpos.Orientation.Z == 0.0 )
		{
			UE_LOG(LogAkAudio, Error, TEXT("Orientation Front vector invalid!") );
		}

		AK::SoundEngine::SetPosition( objId, soundpos );

#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szEventName = TCHAR_TO_ANSI(*in_EventName);
#else
		const WIDECHAR * szEventName = *in_EventName;
#endif
		playingID = AK::SoundEngine::PostEvent( szEventName, objId, 0, NULL, NULL);

		AK::SoundEngine::UnregisterGameObj( objId );
	}
	return playingID;
}

UAkComponent* FAkAudioDevice::SpawnAkComponentAtLocation( class UAkAudioEvent* in_pAkEvent, FVector Location, FRotator Orientation, bool AutoPost, const FString& EventName, bool AutoDestroy, UWorld* World )
{
	UAkComponent * AkComponent = NewObject<UAkComponent>(World->GetWorldSettings());
	if( AkComponent )
	{
		AkComponent->AkAudioEvent = in_pAkEvent;
		AkComponent->EventName = EventName;
		AkComponent->SetWorldLocationAndRotation(Location, Orientation.Quaternion());
		if(World)
		{
			AkComponent->RegisterComponentWithWorld(World);
		}

		AkComponent->SetAutoDestroy(AutoDestroy);

		if(AutoPost)
		{
			AkComponent->PostAssociatedAkEvent();
		}
	}

	return AkComponent;
}

/**
 * Post a trigger to ak soundengine
 *
 * @param in_pszTrigger		Name of the trigger
 * @param in_pAkComponent	AkComponent on which to post the trigger
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::PostTrigger( 
	const TCHAR * in_pszTrigger,
	AActor * in_pActor
	)
{
	AkGameObjectID GameObjID = AK_INVALID_GAME_OBJECT;
	AKRESULT eResult = GetGameObjectID( in_pActor, GameObjID );
	if ( m_bSoundEngineInitialized && eResult == AK_Success)
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szTrigger = TCHAR_TO_ANSI(in_pszTrigger);
#else
		const WIDECHAR * szTrigger = in_pszTrigger;
#endif
		eResult = AK::SoundEngine::PostTrigger( szTrigger, GameObjID );
	}
	return eResult;
} 

/**
 * Set a RTPC in ak soundengine
 *
 * @param in_pszRtpcName	Name of the RTPC
 * @param in_value			Value to set
 * @param in_pActor			Actor on which to set the RTPC
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetRTPCValue( 
	const TCHAR * in_pszRtpcName,
	AkRtpcValue in_value,
	int32 in_interpolationTimeMs = 0,
	AActor * in_pActor = NULL
	)
{
	AKRESULT eResult = AK_Success;
	if ( m_bSoundEngineInitialized )
	{
		AkGameObjectID GameObjID = AK_INVALID_GAME_OBJECT; // RTPC at global scope is supported
		if ( in_pActor )
		{
			eResult = GetGameObjectID( in_pActor, GameObjID );
			if ( eResult != AK_Success )
				return eResult;
		}

#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szRtpcName = TCHAR_TO_ANSI(in_pszRtpcName);
#else
		const WIDECHAR * szRtpcName = in_pszRtpcName;
#endif
		eResult = AK::SoundEngine::SetRTPCValue( szRtpcName, in_value, GameObjID, in_interpolationTimeMs );
	}
	return eResult;
}

/**
 * Set a state in ak soundengine
 *
 * @param in_pszStateGroup	Name of the state group
 * @param in_pszState		Name of the state
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetState( 
	const TCHAR * in_pszStateGroup,
	const TCHAR * in_pszState
    )
{
	AKRESULT eResult = AK_Success;
	if ( m_bSoundEngineInitialized )
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szStateGroup = TCHAR_TO_ANSI(in_pszStateGroup);
		ANSICHAR* szState = TCHAR_TO_ANSI(in_pszState);
#else
		const WIDECHAR * szStateGroup = in_pszStateGroup;
		const WIDECHAR * szState = in_pszState;
#endif
		eResult = AK::SoundEngine::SetState( szStateGroup, szState );
	}
	return eResult;
}

/**
 * Set a switch in ak soundengine
 *
 * @param in_pszSwitchGroup	Name of the switch group
 * @param in_pszSwitchState	Name of the switch
 * @param in_pComponent		AkComponent on which to set the switch
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetSwitch( 
	const TCHAR * in_pszSwitchGroup,
	const TCHAR * in_pszSwitchState,
	AActor * in_pActor
	)
{
	AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
	// Switches must be bound to a game object. passing DUMMY_GAMEOBJ as default game object.
	AKRESULT eResult = GetGameObjectID( in_pActor, GameObjID );
	if ( m_bSoundEngineInitialized && eResult == AK_Success)
	{
#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szSwitchGroup = TCHAR_TO_ANSI(in_pszSwitchGroup);
		ANSICHAR* szSwitchState = TCHAR_TO_ANSI(in_pszSwitchState);
#else
		const WIDECHAR * szSwitchGroup = in_pszSwitchGroup;
		const WIDECHAR * szSwitchState = in_pszSwitchState;
#endif
		eResult = AK::SoundEngine::SetSwitch( szSwitchGroup, szSwitchState, GameObjID );
	}
	return eResult;
}
	
/**
 * Activate an occlusion
 *
 * @param in_bActivate		If true, the occlusion should be activated
 * @param in_pComponent		AkComponent on which to activate the occlusion
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetOcclusionObstruction(
	const UAkComponent * const in_pAkComponent,
	const int32 in_ListenerIndex,
	const float in_Obstruction,
	const float in_Occlusion
	)
{
	AKRESULT eResult = AK_Success;
	AkGameObjectID gameObjId = DUMMY_GAMEOBJ;
	if ( in_pAkComponent )
	{
		gameObjId = (AkGameObjectID)in_pAkComponent;
	}

	if ( m_bSoundEngineInitialized )
	{
		eResult = AK::SoundEngine::SetObjectObstructionAndOcclusion( gameObjId, in_ListenerIndex, in_Obstruction, in_Occlusion );
	}

	return eResult;
}

/**
 * Set auxiliary sends
 *
 * @param in_GameObjId		Wwise Game Object ID
 * @param in_AuxSendValues	Array of AkAuxSendValue, containins all Aux Sends to set on the game objectt
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetAuxSends(
	const AkGameObjectID in_GameObjId,
	TArray<AkAuxSendValue>& in_AuxSendValues
	)
{
	AKRESULT eResult = AK_Success;
	if ( m_bSoundEngineInitialized )
	{
		eResult = AK::SoundEngine::SetGameObjectAuxSendValues( in_GameObjId, in_AuxSendValues.GetData(), in_AuxSendValues.Num() );
	}

	return eResult;
}



/**
 * Obtain a pointer to the singleton instance of FAkAudioDevice
 *
 * @return Pointer to the singleton instance of FAkAudioDevice
 */
FAkAudioDevice * FAkAudioDevice::Get()
{
	FAkAudioModule& AkAudio = FModuleManager::LoadModuleChecked<FAkAudioModule>(TEXT("AkAudio"));
	return AkAudio.IsAvailable() ? AkAudio.GetAkAudioDevice() : NULL;
}

/**
 * Stop all audio associated with a game object
 *
 * @param in_GameObjID		ID of the game object
 */
void FAkAudioDevice::StopGameObject( UAkComponent * in_pComponent )
{
	AkGameObjectID gameObjId = DUMMY_GAMEOBJ;
	if ( in_pComponent )
	{
		gameObjId = (AkGameObjectID)in_pComponent;
	}
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopAll( gameObjId );
	}
}

/**
 * Stop all audio associated with a playing ID
 *
 * @param in_playingID		Playing ID to stop
 */
void FAkAudioDevice::StopPlayingID( AkPlayingID in_playingID )
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopPlayingID( in_playingID );
	}
}


/**
 * Register an ak audio component with ak sound engine
 *
 * @param in_pComponent		Pointer to the component to register
 */
void FAkAudioDevice::RegisterComponent( UAkComponent * in_pComponent )
{
	AActor * parentActor = NULL;
	if (in_pComponent != NULL)
	{
		parentActor = in_pComponent->GetOwner();
	}

	if ( m_bSoundEngineInitialized )
	{
#ifndef AK_OPTIMIZED
		if ( parentActor )
		{
			FString objectName( parentActor->GetFName().ToString() );
			RegisterGameObj_WithName( (AkGameObjectID) in_pComponent, *objectName );
		}
		else
#endif // AK_OPTIMIZED
		{
			AK::SoundEngine::RegisterGameObj( (AkGameObjectID) in_pComponent );
		}
	}
}

/**
 * Unregister an ak audio component with ak sound engine
 *
 * @param in_pComponent		Pointer to the component to unregister
 */
void FAkAudioDevice::UnregisterComponent( UAkComponent * in_pComponent )
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::UnregisterGameObj( (AkGameObjectID) in_pComponent );
	}
}

UAkComponent* FAkAudioDevice::GetAkComponent( class USceneComponent* AttachToComponent, FName AttachPointName, const FVector * Location, EAttachLocation::Type LocationType )
{
	check( AttachToComponent );

	UAkComponent* AkComponent = NULL;

	if( GEngine && AK::SoundEngine::IsInitialized())
	{
		AActor * Actor = AttachToComponent->GetOwner();
		if( Actor ) 
		{
			if( Actor->IsPendingKill() )
			{
				// Avoid creating component if we're trying to play a sound on an already destroyed actor.
				return NULL;
			}

			TArray<UAkComponent*> AkComponents;
			Actor->GetComponents(AkComponents);
			for ( int32 CompIdx = 0; CompIdx < AkComponents.Num(); CompIdx++ )
			{
				UAkComponent* pCompI = AkComponents[CompIdx];
				if ( pCompI && pCompI->IsRegistered() )
				{
					if ( AttachToComponent == pCompI )
					{
						return pCompI;
					}

					if ( AttachToComponent != pCompI->GetAttachParent() 
						|| AttachPointName != pCompI->AttachSocketName )
					{
						continue;
					}

					// If a location is requested, try to match location.
					if ( Location )
					{
						if (LocationType == EAttachLocation::KeepWorldPosition)
						{
							if ( !FVector::PointsAreSame(*Location, pCompI->GetComponentLocation()) )
								continue;
						}
						else
						{
							if ( !FVector::PointsAreSame(*Location, pCompI->RelativeLocation) )
								continue;
						}
					}

					// AkComponent found which exactly matches the attachment: reuse it.
					return pCompI;
				}
			}
		}
		else
		{
			// Try to find if there is an AkComponent attached to AttachToComponent (will be the case if AttachToComponent has no owner)
			for(int32 CompIdx = 0; CompIdx < AttachToComponent->AttachChildren.Num(); CompIdx++)
			{
				UAkComponent* pCompI = Cast<UAkComponent>(AttachToComponent->AttachChildren[CompIdx]);
				if ( pCompI && pCompI->IsRegistered() )
				{
					// There is an associated AkComponent to AttachToComponent, no need to add another one.
					return pCompI;
				}
			}
		}

		if ( AkComponent == NULL )
		{
			if( Actor )
			{
				AkComponent = NewObject<UAkComponent>(Actor);
			}
			else
			{
				AkComponent = NewObject<UAkComponent>();
			}
		}

		check( AkComponent );

		if (Location)
		{
			if (LocationType == EAttachLocation::KeepWorldPosition)
			{
				AkComponent->SetWorldLocation(*Location);
			}
			else
			{
				AkComponent->SetRelativeLocation(*Location);
			}
		}

		AkComponent->RegisterComponentWithWorld(AttachToComponent->GetWorld());
		AkComponent->AttachTo(AttachToComponent, AttachPointName, LocationType);
	}

	return( AkComponent );
}


/**
 * Cancel the callback cookie for a dispatched event 
 *
 * @param in_cookie			The cookie to cancel
 */
void FAkAudioDevice::CancelEventCallbackCookie( void* in_cookie )
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::CancelEventCallbackCookie( in_cookie );
	}
}

AKRESULT FAkAudioDevice::SetAttenuationScalingFactor(AActor* Actor, float ScalingFactor)
{
	AKRESULT eResult = AK_Fail;
	if ( m_bSoundEngineInitialized )
	{
		AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
		eResult = GetGameObjectID( Actor, GameObjID );
		if( eResult == AK_Success )
		{
			eResult = AK::SoundEngine::SetAttenuationScalingFactor(GameObjID, ScalingFactor);
		}
	}

	return eResult;
}

AKRESULT FAkAudioDevice::SetAttenuationScalingFactor(UAkComponent* AkComponent, float ScalingFactor)
{
	AKRESULT eResult = AK_Fail;
	if ( m_bSoundEngineInitialized )
	{
		eResult = AK::SoundEngine::SetAttenuationScalingFactor((AkGameObjectID)AkComponent, ScalingFactor);
	}
	return eResult;
}


#ifdef AK_SOUNDFRAME

/**
 * Called when sound frame connects 
 *
 * @param in_bConnect		True if Wwise is connected, False if it is not
 */
void FAkAudioDevice::OnConnect( 
		bool in_bConnect		///< True if Wwise is connected, False if it is not
		)
{
	if ( in_bConnect == true )
	{
		UE_LOG(	LogAkAudio,
			Log,
			TEXT("SoundFrame successfully connected."));
	}
	else
	{
		UE_LOG(	LogAkAudio,
			Log,
			TEXT("SoundFrame failed to connect."));
	}
}
	
/**
 * Event notification. This method is called when an event is added, removed, changed, or pushed.
 *
 * @param in_eNotif			Notification type
 * @param in_eventID		Unique ID of the event
 */	
void FAkAudioDevice::OnEventNotif( 
	Notif in_eNotif,
	AkUniqueID in_eventID
	)	
{
#if WITH_EDITORONLY_DATA
	if ( in_eNotif == IClient::Notif_Changed )
	{
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
#endif
}

/**
 * Sound object notification. This method is called when a sound object is added, removed, or changed.
 *
 * @param in_eNotif			Notification type
 * @param in_soundObjectID	Unique ID of the sound object
 */
void FAkAudioDevice::OnSoundObjectNotif( 
	Notif in_eNotif,
	AkUniqueID in_soundObjectID
	)
{
#if WITH_EDITORONLY_DATA
	if ( in_eNotif == IClient::Notif_Changed )
	{
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
#endif
}

#endif

#if PLATFORM_WINDOWS || PLATFORM_MAC
static void UELocalOutputFunc(
	AK::Monitor::ErrorCode in_eErrorCode,
	const AkOSChar* in_pszError,
	AK::Monitor::ErrorLevel in_eErrorLevel,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID )
{
    wchar_t* szWideError;
#if PLATFORM_MAC
    CONVERT_OSCHAR_TO_WIDE(in_pszError, szWideError);
#else
    szWideError = (wchar_t*)in_pszError;
#endif
    
	if( !IsRunningCommandlet() )
	{
		if ( in_eErrorLevel == AK::Monitor::ErrorLevel_Message )
		{
			UE_LOG( LogAkAudio, Log, TEXT("%s"), szWideError );
		}
		else
		{
			UE_LOG( LogAkAudio, Error, TEXT("%s"), szWideError );
		}
	}
}
#endif

bool FAkAudioDevice::EnsureInitialized()
{
	// We don't want sound in those cases.
	if (FParse::Param(FCommandLine::Get(), TEXT("nosound")) || FApp::IsBenchmarking() || IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		return false;
	}

	if ( m_bSoundEngineInitialized )
	{
		return true;
	}

#if PLATFORM_WINDOWS 
#if PLATFORM_64BITS
	// Work around the fact the x64 version of XAudio2_7.dll does not properly ref count
	// by forcing it to be always loaded
	LoadLibraryA( "XAudio2_7.dll" );
#endif	//PLATFORM_64BITS
#endif	//PLATFORM_WINDOWS

#if PLATFORM_XBOXONE
#ifndef AK_OPTIMIZED
	try
	{
		// Make sure networkmanifest.xml is loaded by instantiating a Microsoft.Xbox.Networking object.
		auto secureDeviceAssociationTemplate = Windows::Xbox::Networking::SecureDeviceAssociationTemplate::GetTemplateByName( "WwiseDiscovery" );
	}
	catch(...)
	{
		UE_LOG(LogAkAudio, Log, TEXT("Could not find Wwise network ports in AppxManifest. Network communication will not be available."));
	}
#endif
#endif

	m_listenerPositions.Empty();

	UE_LOG(	LogAkAudio,
			Log,
			TEXT("Wwise(R) SDK Version %d.%d.%d Build %d. Copyright (c) 2006-%d Audiokinetic Inc."),
			AK_WWISESDK_VERSION_MAJOR, 
			AK_WWISESDK_VERSION_MINOR, 
			AK_WWISESDK_VERSION_SUBMINOR, 
			AK_WWISESDK_VERSION_BUILD,
			AK_WWISESDK_VERSION_MAJOR );

	AkMemSettings memSettings;
	memSettings.uMaxNumPools = 256;

	if ( AK::MemoryMgr::Init( &memSettings ) != AK_Success )
	{
        return false;
	}

	AkStreamMgrSettings stmSettings;
	AK::StreamMgr::GetDefaultSettings( stmSettings );
	AK::IAkStreamMgr * pStreamMgr = AK::StreamMgr::Create( stmSettings );
	if ( ! pStreamMgr )
	{
        return false;
	}

	AkDeviceSettings deviceSettings;
	AK::StreamMgr::GetDefaultDeviceSettings( deviceSettings );
	// todo:mjb@ak - Not sure where DVD_MIN_READ_SIZE gets defined
	deviceSettings.uGranularity = AK_UNREAL_IO_GRANULARITY;
	deviceSettings.uSchedulerTypeFlags = AK_SCHEDULER_DEFERRED_LINED_UP;
	deviceSettings.uMaxConcurrentIO = AK_UNREAL_MAX_CONCURRENT_IO;

	if ( g_lowLevelIO.Init( deviceSettings, true ) != AK_Success )
	{
        return false;
	}

	AkInitSettings initSettings;
	AkPlatformInitSettings platformInitSettings;
	AK::SoundEngine::GetDefaultInitSettings( initSettings );
	AK::SoundEngine::GetDefaultPlatformInitSettings( platformInitSettings );

#if defined AK_WIN
	// Make the sound to not be audible when the game is minimized.

	auto GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine && GameEngine->GameViewportWindow.IsValid() )
	{
		platformInitSettings.hWnd = (HWND)GameEngine->GameViewportWindow.Pin()->GetNativeWindow()->GetOSWindowHandle();
		platformInitSettings.bGlobalFocus = false;
	}

#endif

	// OCULUS_START vhamm audio redirect with build of wwise >= 2015.1.5
#if AK_WWISESDK_VERSION_BUILD >= 5528
	if (IHeadMountedDisplayModule::IsAvailable())
	{
		FHeadMountedDisplayModuleExt* const HmdEx = FHeadMountedDisplayModuleExt::GetExtendedInterface(&IHeadMountedDisplayModule::Get());
		FString AudioOutputDevice = HmdEx ? HmdEx->GetAudioOutputDevice() : FString();

		if(!AudioOutputDevice.IsEmpty())
		{
			initSettings.eMainOutputType = AkAudioAPI::AkAPI_Wasapi;
			platformInitSettings.idAudioDevice = AK::GetDeviceIDFromName((wchar_t*) *AudioOutputDevice);
		}
	}
#endif
	// OCULUS_END

	if ( AK::SoundEngine::Init( &initSettings, &platformInitSettings ) != AK_Success )
	{
        return false;
	}

	AkMusicSettings musicInit;
	AK::MusicEngine::GetDefaultInitSettings( musicInit );

	if ( AK::MusicEngine::Init( &musicInit ) != AK_Success )
	{
        return false;
	}

#if PLATFORM_WINDOWS || PLATFORM_MAC
	// Enable AK error redirection to UE log.
	AK::Monitor::SetLocalOutput( AK::Monitor::ErrorLevel_All, UELocalOutputFunc );
#endif

#ifndef AK_OPTIMIZED
#if !PLATFORM_LINUX
    //
    // Initialize communications, not in release build, and only for a game (and not the project selection screen, for example)
    //
	if(FApp::HasGameName())
	{
		FString GameName = FApp::GetGameName();
#if WITH_EDITORONLY_DATA
		GameName += TEXT(" (Editor)");
#endif
		AkCommSettings commSettings;
		AK::Comm::GetDefaultInitSettings( commSettings );
		FCStringAnsi::Strcpy(commSettings.szAppNetworkName, AK_COMM_SETTINGS_MAX_STRING_SIZE, TCHAR_TO_ANSI(*GameName));
		if ( AK::Comm::Init( commSettings ) != AK_Success )
		{
			UE_LOG(LogInit, Warning, TEXT("Could not initialize communication. GameName is %s"), *GameName);
			//return false;
		}
	}
#endif
#endif // AK_OPTIMIZED

	// Register built-in plug-ins.
	AK::SoundEngine::RegisterAllBuiltInPlugins();

	// Plug-ins which require additional license.
	AK::SoundEngine::RegisterConvolutionReverbPlugin();
	// AK::SoundEngine::RegisterSoundSeedPlugins();
	// AK::SoundEngine::RegisterMcDSPPlugins();
	// AK::SoundEngine::RegisterAstoundSoundPlugins();
	// AK::SoundEngine::RegisteriZotopePlugins();
	// AK::SoundEngine::RegisterCrankcaseAudioPlugins();

	//
	// Setup banks path
	//
	SetBankDirectory();

	// Init dummy game object
	AK::SoundEngine::RegisterGameObj(DUMMY_GAMEOBJ, "Unreal Global");

	m_bSoundEngineInitialized = true;
	
	AkBankManager = new FAkBankManager;

	LoadAllReferencedBanks();

	// Go get the max number of Aux busses
	const UAkSettings* AkSettings = GetDefault<UAkSettings>();
	MaxAuxBus = AK_MAX_AUX_PER_OBJ;
	if( AkSettings )
	{
		MaxAuxBus = AkSettings->MaxSimultaneousReverbVolumes;
	}

	
	return true;
}

void FAkAudioDevice::SetBankDirectory()
{
#ifdef AK_USE_UNREAL_IO

	FString BasePath = FPaths::Combine(*FPaths::GameContentDir(), TEXT("WwiseAudio"));
	
	#if defined AK_WIN
		BasePath = FPaths::Combine(*BasePath, TEXT("Windows/"));
    #elif defined AK_LINUX
        BasePath = FPaths::Combine(*BasePath, TEXT("Linux/"));
    #elif defined AK_MAC_OS_X
        BasePath = FPaths::Combine(*BasePath, TEXT("Mac/"));
	#elif defined AK_PS4
		BasePath = FPaths::Combine(*BasePath, TEXT("PS4/"));
	#elif defined AK_XBOXONE
		BasePath = FPaths::Combine(*BasePath, TEXT("XboxOne/"));
	#elif defined AK_ANDROID
		BasePath = FPaths::Combine(*BasePath, TEXT("Android/"));
    #elif defined AK_IOS
        BasePath = FPaths::Combine(*BasePath, TEXT("iOS/"));
    #else
		#error "AkAudio integration is unsupported for this platform"
	#endif


#else

	#if defined AK_WIN
		FString BasePath = FPaths::Combine(*FPaths::GameContentDir(), TEXT("WwiseAudio"));
		BasePath = FPaths::Combine(*BasePath, TEXT("Windows/"));
    #elif defined AK_LINUX
        FString BasePath = FPaths::Combine(*FPaths::GameContentDir(), TEXT("WwiseAudio"));
        BasePath = FPaths::Combine(*BasePath, TEXT("Linux/"));
    #elif defined AK_MAC_OS_X
        FString BasePath = FPaths::Combine(*FPaths::GameContentDir(), TEXT("WwiseAudio"));
        BasePath = FPaths::Combine(*BasePath, TEXT("Mac/"));
	#elif defined AK_PS4
		#error "Must define here the base path to use when bypassing Unreal I/O"
	#elif defined AK_XBOXONE
		#error "Must define here the base path to use when bypassing Unreal I/O"
	#elif defined AK_ANDROID
		#error "Must define here the base path to use when bypassing Unreal I/O"
    #elif defined AK_IOS
        #error "Must define here the base path to use when bypassing Unreal I/O"
	#else
		#error "AkAudio integration is unsupported for this platform"
	#endif

#endif

	UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device setting bank directory to %s."), *BasePath);

#ifndef AK_SUPPORT_WCHAR
	AkOSChar * pszPath = TCHAR_TO_ANSI(*BasePath);
#else
	AkOSChar * pszPath = 0;
	CONVERT_WIDE_TO_OSCHAR( *BasePath, pszPath );
#endif
	g_lowLevelIO.SetBasePath( pszPath );

	AK::StreamMgr::SetCurrentLanguage( AKTEXT("English(US)") );
}

/**
 * Allocates memory from permanent pool. This memory will NEVER be freed.
 *
 * @param	Size	Size of allocation.
 *
 * @return pointer to a chunk of memory with size Size
 */
void* FAkAudioDevice::AllocatePermanentMemory( int32 Size, bool& AllocatedInPool )
{
	return 0;
}

AkPlayingID FAkAudioDevice::PostEventInternal(
		const TCHAR * in_pszEvent, 
		class UAkComponent* in_pAkComponent, 
		AkUInt32 in_uFlags /*= 0*/,					///< Bitmask: see \ref AkCallbackType
		AkCallbackFunc in_pfnCallback /*= NULL*/,	///< Callback function
		void * in_pCookie /*= NULL*/				///< Callback cookie that will be sent to the callback function along with additional information.
        )
{
	if ( m_bSoundEngineInitialized )
	{
		AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
		if ( in_pAkComponent )
		{
			GameObjID = (AkGameObjectID)in_pAkComponent;
			if( in_pAkComponent != (UAkComponent*)DUMMY_GAMEOBJ && in_pAkComponent->OcclusionRefreshInterval > 0.0f )
			{
				in_pAkComponent->CalculateOcclusionValues(false);
			}
		}

#ifndef AK_SUPPORT_WCHAR
		ANSICHAR* szEvent = TCHAR_TO_ANSI(in_pszEvent);
#else
		const WIDECHAR * szEvent = in_pszEvent;
#endif
		return AK::SoundEngine::PostEvent( szEvent, GameObjID, in_uFlags, in_pfnCallback, in_pCookie );
	}
	else
	{
		return AK_INVALID_PLAYING_ID;
	}
	return AK_INVALID_PLAYING_ID;
}

AKRESULT FAkAudioDevice::GetGameObjectID( AActor * in_pActor, AkGameObjectID& io_GameObject )
{
	if ( IsValid(in_pActor) )
	{
		UAkComponent * pComponent = GetAkComponent( in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset );
		if ( pComponent )
		{
			io_GameObject = (AkGameObjectID) pComponent;
			return AK_Success;
		}
		else
			return AK_Fail;
	}

	// we do not modify io_GameObject, letting it to the specified default value.
	return AK_Success;
}

AKRESULT FAkAudioDevice::GetGameObjectID( AActor * in_pActor, AkGameObjectID& io_GameObject, bool in_bStopWhenOwnerDestroyed )
{
	if ( IsValid(in_pActor) )
	{
		UAkComponent * pComponent = GetAkComponent( in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset );
		if ( pComponent )
		{
			pComponent->StopWhenOwnerDestroyed = in_bStopWhenOwnerDestroyed;
			io_GameObject = (AkGameObjectID) pComponent;
			return AK_Success;
		}
		else
			return AK_Fail;
	}

	// we do not modify io_GameObject, letting it to the specified default value.
	return AK_Success;
}

void FAkAudioDevice::StartOutputCapture(const FString& Filename)
{
	if ( m_bSoundEngineInitialized )
	{
#if !defined(AK_SUPPORT_WCHAR) || defined(AK_PS4) || defined(AK_LINUX) || defined(AK_MAC_OS_X) || defined(AK_IOS)
		ANSICHAR* szFilename = TCHAR_TO_ANSI(*Filename);
#else
		const WIDECHAR * szFilename = *Filename;
#endif

		AK::SoundEngine::StartOutputCapture(szFilename);
	}
}

void FAkAudioDevice::StopOutputCapture()
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopOutputCapture();
	}
}

void FAkAudioDevice::StartProfilerCapture(const FString& Filename)
{
	if ( m_bSoundEngineInitialized )
	{
#if !defined(AK_SUPPORT_WCHAR) || defined(AK_PS4) || defined(AK_LINUX) || defined(AK_MAC_OS_X) || defined(AK_IOS)
		ANSICHAR* szFilename = TCHAR_TO_ANSI(*Filename);
#else
		const WIDECHAR * szFilename = *Filename;
#endif

		AK::SoundEngine::StartProfilerCapture(szFilename);
	}
}

void FAkAudioDevice::AddOutputCaptureMarker(const FString& MarkerText)
{
	if ( m_bSoundEngineInitialized )
	{
		ANSICHAR* szText = TCHAR_TO_ANSI(*MarkerText);
		AK::SoundEngine::AddOutputCaptureMarker(szText);
	}
}

void FAkAudioDevice::StopProfilerCapture()
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopProfilerCapture();
	}
}


// end

