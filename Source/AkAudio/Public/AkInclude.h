// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkIncludes.h:
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#pragma once

// Currently, Wwise SDK builds with the default 8 byte alignment, whereas Unreal builds with 4 byte alignment under VC.
// This causes data corruption if the headers are not included with forced alignment directives.
// http://msdn.microsoft.com/en-us/library/xh3e3fd0%28VS.80%29.aspx
#if PLATFORM_WINDOWS
#pragma pack(push, 8)
#include "AllowWindowsPlatformTypes.h"
#elif PLATFORM_XBOXONE
#pragma pack(push, 8)
#include "XboxOneAllowPlatformTypes.h"
#endif // PLATFORM_WINDOWS || PLATFORM_XBOXONE

#include <AK/AkWwiseSDKVersion.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/IBytes.h>
#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/AkModule.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#ifndef AK_OPTIMIZED
    #include <AK/Comm/AkCommunication.h>
#endif // AK_OPTIMIZED

#if defined AK_SOUNDFRAME
	#include <AK/SoundFrame/SF.h>
	#include <AK/SoundEngine/Common/AkQueryParameters.h>
#endif

#include <AK/Plugin/AllPluginsFactories.h>

// Stream IO hooks
#include "AkDefaultIOHookDeferred.h"
#ifdef AK_USE_UNREAL_IO
#include "AkUnrealIOHookDeferred.h"
#endif

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#pragma pack(pop)
#elif  PLATFORM_XBOXONE
#include "XboxOneHidePlatformTypes.h"
#pragma pack(pop)
#endif // PLATFORM_WINDOWS || PLATFORM_XBOXONE
