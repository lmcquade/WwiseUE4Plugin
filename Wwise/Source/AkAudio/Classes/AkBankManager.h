// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAudioDevice.h: Audiokinetic audio interface object.
=============================================================================*/

#pragma once

/*------------------------------------------------------------------------------------
	AkAudioDevice system headers
------------------------------------------------------------------------------------*/

#include "Engine.h"

#include "AkInclude.h"
#include "SoundDefinitions.h"

/*------------------------------------------------------------------------------------
	Audiokinetic SoundBank Manager.
------------------------------------------------------------------------------------*/
class AKAUDIO_API FAkBankManager
{
public:
	struct AkBankCallbackInfo
	{
		AkBankCallbackFunc	CallbackFunc;
		class UAkAudioBank *		pBank;

		AkBankCallbackInfo(AkBankCallbackFunc cbFunc, class UAkAudioBank * bank)
			: CallbackFunc(cbFunc)
			, pBank(bank)
		{}
	};

	void AddBankLoadCallbackInfo(void * in_pCookie, AkBankCallbackInfo& in_CallbackInfo)
	{
		m_BankLoadCallbackMap.Add(in_pCookie, in_CallbackInfo);
	}

	AkBankCallbackInfo * GetBankLoadCallbackInfo(void * in_pCookie)
	{
		return m_BankLoadCallbackMap.Find(in_pCookie);
	}

	void RemoveBankLoadCallbackInfo(void * in_pCookie)
	{
		m_BankLoadCallbackMap.Remove(in_pCookie);
	}

	void AddBankUnloadCallbackInfo(void * in_pCookie, AkBankCallbackInfo& in_CallbackInfo)
	{
		m_BankUnloadCallbackMap.Add(in_pCookie, in_CallbackInfo);
	}

	AkBankCallbackInfo * GetBankUnloadCallbackInfo(void * in_pCookie)
	{
		return m_BankUnloadCallbackMap.Find(in_pCookie);
	}

	void RemoveBankUnloadCallbackInfo(void * in_pCookie)
	{
		m_BankUnloadCallbackMap.Remove(in_pCookie);
	}

	void AddLoadedBank(class UAkAudioBank * Bank)
	{
		bool bIsAlreadyInSet = false;
		m_LoadedBanks.Add(Bank, &bIsAlreadyInSet);
		check(bIsAlreadyInSet == false);
	}

	void RemoveLoadedBank(class UAkAudioBank * Bank)
	{
		m_LoadedBanks.Remove(Bank);
	}

	void ClearLoadedBanks()
	{
		m_LoadedBanks.Empty();
	}

	const TSet<class UAkAudioBank *>* GetLoadedBankList()
	{
		return &m_LoadedBanks;
	}

	FCriticalSection m_BankManagerCriticalSection;

private:

	TSet< class UAkAudioBank * > m_LoadedBanks;

	TMap< void*, AkBankCallbackInfo > m_BankLoadCallbackMap;
	TMap< void*, AkBankCallbackInfo > m_BankUnloadCallbackMap;

};