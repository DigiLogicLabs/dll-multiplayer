// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Party/DLLMultiplayerLocalPlayer.h"
#include "Party/DLLMultiplayerPartyRules.h"

#include "Engine/GameInstance.h"

int32 UDLLMultiplayerLocalPlayer::GetSeatIndex() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetLocalPlayers().Find(const_cast<UDLLMultiplayerLocalPlayer*>(this)) : INDEX_NONE;
}

FString UDLLMultiplayerLocalPlayer::GetMachinePartyId() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* FirstSeat = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;

	// Guests share the first seat's id. If the first seat is a plain ULocalPlayer the party is simply
	// keyed by whoever asks first - still one id per machine, since this object is that seat.
	const UDLLMultiplayerLocalPlayer* Owner = Cast<UDLLMultiplayerLocalPlayer>(FirstSeat);
	if (!Owner)
	{
		Owner = this;
	}

	if (!Owner->MachinePartyId.IsValid())
	{
		Owner->MachinePartyId = FGuid::NewGuid();
	}

	return Owner->MachinePartyId.ToString(EGuidFormats::Digits);
}

FString UDLLMultiplayerLocalPlayer::GetGameLoginOptions() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const int32 PartySize = GameInstance ? FMath::Max(1, GameInstance->GetNumLocalPlayers()) : 1;

	const FString PartyOptions = UDLLMultiplayerPartyRules::BuildLoginOptions(
		GetMachinePartyId(), PartySize, FMath::Max(0, GetSeatIndex()), GetLoginPlayerKey());

	const FString BaseOptions = Super::GetGameLoginOptions();
	return BaseOptions.IsEmpty() ? PartyOptions : (BaseOptions + TEXT("?") + PartyOptions);
}

FString UDLLMultiplayerLocalPlayer::GetNickname() const
{
	const FString Nickname = Super::GetNickname();
	const int32 Seat = GetSeatIndex();
	if (Seat <= 0)
	{
		return Nickname;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* FirstSeat = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	const FString OwnerNickname = FirstSeat ? FirstSeat->ULocalPlayer::GetNickname() : FString();

	// Only rename when the platform gave this seat nothing of its own. A platform with real per-seat
	// accounts (consoles) returns distinct names, and those are left alone.
	if (Nickname.IsEmpty() || Nickname == OwnerNickname)
	{
		return UDLLMultiplayerPartyRules::MakeGuestNickname(OwnerNickname, Seat);
	}

	return Nickname;
}
