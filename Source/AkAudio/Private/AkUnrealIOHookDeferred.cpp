//////////////////////////////////////////////////////////////////////
//
// AkUnrealIOHookDeferred.cpp
//
// Default deferred low level IO hook (AK::StreamMgr::IAkIOHookDeferred) 
// and file system (AK::StreamMgr::IAkFileLocationResolver) implementation 
// on Windows.
// 
// AK::StreamMgr::IAkFileLocationResolver: 
// Resolves file location using simple path concatenation logic 
// (implemented in ../Common/CAkFileLocationBase). It can be used as a 
// standalone Low-Level IO system, or as part of a multi device system. 
// In the latter case, you should manage multiple devices by implementing 
// AK::StreamMgr::IAkFileLocationResolver elsewhere (you may take a look 
// at class CAkDefaultLowLevelIODispatcher).
//
// AK::StreamMgr::IAkIOHookDeferred: 
// Uses platform API for I/O. Calls to ::ReadFile() and ::WriteFile() 
// do not block because files are opened with the FILE_FLAG_OVERLAPPED flag. 
// Transfer completion is handled by internal FileIOCompletionRoutine function,
// which then calls the AkAIOCallback.
// The AK::StreamMgr::IAkIOHookDeferred interface is meant to be used with
// AK_SCHEDULER_DEFERRED_LINED_UP streaming devices. 
//
// Init() creates a streaming device (by calling AK::StreamMgr::CreateDevice()).
// AkDeviceSettings::uSchedulerTypeFlags is set inside to AK_SCHEDULER_DEFERRED_LINED_UP.
// If there was no AK::StreamMgr::IAkFileLocationResolver previously registered 
// to the Stream Manager, this object registers itself as the File Location Resolver.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "AkAudioDevice.h"

// @todo:ak verify consistency of Wwise SDK alignment settings with unreal build settings on all platforms
//			Currently, Wwise SDK builds with the default alignment (8-bytes as per MSDN) with the VC toolchain.
//			Unreal builds with 4-byte alignment on the VC toolchain on both Win32 and Win64. 
//			This could (and currently does, on Win64) cause data corruption if the headers are not included with forced alignment directives as below.
#if PLATFORM_WINDOWS
#pragma pack(push, 8)
#include "AllowWindowsPlatformTypes.h"
#elif PLATFORM_XBOXONE
#pragma pack(push, 8)
#include "XboxOneAllowPlatformTypes.h"
#endif // defined(WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(XBOX)

#include "AkUnrealIOHookDeferred.h"
#include "AkFileHelpers.h"

// The following will include the correct platform specific cpp to be compiled.
#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
#include "AkDefaultIOHookDeferred.cpp"
#endif

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#pragma pack(pop)
#elif PLATFORM_XBOXONE
#include "XboxOneHidePlatformTypes.h"
#pragma pack(pop)
#endif // defined(WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(XBOX)

#include "Core.h"

// Device info.
#if PLATFORM_ANDROID
#define POSIX_DEFERRED_DEVICE_NAME		("Android Deferred")	// Default deferred device name.
#endif
#if PLATFORM_LINUX
#define POSIX_DEFERRED_DEVICE_NAME		("Linux Deferred")	// Default deferred device name.
#endif
#if PLATFORM_MAC
#define POSIX_DEFERRED_DEVICE_NAME		("Mac Deferred")	// Default deferred device name.
#endif
#if PLATFORM_IOS
#define POSIX_DEFERRED_DEVICE_NAME		("iOS Deferred")	// Default deferred device name.
#endif
CAkUnrealIOHookDeferred::AkDeferredIOInfo CAkUnrealIOHookDeferred::aPendingTransfers[AK_UNREAL_MAX_CONCURRENT_IO];
extern CAkUnrealIOHookDeferred g_lowLevelIO;

CAkUnrealIOHookDeferred::CAkUnrealIOHookDeferred() 
	: m_bCallbackRegistered(false)
{
}

CAkUnrealIOHookDeferred::~CAkUnrealIOHookDeferred()
{
}

// Initialization/termination. Init() registers this object as the one and 
// only File Location Resolver if none were registered before. Then 
// it creates a streaming device with scheduler type AK_SCHEDULER_DEFERRED_LINED_UP.
AKRESULT CAkUnrealIOHookDeferred::Init(
	const AkDeviceSettings &	in_deviceSettings,		// Device settings.
	bool						in_bAsyncOpen/*=false*/	// If true, files are opened asynchronously when possible.
	)
{
	CriticalSection = new FCriticalSection();
	{
		FScopeLock ScopeLock( CriticalSection );

		for(int32 i = 0; i < AK_UNREAL_MAX_CONCURRENT_IO; i++)
		{
			CAkUnrealIOHookDeferred::aPendingTransfers[i].Counter.Set(-1);
		}
	}
#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
	return CAkDefaultIOHookDeferred::Init(in_deviceSettings, in_bAsyncOpen);
#else
	if ( in_deviceSettings.uSchedulerTypeFlags != AK_SCHEDULER_DEFERRED_LINED_UP )
	{
		AKASSERT( !"CAkDefaultIOHookDeferred I/O hook only works with AK_SCHEDULER_DEFERRED_LINED_UP devices" );

		return AK_Fail;
	}
	
	m_bAsyncOpen = in_bAsyncOpen;
	
	// If the Stream Manager's File Location Resolver was not set yet, set this object as the 
	// File Location Resolver (this I/O hook is also able to resolve file location).
	if ( !AK::StreamMgr::GetFileLocationResolver() )
		AK::StreamMgr::SetFileLocationResolver( this );

	// Create a device in the Stream Manager, specifying this as the hook.
	m_deviceID = AK::StreamMgr::CreateDevice( in_deviceSettings, this );
	if ( m_deviceID != AK_INVALID_DEVICE_ID )
	{
		return AK_Success;
	}


	return AK_Fail;
#endif
}

void CAkUnrealIOHookDeferred::Term()
{
	if ( m_bCallbackRegistered && FAkAudioDevice::Get() )
	{
		AK::SoundEngine::UnregisterGlobalCallback( GlobalCallback );
		m_bCallbackRegistered = false;
	}

	delete CriticalSection;

#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
	CAkDefaultIOHookDeferred::Term();
#else
	if ( AK::StreamMgr::GetFileLocationResolver() == this )
		AK::StreamMgr::SetFileLocationResolver( NULL );
	
	AK::StreamMgr::DestroyDevice( m_deviceID );
#endif
}

//
// IAkFileLocationAware implementation.
//-----------------------------------------------------------------------------

template<typename T>
AKRESULT CAkUnrealIOHookDeferred::PerformOpen( 
    T		        in_fileDescriptor,  // File ID or file Name.
    AkOpenMode      in_eOpenMode,       // Open mode.
    AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
	bool &			io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
    AkFileDesc &    out_fileDesc        // Returned file descriptor.
    )
{
	if( AK_OpenModeRead != in_eOpenMode )
	{
#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
		return CAkDefaultIOHookDeferred::Open( in_fileDescriptor, in_eOpenMode, in_pFlags, io_bSyncOpen, out_fileDesc );
#else
		return AK_Fail;
#endif
	}

	CleanFileDescriptor( out_fileDesc );
	if ( io_bSyncOpen || !m_bAsyncOpen )
	{
		io_bSyncOpen = true;
		AkOSChar lpszFilePath[AK_MAX_PATH];

		// Get the full file path, using path concatenation logic.

		if ( GetFullFilePath( in_fileDescriptor, in_pFlags, in_eOpenMode, lpszFilePath ) == AK_Success )
		{
			return FillFileDescriptorHelper( lpszFilePath, out_fileDesc );
		}
	}

	return AK_Success;
}

// Returns a file descriptor for a given file name (string).
AKRESULT CAkUnrealIOHookDeferred::Open( 
    const AkOSChar* in_pszFileName,     // File name.
    AkOpenMode      in_eOpenMode,       // Open mode.
    AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
	bool &			io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
    AkFileDesc &    out_fileDesc        // Returned file descriptor.
    )
{
	return PerformOpen(in_pszFileName, in_eOpenMode, in_pFlags, io_bSyncOpen, out_fileDesc);
}

// Returns a file descriptor for a given file ID.
AKRESULT CAkUnrealIOHookDeferred::Open( 
    AkFileID        in_fileID,          // File ID.
    AkOpenMode      in_eOpenMode,       // Open mode.
    AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
	bool &			io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
    AkFileDesc &    out_fileDesc        // Returned file descriptor.
    )
{
	return PerformOpen(in_fileID, in_eOpenMode, in_pFlags, io_bSyncOpen, out_fileDesc);
}

int32 CAkUnrealIOHookDeferred::GetFreeTransferIndex()
{
	for(int32 i = 0; i < AK_UNREAL_MAX_CONCURRENT_IO; i++)
	{
		if( CAkUnrealIOHookDeferred::aPendingTransfers[i].Counter.GetValue() == -1 )
		{
			return i;
		}
	}

	return -1;
}

int32 CAkUnrealIOHookDeferred::FindTransfer(void* pBuffer)
{
	for(int32 i = 0; i < AK_UNREAL_MAX_CONCURRENT_IO; i++)
	{
		if( CAkUnrealIOHookDeferred::aPendingTransfers[i].AkTransferInfo.pBuffer == pBuffer )
		{
			return i;
		}
	}

	return -1;
}


//
// IAkIOHookDeferred implementation.
//-----------------------------------------------------------------------------

// Reads data from a file (asynchronous overload).
AKRESULT CAkUnrealIOHookDeferred::Read(
	AkFileDesc &			in_fileDesc,        // File descriptor.
	const AkIoHeuristics & in_heuristics,	// Heuristics for this data transfer (not used in this implementation).
	AkAsyncIOTransferInfo & io_transferInfo		// Asynchronous data transfer info.
	)
{
	// in_fileDesc.pCustomParam == 1 if opened in write mode
	if( in_fileDesc.pCustomParam && in_fileDesc.pCustomParam != (void*)1 )
	{
		// Register the global callback, if not already done
		if( !m_bCallbackRegistered && FAkAudioDevice::Get() )
		{
			if ( AK::SoundEngine::RegisterGlobalCallback( GlobalCallback ) != AK_Success )
			{
				AKASSERT( !"Failed registering to global callback" );
				return AK_Fail;
			}
			m_bCallbackRegistered = true;
		}

		FScopeLock ScopeLock( CriticalSection );
		int32 TransferIndex = GetFreeTransferIndex();
		AKASSERT( TransferIndex != -1 );

		CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].pCustomParam = in_fileDesc.pCustomParam;
		CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].AkTransferInfo = io_transferInfo;
		CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].Counter.Set(1);

		{
			FIOSystem & IO = FIOSystem::Get();
			CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].RequestIndex = IO.LoadData( 
				*((FString*)in_fileDesc.pCustomParam), 
				(int64) io_transferInfo.uFilePosition, 
				io_transferInfo.uRequestedSize, 
				io_transferInfo.pBuffer, 
				&CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].Counter, 
				AIOP_High
				);
		}
		if( CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].RequestIndex == 0 )
		{
			return AK_Fail;
		}

		return AK_Success;
	}
	else
	{
		// Opened in ReadWrite
#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
		return CAkDefaultIOHookDeferred::Read(in_fileDesc, in_heuristics, io_transferInfo);
#else
		return AK_Fail;
#endif
	}
}

// Cancel transfer(s).
void CAkUnrealIOHookDeferred::Cancel(
	AkFileDesc &			in_fileDesc,		// File descriptor.
	AkAsyncIOTransferInfo & io_transferInfo,// Transfer info to cancel (this implementation only handles "cancel all").
	bool & io_bCancelAllTransfersForThisFile	// Flag indicating whether all transfers should be cancelled for this file (see notes in function description).
	)
{
	// If there is no custom param, we did not handle this file.
	// in_fileDesc.pCustomParam == 1 if opened in write.
	// It was sent to the default IO Hook, so send the cancel request to the default hook.
	if( in_fileDesc.pCustomParam == NULL || in_fileDesc.pCustomParam == (void*)1)
	{
#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
		CAkDefaultIOHookDeferred::Cancel( in_fileDesc, io_transferInfo, io_bCancelAllTransfersForThisFile );
#else
		// Not implemented
#endif
	}
	else
	{
		FScopeLock ScopeLock( CriticalSection );
		FIOSystem & IO = FIOSystem::Get();
		int32 TransferIndex = FindTransfer(io_transferInfo.pBuffer);

		if( TransferIndex != -1 )
		{
			// Cancel the request. This decrements the thread-safe counter, so our global callback
			// will call the callback function automatically. The transfer will thus be handled correctly.
			IO.CancelRequests(&CAkUnrealIOHookDeferred::aPendingTransfers[TransferIndex].RequestIndex, 1);

			// We only cancelled one request, and not all of them.
			io_bCancelAllTransfersForThisFile = false;
		}
	}
}

// Close a file.
AKRESULT CAkUnrealIOHookDeferred::Close(
    AkFileDesc & in_fileDesc      // File descriptor.
    )
{
	// If there is no custom param, we did not handle this file.
	// in_fileDesc.pCustomParam == 1 if opened in write
	// It was sent to the default IO Hook, so send the close request to the default hook.
	if( in_fileDesc.pCustomParam == NULL || in_fileDesc.pCustomParam == (void*)1)
	{
#if !(PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
		return CAkDefaultIOHookDeferred::Close(in_fileDesc);
#else
		return AK_Fail;
#endif
	}
	else
	{
		{
			FIOSystem & IO = FIOSystem::Get();
			IO.HintDoneWithFile(*((FString*)in_fileDesc.pCustomParam));
		}

		delete (FString*)in_fileDesc.pCustomParam;
	}
	return AK_Success;
}

void CAkUnrealIOHookDeferred::GlobalCallback( bool in_bLastCall )
{
	// Loop through all our pending transfers to see if some are done.
	FScopeLock ScopeLock( g_lowLevelIO.CriticalSection );
	for (int32 i = 0; i < AK_UNREAL_MAX_CONCURRENT_IO; i++)
	{
		if( CAkUnrealIOHookDeferred::aPendingTransfers[i].Counter.GetValue() == 0 )
		{
			AKASSERT(CAkUnrealIOHookDeferred::aPendingTransfers[i].AkTransferInfo.pCallback);

			CAkUnrealIOHookDeferred::aPendingTransfers[i].AkTransferInfo.pCallback(&CAkUnrealIOHookDeferred::aPendingTransfers[i].AkTransferInfo, AK_Success);

			// Release the slot for this transfer. We are done for now.
			CAkUnrealIOHookDeferred::aPendingTransfers[i].Counter.Set(-1);
		}
	}

	if ( in_bLastCall )
	{
		g_lowLevelIO.m_bCallbackRegistered = false;
	}
}


void CAkUnrealIOHookDeferred::CleanFileDescriptor( AkFileDesc& out_fileDesc )
{
	out_fileDesc.uSector			= 0;
	out_fileDesc.deviceID			= m_deviceID;
	out_fileDesc.uCustomParamSize	= 0;	// not used.
	out_fileDesc.pCustomParam		= NULL;
	out_fileDesc.iFileSize			= 0;
}

AKRESULT CAkUnrealIOHookDeferred::FillFileDescriptorHelper(const AkOSChar* in_szFullFilePath, AkFileDesc& out_fileDesc )
{
	// Get a FString to pass to the UE IO module. FString has all necessary overloads for the types AkOSChar can be.
	FString* filePath = new FString(in_szFullFilePath);

	if( filePath )
	{
		out_fileDesc.iFileSize	= IFileManager::Get().FileSize( **filePath );
		if( out_fileDesc.iFileSize > 0 )
		{
			//out_fileDesc.uCustomParamSize	= 0; // Do not set (used by File packages).

			// We need the file name to pass UE4's IO module. Use the custom param to remember it.
			out_fileDesc.pCustomParam = (void*)filePath;
			return AK_Success;
		}

		delete filePath;
	}
	return AK_Fail;
}

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
// Returns the block size for the file or its storage device. 
AkUInt32 CAkUnrealIOHookDeferred::GetBlockSize(
    AkFileDesc &  /*in_fileDesc*/     // File descriptor.
    )
{
	// There is no limitation nor performance degradation with unaligned
	// seeking on any mount point with asynchronous cell fs API.
    return 1;
}

// Returns a description for the streaming device above this low-level hook.
void CAkUnrealIOHookDeferred::GetDeviceDesc(
    AkDeviceDesc & 
#ifndef AK_OPTIMIZED
	out_deviceDesc      // Description of associated low-level I/O device.
#endif
    )
{
#ifndef AK_OPTIMIZED
	// Deferred scheduler.
	out_deviceDesc.deviceID       = m_deviceID;
	out_deviceDesc.bCanRead       = true;
	out_deviceDesc.bCanWrite      = true;
	AK_CHAR_TO_UTF16( out_deviceDesc.szDeviceName, POSIX_DEFERRED_DEVICE_NAME, AK_MONITOR_DEVICENAME_MAXLENGTH );
	out_deviceDesc.uStringSize   = (AkUInt32)AKPLATFORM::AkUtf16StrLen( out_deviceDesc.szDeviceName ) + 1;
#endif
}

// Returns custom profiling data: 1 if file opens are asynchronous, 0 otherwise.
AkUInt32 CAkUnrealIOHookDeferred::GetDeviceData()
{
	return 0;
}

// Writes data to a file (asynchronous overload).
AKRESULT CAkUnrealIOHookDeferred::Write(
	AkFileDesc &			in_fileDesc,        // File descriptor.
	const AkIoHeuristics & /*in_heuristics*/,	// Heuristics for this data transfer (not used in this implementation).
	AkAsyncIOTransferInfo & io_transferInfo		// Platform-specific asynchronous IO operation info.
	)
{
	// Android asset manager cannot write date to a file inside apk.
	return AK_Fail;
}


#endif
