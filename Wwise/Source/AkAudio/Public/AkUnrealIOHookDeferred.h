//////////////////////////////////////////////////////////////////////
//
// AkUnrealIOHookDeferred.h
//
// Implementation of the IO Hook redirecting 
// the Read requests into Unreal I/O system.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "AkInclude.h"

//-----------------------------------------------------------------------------
// Name: class CAkUnrealIOHookDeferred.
// Desc: Implements IAkIOHookDeferred low-level I/O hook, and 
//		 IAkFileLocationResolver. Can be used as a standalone Low-Level I/O
//		 system, or as part of a system with multiple devices.
//		 File location is resolved using simple path concatenation logic
//		 (implemented in CAkFileLocationBase).
//-----------------------------------------------------------------------------
#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
#include "../Common/AkFileLocationBase.h"

class CAkUnrealIOHookDeferred :	public AK::StreamMgr::IAkFileLocationResolver
								,public AK::StreamMgr::IAkIOHookDeferred
								,public CAkFileLocationBase
#else
class CAkUnrealIOHookDeferred : public CAkDefaultIOHookDeferred
#endif
{
public:

	/**
	 * Constructor.
	 */
	CAkUnrealIOHookDeferred();

	/**
	 * Destructor.
	 */
	virtual ~CAkUnrealIOHookDeferred();

	/** 
	 * Initialization. Init() registers this object as the one and 
	 * only File Location Resolver if none were registered before. Then 
	 * it creates a streaming device with scheduler type AK_SCHEDULER_DEFERRED_LINED_UP.
	 *
	 * @param in_deviceSettings		Device settings.
	 * @param in_bAsyncOpen			If true, files are opened asynchronously when possible.
	 * @return	AK_Success if initialization was successful, error code otherwise
	 */
	AKRESULT Init(
		const AkDeviceSettings &	in_deviceSettings,	
		bool						in_bAsyncOpen=false
		);

	/**
	 * Terminate.
	 */
	void Term();

	//
	// IAkFileLocationAware interface.
	//-----------------------------------------------------------------------------

    /**
	 * Returns a file descriptor for a given file name (string).
	 *
	 * @param in_pszFileName	File name.
	 * @param in_eOpenMode		Open mode.
	 * @param in_pFlags			Special flags. Can pass NULL.
	 * @param io_bSyncOpen		If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
	 * @param out_fileDesc		Returned file descriptor.
	 * @return	AK_Success if operation was successful, error code otherwise
	 */
    virtual AKRESULT Open( 
        const AkOSChar*			in_pszFileName,		
		AkOpenMode				in_eOpenMode,		
        AkFileSystemFlags *		in_pFlags,			
		bool &					io_bSyncOpen,		
        AkFileDesc &			out_fileDesc        
        );

    /**
	 * Returns a file descriptor for a given file ID.
	 *
	 * @param in_fileID			File ID.
	 * @param in_eOpenMode		Open mode.
	 * @param in_pFlags			Special flags. Can pass NULL.
	 * @param io_bSyncOpen		If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
	 * @param out_fileDesc		Returned file descriptor.
	 * @return	AK_Success if operation was successful, error code otherwise
	 */
    virtual AKRESULT Open( 
        AkFileID				in_fileID,          // File ID.
        AkOpenMode				in_eOpenMode,       // Open mode.
        AkFileSystemFlags *		in_pFlags,			// Special flags. Can pass NULL.
		bool &					io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
        AkFileDesc &			out_fileDesc        // Returned file descriptor.
        );


	//
	// IAkIOHookDeferred interface.
	//-----------------------------------------------------------------------------

    /**
	 * Reads data from a file (asynchronous).
	 *
	 * @param in_fileDesc		File descriptor.
	 * @param in_heuristics		Heuristics for this data transfer.
	 * @param io_transferInfo	Asynchronous data transfer info.
	 * @return	AK_Success if operation was successful, error code otherwise
	 */
    virtual AKRESULT Read(
		AkFileDesc &			in_fileDesc,
		const AkIoHeuristics &	in_heuristics,
		AkAsyncIOTransferInfo & io_transferInfo
		);

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
    // Writes data to a file (asynchronous).
    virtual AKRESULT Write(
		AkFileDesc &			in_fileDesc,        // File descriptor.
		const AkIoHeuristics &	in_heuristics,		// Heuristics for this data transfer.
		AkAsyncIOTransferInfo & io_transferInfo		// Platform-specific asynchronous IO operation info.
		);
#endif
    /**
	 * Notifies that a transfer request is cancelled. It will be flushed by the streaming device when completed.
	 *
	 * @param in_fileDesc							File descriptor.
	 * @param io_transferInfo						Transfer info to cancel.
	 * @param io_bCancelAllTransfersForThisFile		Flag indicating whether all transfers should be cancelled for this file (see notes in function description).
	 */
    virtual void Cancel(
		AkFileDesc &			in_fileDesc,
		AkAsyncIOTransferInfo & io_transferInfo,
		bool & io_bCancelAllTransfersForThisFile
		);

	/**
	 * Cleans up a file.
	 *
	 * @param in_fileDesc		File descriptor.
	 * @return	AK_Success if operation was successful, error code otherwise
	 */
	virtual AKRESULT Close(
        AkFileDesc &			in_fileDesc
        );

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
    // Returns the block size for the file or its storage device. 
    virtual AkUInt32 GetBlockSize(
        AkFileDesc &  			in_fileDesc			// File descriptor.
        );

	// Returns a description for the streaming device above this low-level hook.
    virtual void GetDeviceDesc(
        AkDeviceDesc &  		out_deviceDesc      // Description of associated low-level I/O device.
        );
	
	// Returns custom profiling data: 1 if file opens are asynchronous, 0 otherwise.
	virtual AkUInt32 GetDeviceData();
#endif


	struct AkDeferredIOInfo
	{
		AkAsyncIOTransferInfo AkTransferInfo;
		void* pCustomParam;
		uint64 RequestIndex;
		FThreadSafeCounter Counter;
	};

protected:
	/**
	 * Global callback to Sound Engine.
	 */
	static void GlobalCallback(
		bool in_bLastCall
		);

	/** True if global callback was registered to the sound engine. */
	bool		m_bCallbackRegistered;
	
	/** Get a free index in the pending transfer arrays 
	 *
	 * @return An index pointer to a free entry. -1 if no free entry. Not thread-safe.
	 */
	int32 GetFreeTransferIndex();

	/**
	 * Find a transfer, using the custom param pointer as key, in the pending transfer arrays
	 * @param pCustomParam	The custom param pointer for the file we are searching
	 * @return The index of the transfer. -1 if not found. Not thread-safe.
	 */
	int32 FindTransfer(void* pBuffer);

	/** Array for pending transfers. */
	static AkDeferredIOInfo aPendingTransfers[AK_UNREAL_MAX_CONCURRENT_IO];

	/** Critical section used to syncronize access to pending transfers array */
	FCriticalSection* CriticalSection;


private:
	/** 
	 * Reset the file descriptor
	 *
	 * @param out_fileDesc The file descriptor
	 */
	void CleanFileDescriptor( AkFileDesc& out_fileDesc );

	/** 
	 * Fill the file descriptor
	 *
	 * @param in_szFullFilePath The path of the file
	 * @param out_fileDesc The file descriptor
	 */
	AKRESULT FillFileDescriptorHelper(const AkOSChar* in_szFullFilePath, AkFileDesc& out_fileDesc );

	/**
	 * Perform the open, whether we have a file ID or a file Name
	 *
	 * @param in_fileDescriptor		file ID or file Name
	 * @param in_eOpenMode			Open mode.
	 * @param in_pFlags				Special flags. Can pass NULL.
	 * @param io_bSyncOpen			If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
	 * @param out_fileDesc			Returned file descriptor.
	 * @return AK_Success if operation was successful, error code otherwise
	 */
	template<typename T>
	AKRESULT PerformOpen( 
		T				in_fileDescriptor,  // Can be file ID or file Name
		AkOpenMode      in_eOpenMode,       // Open mode.
		AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
		bool &			io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
		AkFileDesc &    out_fileDesc        // Returned file descriptor.
		);

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
	AkDeviceID			m_deviceID;
	bool				m_bAsyncOpen;	// If true, opens files asynchronously when it can.
#endif

};
