// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Party/DLLMultiplayerGameSession.h"

#include "DLLMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogDLLMultiplayerParty, Log, All);

static TAutoConsoleVariable<int32> CVarDLLMultiplayerPartyDebug(
	TEXT("dll.Multiplayer.PartyDebug"),
	0,
	TEXT("1 = print the split-screen party seat/reservation table on screen at every login and logout (server)."),
	ECVF_Cheat);

ADLLMultiplayerGameSession::ADLLMultiplayerGameSession()
{
	// The engine default. Stated so the rule "a machine brings at most this many seats" is visible here.
	MaxSplitscreensPerConnection = 4;
}

double ADLLMultiplayerGameSession::GetNowSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

void ADLLMultiplayerGameSession::PruneExpiredReservations()
{
	const double Now = GetNowSeconds();
	for (auto It = Reservations.CreateIterator(); It; ++It)
	{
		if (It.Value().Seats <= 0 || It.Value().ExpiresAtSeconds <= Now)
		{
			if (It.Value().Seats > 0)
			{
				UE_LOG(LogDLLMultiplayerParty, Warning, TEXT("Party %s: %d guest seat(s) released - guests never arrived."),
					*It.Key(), It.Value().Seats);
			}
			It.RemoveCurrent();
		}
	}
}

int32 ADLLMultiplayerGameSession::GetReservedSeatCount() const
{
	const double Now = GetNowSeconds();
	int32 Total = 0;
	for (const TPair<FString, FSeatReservation>& Pair : Reservations)
	{
		if (Pair.Value.ExpiresAtSeconds > Now)
		{
			Total += FMath::Max(0, Pair.Value.Seats);
		}
	}
	return Total;
}

int32 ADLLMultiplayerGameSession::CountPlayers(const APlayerController* ExcludedController) const
{
	const AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	if (!GameMode)
	{
		return 0;
	}

	int32 Count = const_cast<AGameModeBase*>(GameMode)->GetNumPlayers();
	if (ExcludedController && ExcludedController->PlayerState && !ExcludedController->PlayerState->IsOnlyASpectator())
	{
		Count = FMath::Max(0, Count - 1);
	}
	return Count;
}

int32 ADLLMultiplayerGameSession::GetFreeSeatCount() const
{
	return UDLLMultiplayerPartyRules::GetFreeSeats(MaxPlayers, CountPlayers(), GetReservedSeatCount());
}

FString ADLLMultiplayerGameSession::ApproveLogin(const FString& Options)
{
	// Spectators do not take a player seat; the base rules for them are unchanged.
	if (UGameplayStatics::GetIntOption(Options, TEXT("SpectatorOnly"), 0) == 1)
	{
		return Super::ApproveLogin(Options);
	}

	// The engine writes SplitscreenCount itself (1 for a machine's first login, 2+ for a guest) and
	// strips any client-supplied value, so it is the trustworthy guest signal here.
	const int32 SplitscreenCount = UGameplayStatics::GetIntOption(Options, TEXT("SplitscreenCount"), 0);
	if (SplitscreenCount > MaxSplitscreensPerConnection)
	{
		UE_LOG(LogDLLMultiplayerParty, Warning, TEXT("ApproveLogin: %d seats on one machine exceeds the limit of %d."),
			SplitscreenCount, MaxSplitscreensPerConnection);
		return UDLLMultiplayerPartyRules::ErrorTooManyLocal;
	}

	if (GetNetMode() == NM_Standalone)
	{
		return FString();
	}

	PruneExpiredReservations();

	FDLLMultiplayerLoginPartyInfo Info = UDLLMultiplayerPartyRules::ParseLoginOptions(Options);
	Info.bIsGuest = Info.bIsGuest || SplitscreenCount >= 2;

	// Password-protected sessions: a machine's first login carries ?Password= (added by the subsystem's join). Only
	// that login is checked: a split-screen guest's URL is rebuilt by the engine without it, and a guest can only
	// exist on a connection that already passed. The guest test is the engine-written SplitscreenCount, never the
	// client-supplied seat option, which anyone could set to skip the check.
	if (SplitscreenCount < 2)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UDLLMultiplayerSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UDLLMultiplayerSubsystem>() : nullptr;
		if (Sessions && !Sessions->IsSessionPasswordAccepted(UGameplayStatics::ParseOption(Options, TEXT("Password"))))
		{
			UE_LOG(LogDLLMultiplayerParty, Log, TEXT("Refused a login: wrong session password."));
			return UDLLMultiplayerSubsystem::ErrorWrongPassword;
		}
	}

	const FSeatReservation* PartyReservation = Info.PartyId.IsEmpty() ? nullptr : Reservations.Find(Info.PartyId);
	const int32 ReservedForParty = PartyReservation ? PartyReservation->Seats : 0;
	const int32 ReservedByOthers = GetReservedSeatCount() - ReservedForParty;
	const int32 Current = CountPlayers();

	const FString Error = UDLLMultiplayerPartyRules::EvaluateJoin(MaxPlayers, Current, ReservedByOthers, ReservedForParty, Info,
		MaxSplitscreensPerConnection);

	if (!Error.IsEmpty())
	{
		UE_LOG(LogDLLMultiplayerParty, Log, TEXT("Refused %s seat %d (party %s, size %d): %s  [players %d/%d, reserved %d]"),
			Info.bIsGuest ? TEXT("guest") : TEXT("primary"), Info.SeatIndex, *Info.PartyId, Info.PartySize, *Error,
			Current, MaxPlayers, ReservedByOthers + ReservedForParty);
		return Error;
	}

	// Hold the guests' seats now, at the only moment the whole party is known to fit.
	if (!Info.bIsGuest && Info.PartySize > 1 && !Info.PartyId.IsEmpty())
	{
		FSeatReservation& Reservation = Reservations.FindOrAdd(Info.PartyId);
		Reservation.Seats = Info.PartySize - 1;
		Reservation.ExpiresAtSeconds = GetNowSeconds() + PartyReservationSeconds;
		UE_LOG(LogDLLMultiplayerParty, Log, TEXT("Party %s admitted; holding %d guest seat(s) for %.0fs."),
			*Info.PartyId, Reservation.Seats, PartyReservationSeconds);
	}

	return FString();
}

void ADLLMultiplayerGameSession::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const FDLLMultiplayerLoginPartyInfo Info = UDLLMultiplayerPartyRules::GetPartyInfoForController(NewPlayer);

	if (Info.bIsGuest && !Info.PartyId.IsEmpty())
	{
		if (FSeatReservation* Reservation = Reservations.Find(Info.PartyId))
		{
			if (--Reservation->Seats <= 0)
			{
				Reservations.Remove(Info.PartyId);
			}
		}
	}

	OnPartyPlayerLoggedIn(NewPlayer, Info);
	RefreshAdvertisedHeadcount();
	DrawPartyDebug(TEXT("login"));
}

void ADLLMultiplayerGameSession::NotifyLogout(const APlayerController* PC)
{
	Super::NotifyLogout(PC);

	// A machine's first seat leaving takes its connection - and every guest - with it, so any seats
	// still held for that party would only block other players until they expired.
	const FDLLMultiplayerLoginPartyInfo Info = UDLLMultiplayerPartyRules::GetPartyInfoForController(PC);
	if (!Info.bIsGuest && !Info.PartyId.IsEmpty())
	{
		Reservations.Remove(Info.PartyId);
	}

	RefreshAdvertisedHeadcount(PC);
	DrawPartyDebug(TEXT("logout"));
}

void ADLLMultiplayerGameSession::UnregisterPlayer(const APlayerController* ExitingPlayer)
{
	if (ExitingPlayer && UDLLMultiplayerPartyRules::IsUniqueIdSharedWithAnotherPlayer(ExitingPlayer->PlayerState))
	{
		UE_LOG(LogDLLMultiplayerParty, Log,
			TEXT("%s left but shares its platform id with another seat on the same machine - kept in the session."),
			ExitingPlayer->PlayerState ? *ExitingPlayer->PlayerState->GetPlayerName() : TEXT("?"));
		return;
	}

	Super::UnregisterPlayer(ExitingPlayer);
}

void ADLLMultiplayerGameSession::RefreshAdvertisedHeadcount(const APlayerController* ExcludedController) const
{
	if (!bAdvertiseHeadcount || GetNetMode() == NM_Client || GetNetMode() == NM_Standalone)
	{
		return;
	}

	const IOnlineSessionPtr Sessions = Online::GetSessionInterface(GetWorld());
	FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(SessionName) : nullptr;
	if (!Session)
	{
		return; // A sessionless listen server (PIE, LAN without a session) has nothing to advertise.
	}

	const int32 Humans = CountPlayers(ExcludedController);
	int32 Previous = -1;
	int32 PreviousMax = -1;
	Session->SessionSettings.Get(UDLLMultiplayerPartyRules::SettingHumans, Previous);
	Session->SessionSettings.Get(UDLLMultiplayerPartyRules::SettingMaxHumans, PreviousMax);
	if (Previous == Humans && PreviousMax == MaxPlayers)
	{
		return; // Nothing changed; an UpdateSession is a platform round trip.
	}

	FOnlineSessionSettings Updated = Session->SessionSettings;
	Updated.Set(UDLLMultiplayerPartyRules::SettingHumans, Humans, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Updated.Set(UDLLMultiplayerPartyRules::SettingMaxHumans, MaxPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Sessions->UpdateSession(SessionName, Updated, true);
}

void ADLLMultiplayerGameSession::DrawPartyDebug(const TCHAR* Reason) const
{
	if (CVarDLLMultiplayerPartyDebug.GetValueOnGameThread() <= 0 || !GEngine)
	{
		return;
	}

	// Keyed messages so each refresh replaces the last; held long enough to read between events.
	const float Hold = 12.f;
	int32 Key = 9400;

	GEngine->AddOnScreenDebugMessage(Key++, Hold, FColor::Cyan,
		FString::Printf(TEXT("=== PARTY SEATS (%s) === players %d/%d  reserved %d  free %d"),
			Reason, CountPlayers(), MaxPlayers, GetReservedSeatCount(), GetFreeSeatCount()));

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC || !PC->PlayerState)
		{
			continue;
		}

		const FDLLMultiplayerLoginPartyInfo Info = UDLLMultiplayerPartyRules::GetPartyInfoForController(PC);
		GEngine->AddOnScreenDebugMessage(Key++, Hold, Info.bIsGuest ? FColor::Yellow : FColor::White,
			FString::Printf(TEXT("  %s  seat %d  party %s  platform id %s%s"),
				*PC->PlayerState->GetPlayerName(), Info.SeatIndex, *Info.PartyId.Left(8),
				PC->PlayerState->GetUniqueId().IsValid() ? TEXT("valid") : TEXT("none"),
				UDLLMultiplayerPartyRules::IsUniqueIdSharedWithAnotherPlayer(PC->PlayerState) ? TEXT("  (shared id)") : TEXT("")));
	}

	for (const TPair<FString, FSeatReservation>& Pair : Reservations)
	{
		GEngine->AddOnScreenDebugMessage(Key++, Hold, FColor::Orange,
			FString::Printf(TEXT("  reserved: party %s  %d seat(s)  %.0fs left"),
				*Pair.Key.Left(8), Pair.Value.Seats, FMath::Max(0.0, Pair.Value.ExpiresAtSeconds - GetNowSeconds())));
	}
}
