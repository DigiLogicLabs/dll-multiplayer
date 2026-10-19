// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Session/DLLMultiplayerSessionLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "DLLMultiplayerSubsystem.h"
#include "OnlineSubsystem.h"
#include "Types/DLLMultiplayerSessionTypes.h"

namespace DLLMultiplayerSessionLimits
{
	/** Player counts a session can plausibly hold. Guards UI-sourced values. */
	static constexpr int32 MinPlayersAllowed = 1;
	static constexpr int32 MaxPlayersAllowed = 16;

	/** Search result bounds. The upper bound exists to stop a runaway LAN query. */
	static constexpr int32 MinSearchResults = 1;
	static constexpr int32 MaxSearchResults = 10000;
}

UDLLMultiplayerSubsystem* UDLLMultiplayerSessionLibrary::GetSessionSubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UDLLMultiplayerSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UDLLMultiplayerSubsystem>() : nullptr;
	if (Subsystem)
	{
		// Every library operation starts from the online subsystem's truth, not a stale cached state (2026-10-01).
		Subsystem->ReconcileSessionState();
	}
	return Subsystem;
}

bool UDLLMultiplayerSessionLibrary::IsSessionSystemAvailable(UObject* WorldContextObject)
{
	const UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	return Subsystem != nullptr && const_cast<UDLLMultiplayerSubsystem*>(Subsystem)->HasSessionInterface();
}

FString UDLLMultiplayerSessionLibrary::GetOnlineServiceName(UObject* WorldContextObject)
{
	if (UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject))
	{
		return Subsystem->GetServiceName();
	}
	return TEXT("None");
}

bool UDLLMultiplayerSessionLibrary::IsUsingLAN(UObject* WorldContextObject)
{
	if (UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject))
	{
		return Subsystem->IsUsingLAN();
	}
	// LAN needs no online service, so it is the mode most likely to work with nothing configured.
	return true;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::QuickHostSession(
	UObject* WorldContextObject, int32 MaxPlayers, const FString& MatchType, const FString& LobbyMapPath)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return EDLLMultiplayerSessionOperationResult::SubsystemUnavailable;
	}

	// Availability before occupancy: a project with no online support should say so, rather than
	// report the misleading "already in a session".
	if (Subsystem->IsInSession())
	{
		return EDLLMultiplayerSessionOperationResult::AlreadyInSession;
	}

	const int32 Clamped = FMath::Clamp(MaxPlayers, DLLMultiplayerSessionLimits::MinPlayersAllowed, DLLMultiplayerSessionLimits::MaxPlayersAllowed);

	// With a lobby map, host and travel there as a listen server once the session exists.
	if (!LobbyMapPath.IsEmpty())
	{
		const bool bStarted = Subsystem->HostSessionAndTravelToLevel(LobbyMapPath, Clamped, MatchType, /*bLAN*/ false, /*bPrivate*/ false);
		return bStarted ? EDLLMultiplayerSessionOperationResult::InProgress : EDLLMultiplayerSessionOperationResult::Failed;
	}

	Subsystem->CreateSession(Clamped, MatchType);

	// The operation was STARTED, not finished - callers wait on the subsystem's delegates.
	return EDLLMultiplayerSessionOperationResult::InProgress;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::QuickHostLANSession(
	UObject* WorldContextObject, int32 MaxPlayers, const FString& MatchType)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return EDLLMultiplayerSessionOperationResult::SubsystemUnavailable;
	}

	if (Subsystem->IsInSession())
	{
		return EDLLMultiplayerSessionOperationResult::AlreadyInSession;
	}

	const bool bStarted = Subsystem->CreateLANSession(
		FMath::Clamp(MaxPlayers, DLLMultiplayerSessionLimits::MinPlayersAllowed, DLLMultiplayerSessionLimits::MaxPlayersAllowed), MatchType);

	return bStarted ? EDLLMultiplayerSessionOperationResult::InProgress : EDLLMultiplayerSessionOperationResult::Failed;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::QuickHostPrivateSession(
	UObject* WorldContextObject, int32 MaxPlayers, const FString& MatchType)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return EDLLMultiplayerSessionOperationResult::SubsystemUnavailable;
	}

	if (Subsystem->IsInSession())
	{
		return EDLLMultiplayerSessionOperationResult::AlreadyInSession;
	}

	const bool bStarted = Subsystem->CreateQuickPrivateSession(
		FMath::Clamp(MaxPlayers, DLLMultiplayerSessionLimits::MinPlayersAllowed, DLLMultiplayerSessionLimits::MaxPlayersAllowed), MatchType);
	return bStarted ? EDLLMultiplayerSessionOperationResult::InProgress : EDLLMultiplayerSessionOperationResult::Failed;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::QuickFindSessions(
	UObject* WorldContextObject, int32 MaxResults, bool bSearchLAN)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return EDLLMultiplayerSessionOperationResult::SubsystemUnavailable;
	}

	const int32 ClampedResults = FMath::Clamp(MaxResults, DLLMultiplayerSessionLimits::MinSearchResults, DLLMultiplayerSessionLimits::MaxSearchResults);

	if (bSearchLAN)
	{
		FDLLMultiplayerSessionSearchFilter Filter;
		Filter.bIncludeLAN = true;
		Filter.MaxResults = ClampedResults;
		Subsystem->FindSessionsFiltered(Filter);
	}
	else
	{
		Subsystem->FindSessions(ClampedResults);
	}

	return EDLLMultiplayerSessionOperationResult::InProgress;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::QuickJoinSession(UObject* WorldContextObject, int32 SessionIndex)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return EDLLMultiplayerSessionOperationResult::SubsystemUnavailable;
	}

	// Refusals are BROADCAST as well as returned (2026-10-01): menus often ignore the return value, and a menu
	// that already put a loading screen up must hear that no join is coming.
	if (Subsystem->IsInSession())
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: QuickJoinSession(%d) refused - already in a session."), SessionIndex);
		Subsystem->OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::AlreadyInSession, FDLLMultiplayerSessionInfo());
		return EDLLMultiplayerSessionOperationResult::AlreadyInSession;
	}

	// Bounds-checked against the CACHED list, so a browser row left over from a previous search
	// reports SessionNotFound instead of indexing past the end.
	const TArray<FDLLMultiplayerSessionInfo> Sessions = Subsystem->GetCachedSessionList();
	if (!Sessions.IsValidIndex(SessionIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: QuickJoinSession(%d) refused - no such row in the last search (%d rows)."), SessionIndex, Sessions.Num());
		Subsystem->OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::SessionNotFound, FDLLMultiplayerSessionInfo());
		return EDLLMultiplayerSessionOperationResult::SessionNotFound;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: joining search row %d."), SessionIndex);

	Subsystem->JoinSessionByIndex(SessionIndex);
	return EDLLMultiplayerSessionOperationResult::InProgress;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::DirectJoinByAddress(
	UObject* WorldContextObject, const FString& Address, int32 DefaultPort)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World)
	{
		return EDLLMultiplayerSessionOperationResult::Failed;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(const_cast<UWorld*>(World), 0);
	if (!PlayerController)
	{
		return EDLLMultiplayerSessionOperationResult::Failed;
	}

	FString TargetAddress = Address.TrimStartAndEnd();
	if (TargetAddress.IsEmpty())
	{
		return EDLLMultiplayerSessionOperationResult::SessionNotFound;
	}

	if (!TargetAddress.Contains(TEXT(":")) && DefaultPort > 0)
	{
		TargetAddress = FString::Printf(TEXT("%s:%d"), *TargetAddress, DefaultPort);
	}

	UE_LOG(LogTemp, Log, TEXT("[DLLMultiplayer] Direct joining server at %s"), *TargetAddress);
	PlayerController->ClientTravel(TargetAddress, ETravelType::TRAVEL_Absolute);
	return EDLLMultiplayerSessionOperationResult::InProgress;
}

EDLLMultiplayerSessionOperationResult UDLLMultiplayerSessionLibrary::LeaveCurrentSession(UObject* WorldContextObject)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return EDLLMultiplayerSessionOperationResult::SubsystemUnavailable;
	}

	if (!Subsystem->IsInSession())
	{
		return EDLLMultiplayerSessionOperationResult::NotInSession;
	}

	Subsystem->DestroySession();
	return EDLLMultiplayerSessionOperationResult::InProgress;
}

bool UDLLMultiplayerSessionLibrary::IsInSession(UObject* WorldContextObject)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	return Subsystem && Subsystem->IsInSession();
}

bool UDLLMultiplayerSessionLibrary::IsSessionHost(UObject* WorldContextObject)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	return Subsystem && Subsystem->IsSessionHost();
}

FString UDLLMultiplayerSessionLibrary::GetCurrentSessionName(UObject* WorldContextObject)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (Subsystem && Subsystem->IsInSession())
	{
		return Subsystem->GetCurrentSessionInfo().SessionName;
	}
	return FString();
}

bool UDLLMultiplayerSessionLibrary::GetCurrentSessionPlayerCount(
	UObject* WorldContextObject, int32& OutCurrentPlayers, int32& OutMaxPlayers)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (Subsystem && Subsystem->IsInSession())
	{
		const FDLLMultiplayerSessionInfo Info = Subsystem->GetCurrentSessionInfo();
		OutCurrentPlayers = Info.CurrentPlayers;
		OutMaxPlayers = Info.MaxPlayers;
		return true;
	}

	OutCurrentPlayers = 0;
	OutMaxPlayers = 0;
	return false;
}

TArray<FDLLMultiplayerSessionEntry> UDLLMultiplayerSessionLibrary::GetSessionListForUI(UObject* WorldContextObject)
{
	TArray<FDLLMultiplayerSessionEntry> Result;

	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return Result;
	}

	const TArray<FDLLMultiplayerSessionInfo> CachedSessions = Subsystem->GetCachedSessionList();
	Result.Reserve(CachedSessions.Num());

	for (int32 i = 0; i < CachedSessions.Num(); ++i)
	{
		const FDLLMultiplayerSessionInfo& Info = CachedSessions[i];

		FDLLMultiplayerSessionEntry& Entry = Result.AddDefaulted_GetRef();
		Entry.SessionName = Info.SessionName;
		Entry.HostName = Info.HostName;
		Entry.CurrentPlayers = Info.CurrentPlayers;
		Entry.MaxPlayers = Info.MaxPlayers;
		Entry.PingMs = Info.PingMs;
		Entry.MatchType = Info.MatchType;
		Entry.MapName = Info.MapName;
		Entry.bIsLAN = Info.bIsLAN;

		// DERIVED, not trusted: a free slot alone is not enough - a session already underway that
		// forbids join-in-progress will refuse. Deriving it here stops browsers offering dead rows.
		Entry.bIsJoinable = (Info.CurrentPlayers < Info.MaxPlayers)
			&& (Info.bAllowJoinInProgress || Info.State == EDLLMultiplayerSessionState::Lobby);

		// Index into the cached list - the only stable way to refer back to this session.
		Entry.SessionIndex = i;
	}

	return Result;
}

int32 UDLLMultiplayerSessionLibrary::GetFoundSessionCount(UObject* WorldContextObject)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	return Subsystem ? Subsystem->GetCachedSessionList().Num() : 0;
}

bool UDLLMultiplayerSessionLibrary::IsSearchingForSessions(UObject* WorldContextObject)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	return Subsystem && Subsystem->IsSearchingForSessions();
}

FString UDLLMultiplayerSessionLibrary::GetOperationResultMessage(EDLLMultiplayerSessionOperationResult Result)
{
	switch (Result)
	{
		case EDLLMultiplayerSessionOperationResult::Success:              return TEXT("Operation completed successfully");
		case EDLLMultiplayerSessionOperationResult::InProgress:           return TEXT("Operation in progress...");
		case EDLLMultiplayerSessionOperationResult::Failed:               return TEXT("Operation failed");
		case EDLLMultiplayerSessionOperationResult::SessionNotFound:      return TEXT("Session not found");
		case EDLLMultiplayerSessionOperationResult::SessionFull:          return TEXT("Session is full");
		case EDLLMultiplayerSessionOperationResult::AlreadyInSession:     return TEXT("Already in a session");
		case EDLLMultiplayerSessionOperationResult::NotInSession:         return TEXT("Not currently in a session");
		case EDLLMultiplayerSessionOperationResult::SubsystemUnavailable: return TEXT("Multiplayer system unavailable");
		default:                                              return TEXT("Unknown error");
	}
}

FString UDLLMultiplayerSessionLibrary::FormatPingForDisplay(int32 PingMs)
{
	if (PingMs < 0)
	{
		return TEXT("N/A");
	}
	return FString::Printf(TEXT("%dms"), PingMs);
}

FLinearColor UDLLMultiplayerSessionLibrary::GetPingColor(int32 PingMs)
{
	if (PingMs < 0)   { return FLinearColor::Gray; }
	if (PingMs < 50)  { return FLinearColor::Green; }
	if (PingMs < 100) { return FLinearColor::Yellow; }
	if (PingMs < 150) { return FLinearColor(1.0f, 0.5f, 0.0f); }
	return FLinearColor::Red;
}

bool UDLLMultiplayerSessionLibrary::CanHostSession(UObject* WorldContextObject, FString& OutReason)
{
	UDLLMultiplayerSubsystem* Subsystem = GetSessionSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		OutReason = TEXT("Multiplayer system not available");
		return false;
	}

	if (!Subsystem->HasSessionInterface())
	{
		// LAN hosting needs no online service. Treating an invalid interface as fatal here would
		// block couch/LAN play any time Steam happens to be offline.
		if (Subsystem->IsUsingLAN())
		{
			OutReason.Reset();
			return true;
		}

		OutReason = TEXT("Online service not connected. Please ensure Steam is running.");
		return false;
	}

	if (Subsystem->IsInSession())
	{
		OutReason = TEXT("Already in a session. Leave current session first.");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool UDLLMultiplayerSessionLibrary::OpenSteamOverlay()
{
	if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
	{
		const IOnlineExternalUIPtr ExternalUI = OnlineSub->GetExternalUIInterface();
		if (ExternalUI.IsValid())
		{
			ExternalUI->ShowInviteUI(0);
			return true;
		}
	}
	return false;
}
