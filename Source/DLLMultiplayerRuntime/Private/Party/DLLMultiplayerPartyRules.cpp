// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Party/DLLMultiplayerPartyRules.h"
#include "Party/DLLMultiplayerLocalPlayer.h"
#include "Types/DLLMultiplayerSessionTypes.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "DLLMultiplayerPartyRules"

const TCHAR* UDLLMultiplayerPartyRules::OptionPartyId = TEXT("MPParty");
const TCHAR* UDLLMultiplayerPartyRules::OptionPartySize = TEXT("MPPartySize");
const TCHAR* UDLLMultiplayerPartyRules::OptionSeat = TEXT("MPSeat");
const TCHAR* UDLLMultiplayerPartyRules::OptionPlayerKey = TEXT("MPKey");

const FName UDLLMultiplayerPartyRules::SettingHumans(TEXT("MP_HUMANS"));
const FName UDLLMultiplayerPartyRules::SettingMaxHumans(TEXT("MP_MAXHUMANS"));

const TCHAR* UDLLMultiplayerPartyRules::ErrorFull = TEXT("MPParty.Full");
const TCHAR* UDLLMultiplayerPartyRules::ErrorNoRoomPrefix = TEXT("MPParty.NoRoom");
const TCHAR* UDLLMultiplayerPartyRules::ErrorTooManyLocal = TEXT("MPParty.TooManyLocal");

FString UDLLMultiplayerPartyRules::BuildLoginOptions(const FString& PartyId, int32 PartySize, int32 SeatIndex, const FString& PlayerKey)
{
	// URL option values cannot carry '?', '=' or '#'. Guids and integers never do; the key is opaque
	// but guarded anyway so a game passing something odd cannot corrupt every option after it.
	auto Sanitise = [](const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("?"), TEXT(""));
		Out.ReplaceInline(TEXT("="), TEXT(""));
		Out.ReplaceInline(TEXT("#"), TEXT(""));
		return Out;
	};

	FString Options = FString::Printf(TEXT("%s=%s?%s=%d?%s=%d"),
		OptionPartyId, *Sanitise(PartyId),
		OptionPartySize, FMath::Max(1, PartySize),
		OptionSeat, FMath::Max(0, SeatIndex));

	if (!PlayerKey.IsEmpty())
	{
		Options += FString::Printf(TEXT("?%s=%s"), OptionPlayerKey, *Sanitise(PlayerKey));
	}

	return Options;
}

FDLLMultiplayerLoginPartyInfo UDLLMultiplayerPartyRules::ParseLoginOptions(const FString& Options)
{
	FDLLMultiplayerLoginPartyInfo Info;

	// ParseOption expects "?Key=Value" segments; tolerate an options string without the leading '?'.
	const FString Normalised = Options.StartsWith(TEXT("?")) ? Options : (TEXT("?") + Options);

	Info.PartyId = UGameplayStatics::ParseOption(Normalised, OptionPartyId);
	Info.PartySize = FMath::Max(1, UGameplayStatics::GetIntOption(Normalised, OptionPartySize, 1));
	Info.SeatIndex = FMath::Max(0, UGameplayStatics::GetIntOption(Normalised, OptionSeat, 0));
	Info.PlayerKey = UGameplayStatics::ParseOption(Normalised, OptionPlayerKey);
	Info.bIsGuest = Info.SeatIndex > 0;
	return Info;
}

int32 UDLLMultiplayerPartyRules::GetFreeSeats(int32 MaxPlayers, int32 CurrentPlayers, int32 ReservedSeats)
{
	return FMath::Max(0, MaxPlayers - FMath::Max(0, CurrentPlayers) - FMath::Max(0, ReservedSeats));
}

bool UDLLMultiplayerPartyRules::CanPartyFit(int32 PartySize, int32 MaxPlayers, int32 CurrentPlayers)
{
	return FMath::Max(1, PartySize) <= GetFreeSeats(MaxPlayers, CurrentPlayers, 0);
}

FString UDLLMultiplayerPartyRules::EvaluateJoin(int32 MaxPlayers, int32 CurrentPlayers, int32 ReservedByOthers, int32 ReservedForParty,
	const FDLLMultiplayerLoginPartyInfo& Info, int32 MaxSeatsPerMachine)
{
	if (Info.PartySize > FMath::Max(1, MaxSeatsPerMachine))
	{
		return ErrorTooManyLocal;
	}

	// A guest whose seat was held when its machine logged in walks straight in. This is the whole
	// point of the reservation: otherwise a third machine joining in the gap takes the seat and the
	// engine then kicks every player on the guest's machine.
	if (Info.bIsGuest && ReservedForParty > 0)
	{
		return FString();
	}

	const int32 Free = GetFreeSeats(MaxPlayers, CurrentPlayers, ReservedByOthers);

	// A guest with no reservation (added after its machine joined) needs one ordinary free seat.
	const int32 Needed = Info.bIsGuest ? 1 : FMath::Max(1, Info.PartySize);
	if (Needed <= Free)
	{
		return FString();
	}

	if (Free <= 0)
	{
		return ErrorFull;
	}

	return FString::Printf(TEXT("%s:%d:%d"), ErrorNoRoomPrefix, Needed, Free);
}

FString UDLLMultiplayerPartyRules::MakeGuestNickname(const FString& OwnerNickname, int32 SeatIndex)
{
	if (SeatIndex <= 0)
	{
		return OwnerNickname;
	}

	if (OwnerNickname.IsEmpty())
	{
		return FString::Printf(TEXT("Player %d"), SeatIndex + 1);
	}

	return FString::Printf(TEXT("%s (%d)"), *OwnerNickname, SeatIndex + 1);
}

FText UDLLMultiplayerPartyRules::DescribeJoinFailure(const FString& ErrorString)
{
	if (ErrorString.Contains(ErrorFull) || ErrorString.Contains(TEXT("Server full")))
	{
		return LOCTEXT("Full", "That game is full.");
	}

	if (ErrorString.Contains(ErrorTooManyLocal) || ErrorString.Contains(TEXT("Maximum splitscreen players")))
	{
		return LOCTEXT("TooManyLocal", "That game allows fewer players on one screen than you have.");
	}

	const int32 NoRoomAt = ErrorString.Find(ErrorNoRoomPrefix);
	if (NoRoomAt != INDEX_NONE)
	{
		TArray<FString> Parts;
		ErrorString.Mid(NoRoomAt).ParseIntoArray(Parts, TEXT(":"));
		const int32 Needed = Parts.IsValidIndex(1) ? FCString::Atoi(*Parts[1]) : 0;
		const int32 Free = Parts.IsValidIndex(2) ? FCString::Atoi(*Parts[2]) : 0;
		return FText::Format(
			LOCTEXT("NoRoom", "Not enough room for your party: you need {0} seats and that game has {1}."),
			FText::AsNumber(Needed), FText::AsNumber(Free));
	}

	return FText::FromString(ErrorString);
}

bool UDLLMultiplayerPartyRules::IsUniqueIdSharedWithAnotherPlayer(const APlayerState* PlayerState)
{
	if (!PlayerState || !PlayerState->GetUniqueId().IsValid())
	{
		return false;
	}

	const UWorld* World = PlayerState->GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return false;
	}

	for (const APlayerState* Other : GameState->PlayerArray)
	{
		if (Other && Other != PlayerState && IsValid(Other) && Other->GetUniqueId() == PlayerState->GetUniqueId())
		{
			return true;
		}
	}

	return false;
}

TArray<int32> UDLLMultiplayerPartyRules::RankQuickMatchCandidates(const TArray<FDLLMultiplayerSessionInfo>& Sessions, int32 PartySize, int32 MaxPing)
{
	TArray<int32> Candidates;
	for (int32 Index = 0; Index < Sessions.Num(); ++Index)
	{
		const FDLLMultiplayerSessionInfo& Session = Sessions[Index];
		if (!Session.IsValid() || Session.bHasPassword)
		{
			continue;
		}
		if (!CanPartyFit(PartySize, Session.MaxPlayers, Session.CurrentPlayers))
		{
			continue;
		}
		if (MaxPing > 0 && Session.PingMs > MaxPing)
		{
			continue;
		}
		Candidates.Add(Index);
	}

	Candidates.StableSort([&Sessions](int32 A, int32 B)
	{
		if (Sessions[A].CurrentPlayers != Sessions[B].CurrentPlayers)
		{
			return Sessions[A].CurrentPlayers > Sessions[B].CurrentPlayers;
		}
		return Sessions[A].PingMs < Sessions[B].PingMs;
	});
	return Candidates;
}

FDLLMultiplayerLoginPartyInfo UDLLMultiplayerPartyRules::GetPartyInfoForController(const APlayerController* PC)
{
	if (!PC)
	{
		return FDLLMultiplayerLoginPartyInfo();
	}

	// A remote seat: the server kept the URL it logged in with. Child connections (guests) carry
	// their own split-join URL, so each seat reads its own options.
	if (const UNetConnection* Connection = Cast<UNetConnection>(PC->Player))
	{
		FDLLMultiplayerLoginPartyInfo Info = ParseLoginOptions(Connection->RequestURL);
		// The engine's own flag outranks anything a client wrote: a child connection IS a guest.
		Info.bIsGuest = Info.bIsGuest || const_cast<UNetConnection*>(Connection)->GetUChildConnection() != nullptr;
		return Info;
	}

	// A seat on this machine (standalone, listen host, host's couch players): ask the local player.
	if (const UDLLMultiplayerLocalPlayer* LocalPlayer = Cast<UDLLMultiplayerLocalPlayer>(PC->Player))
	{
		return ParseLoginOptions(LocalPlayer->GetGameLoginOptions());
	}

	if (const ULocalPlayer* PlainLocalPlayer = Cast<ULocalPlayer>(PC->Player))
	{
		// Project not using UDLLMultiplayerLocalPlayer: still report the seat, which is all a local player needs.
		FDLLMultiplayerLoginPartyInfo Info;
		const UGameInstance* GameInstance = PlainLocalPlayer->GetGameInstance();
		Info.SeatIndex = GameInstance ? FMath::Max(0, GameInstance->GetLocalPlayers().Find(const_cast<ULocalPlayer*>(PlainLocalPlayer))) : 0;
		Info.PartySize = GameInstance ? FMath::Max(1, GameInstance->GetNumLocalPlayers()) : 1;
		Info.bIsGuest = Info.SeatIndex > 0;
		return Info;
	}

	return FDLLMultiplayerLoginPartyInfo();
}

#undef LOCTEXT_NAMESPACE
