// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Party/DLLMultiplayerQuickMatchSubsystem.h"

#include "DLLMultiplayerSubsystem.h"
#include "Party/DLLMultiplayerPartyRules.h"

#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "MPQuickMatch"

DEFINE_LOG_CATEGORY_STATIC(LogDLLMultiplayerQuickMatch, Log, All);

UDLLMultiplayerSubsystem* UDLLMultiplayerQuickMatchSubsystem::GetSessions() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UDLLMultiplayerSubsystem>() : nullptr;
}

void UDLLMultiplayerQuickMatchSubsystem::Deinitialize()
{
	UnbindAll();
	Super::Deinitialize();
}

void UDLLMultiplayerQuickMatchSubsystem::SetState(EDLLMultiplayerQuickMatchState NewState, const FText& Message)
{
	State = NewState;
	UE_LOG(LogDLLMultiplayerQuickMatch, Log, TEXT("[QuickMatch] %s"), *Message.ToString());
	OnQuickMatchStateChanged.Broadcast(NewState, Message);
}

void UDLLMultiplayerQuickMatchSubsystem::UnbindAll()
{
	if (UDLLMultiplayerSubsystem* Sessions = GetSessions())
	{
		Sessions->OnSessionsFound.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionsFound);
		Sessions->OnSessionJoined.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionJoined);
		Sessions->OnSessionCreated.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleHostCreated);
	}
}

bool UDLLMultiplayerQuickMatchSubsystem::StartQuickMatch(const FDLLMultiplayerQuickMatchRequest& Request)
{
	UDLLMultiplayerSubsystem* Sessions = GetSessions();
	if (IsRunning())
	{
		return false; // not a failure of the running match - just refuse the duplicate press
	}
	if (!Sessions)
	{
		SetState(EDLLMultiplayerQuickMatchState::Failed, LOCTEXT("NoSessions", "Online play is not available."));
		return false;
	}
	if (Sessions->IsInSession())
	{
		SetState(EDLLMultiplayerQuickMatchState::Failed, LOCTEXT("InSession", "Leave your current game before Quick Match."));
		return false;
	}

	ActiveRequest = Request;
	const UGameInstance* GameInstance = GetGameInstance();
	ActivePartySize = Request.PartySize > 0 ? Request.PartySize : FMath::Max(1, GameInstance ? GameInstance->GetNumLocalPlayers() : 1);
	Candidates.Reset();
	NextCandidate = 0;
	JoinAttempts = 0;

	UnbindAll();
	Sessions->OnSessionsFound.AddDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionsFound);

	FDLLMultiplayerSessionSearchFilter Filter;
	Filter.MaxResults = Request.MaxSearchResults;
	Filter.MatchType = Request.MatchType;
	Filter.bIncludeLAN = Request.bLAN;
	Filter.bOnlyShowAvailable = false; // room is judged for the whole PARTY in RankQuickMatchCandidates
	Filter.bOnlyShowJoinable = true;

	SetState(EDLLMultiplayerQuickMatchState::Searching, FText::Format(
		LOCTEXT("Searching", "Searching for a game with room for {0}..."), FText::AsNumber(ActivePartySize)));
	Sessions->FindSessionsFiltered(Filter);
	return true;
}

void UDLLMultiplayerQuickMatchSubsystem::CancelQuickMatch()
{
	if (!IsRunning())
	{
		return;
	}
	UnbindAll();
	SetState(EDLLMultiplayerQuickMatchState::Cancelled, LOCTEXT("Cancelled", "Quick Match cancelled."));
}

void UDLLMultiplayerQuickMatchSubsystem::HandleSessionsFound(const TArray<FDLLMultiplayerSessionInfo>& Sessions, bool bWasSuccessful)
{
	if (State != EDLLMultiplayerQuickMatchState::Searching)
	{
		return;
	}
	if (UDLLMultiplayerSubsystem* SessionSubsystem = GetSessions())
	{
		SessionSubsystem->OnSessionsFound.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionsFound);
	}

	for (const int32 Index : UDLLMultiplayerPartyRules::RankQuickMatchCandidates(Sessions, ActivePartySize, ActiveRequest.MaxPing))
	{
		Candidates.Add(Sessions[Index]);
	}

	UE_LOG(LogDLLMultiplayerQuickMatch, Log, TEXT("[QuickMatch] search %s: %d game(s) found, %d fit a party of %d."),
		bWasSuccessful ? TEXT("ok") : TEXT("FAILED"), Sessions.Num(), Candidates.Num(), ActivePartySize);

	if (Candidates.Num() == 0)
	{
		HostOrFail(bWasSuccessful ? LOCTEXT("NoneFit", "No game has room for your party.")
			: LOCTEXT("SearchFailed", "The game search failed."));
		return;
	}
	TryNextCandidate();
}

void UDLLMultiplayerQuickMatchSubsystem::TryNextCandidate()
{
	UDLLMultiplayerSubsystem* Sessions = GetSessions();
	if (!Sessions || NextCandidate >= Candidates.Num() || JoinAttempts >= ActiveRequest.MaxJoinAttempts)
	{
		HostOrFail(LOCTEXT("JoinsFailed", "Could not join any of the games found."));
		return;
	}

	const FDLLMultiplayerSessionInfo& Candidate = Candidates[NextCandidate++];
	++JoinAttempts;

	Sessions->OnSessionJoined.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionJoined);
	Sessions->OnSessionJoined.AddDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionJoined);

	SetState(EDLLMultiplayerQuickMatchState::Joining, FText::Format(LOCTEXT("Joining", "Joining {0} ({1}/{2})..."),
		FText::FromString(Candidate.SessionName.IsEmpty() ? Candidate.HostName : Candidate.SessionName),
		FText::AsNumber(Candidate.CurrentPlayers), FText::AsNumber(Candidate.MaxPlayers)));
	Sessions->JoinSessionByInfo(Candidate);
}

void UDLLMultiplayerQuickMatchSubsystem::HandleSessionJoined(EDLLMultiplayerJoinSessionResult Result, const FDLLMultiplayerSessionInfo& SessionInfo)
{
	if (State != EDLLMultiplayerQuickMatchState::Joining)
	{
		return;
	}
	if (UDLLMultiplayerSubsystem* Sessions = GetSessions())
	{
		Sessions->OnSessionJoined.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleSessionJoined);
	}

	if (Result == EDLLMultiplayerJoinSessionResult::Success)
	{
		// The session layer is travelling the client now. If the host refuses the party at login (it filled up
		// in between), the engine's network failure carries ADLLMultiplayerGameSession's reason back to the menu.
		SetState(EDLLMultiplayerQuickMatchState::Succeeded, LOCTEXT("Joined", "Game found - joining."));
		return;
	}

	UE_LOG(LogDLLMultiplayerQuickMatch, Log, TEXT("[QuickMatch] join attempt %d failed (%d); trying the next game."), JoinAttempts, static_cast<int32>(Result));
	TryNextCandidate();
}

void UDLLMultiplayerQuickMatchSubsystem::HostOrFail(const FText& Reason)
{
	UDLLMultiplayerSubsystem* Sessions = GetSessions();
	if (!ActiveRequest.bHostIfNoneFound || ActiveRequest.HostLevelPath.IsEmpty() || !Sessions)
	{
		UnbindAll();
		SetState(EDLLMultiplayerQuickMatchState::Failed, Reason);
		return;
	}

	Sessions->OnSessionCreated.RemoveDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleHostCreated);
	Sessions->OnSessionCreated.AddDynamic(this, &UDLLMultiplayerQuickMatchSubsystem::HandleHostCreated);

	SetState(EDLLMultiplayerQuickMatchState::Hosting, FText::Format(LOCTEXT("Hosting", "{0} Hosting a new game..."), Reason));
	if (!Sessions->HostSessionAndTravelToLevel(ActiveRequest.HostLevelPath, ActiveRequest.HostMaxConnections,
		ActiveRequest.MatchType, ActiveRequest.bLAN, false))
	{
		UnbindAll();
		SetState(EDLLMultiplayerQuickMatchState::Failed, LOCTEXT("HostFailed", "Could not host a game."));
	}
}

void UDLLMultiplayerQuickMatchSubsystem::HandleHostCreated(bool bWasSuccessful, const FDLLMultiplayerSessionInfo& SessionInfo)
{
	if (State != EDLLMultiplayerQuickMatchState::Hosting)
	{
		return;
	}
	UnbindAll();
	SetState(bWasSuccessful ? EDLLMultiplayerQuickMatchState::Succeeded : EDLLMultiplayerQuickMatchState::Failed,
		bWasSuccessful ? LOCTEXT("Hosted", "Hosting - other parties can Quick Match into your game.")
			: LOCTEXT("HostFailed2", "Could not host a game."));
}

#undef LOCTEXT_NAMESPACE
