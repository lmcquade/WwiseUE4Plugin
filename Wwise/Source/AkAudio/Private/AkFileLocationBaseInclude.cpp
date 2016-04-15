// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

// @todo:ak verify consistency of Wwise SDK alignment settings with unreal build settings on all platforms
//			Currently, Wwise SDK builds with the default alignment (8-bytes as per MSDN) with the VC toolchain.
//			Unreal builds with 4-byte alignment on the VC toolchain on both Win32 and Win64. 
//			This could (and currently does, on Win64) cause data corruption if the headers are not included with forced alignment directives as below.
#include "AkAudioDevice.h"

#if PLATFORM_WINDOWS
#pragma pack(push, 8)
#include "AllowWindowsPlatformTypes.h"
#elif PLATFORM_XBOXONE
#pragma pack(push, 8)
#include "XboxOneAllowPlatformTypes.h"
#endif // defined(WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(XBOX)

#include "AkFileLocationBase.cpp"

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#pragma pack(pop)
#elif PLATFORM_XBOXONE
#include "XboxOneHidePlatformTypes.h"
#pragma pack(pop)
#endif // defined(WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(XBOX)
