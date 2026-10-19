// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Types/DLLMultiplayerSessionTypes.h"
#include "OnlineSessionSettings.h"
#include "Party/DLLMultiplayerPartyRules.h"

FDLLMultiplayerSessionInfo FDLLMultiplayerSessionInfo::FromSearchResult(const FOnlineSessionSearchResult& Result)
{
	FDLLMultiplayerSessionInfo Info;

	if (!Result.IsValid())
	{
		return Info;
	}

	const FOnlineSession& Session = Result.Session;
	const FOnlineSessionSettings& Settings = Session.SessionSettings;

	// Basic info
	Info.SearchResult = Result;
	Info.MaxPlayers = Settings.NumPublicConnections + Settings.NumPrivateConnections;
	Info.CurrentPlayers = Info.MaxPlayers - Session.NumOpenPublicConnections - Session.NumOpenPrivateConnections;
	Info.bIsLAN = Settings.bIsLANMatch;
	Info.bIsDedicatedServer = Settings.bIsDedicated;
	Info.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
	Info.BuildUniqueId = Settings.BuildUniqueId;
	Info.PingMs = Result.PingInMs;

	// Headcount in PEOPLE when the host advertises it (ADLLMultiplayerGameSession). The connection counts above are
	// per machine on platforms that dedupe by account, so a 4-player couch party would read as "1".
	int32 AdvertisedHumans = 0;
	int32 AdvertisedMaxHumans = 0;
	if (Settings.Get(UDLLMultiplayerPartyRules::SettingHumans, AdvertisedHumans) && Settings.Get(UDLLMultiplayerPartyRules::SettingMaxHumans, AdvertisedMaxHumans)
		&& AdvertisedMaxHumans > 0)
	{
		Info.MaxPlayers = AdvertisedMaxHumans;
		Info.CurrentPlayers = FMath::Clamp(AdvertisedHumans, 0, AdvertisedMaxHumans);
	}

	// Get owning user name as host name
	if (Session.OwningUserName.Len() > 0)
	{
		Info.HostName = Session.OwningUserName;
		Info.SessionName = Session.OwningUserName + TEXT("'s Game");
	}

	// Extract custom settings
	FString MatchTypeValue;
	if (Settings.Get(FName("MatchType"), MatchTypeValue))
	{
		Info.MatchType = MatchTypeValue;
	}

	FString MapValue;
	if (Settings.Get(FName("MapName"), MapValue))
	{
		Info.MapName = MapValue;
	}

	FString GameModeValue;
	if (Settings.Get(FName("GameMode"), GameModeValue))
	{
		Info.GameMode = GameModeValue;
	}

	FString SessionTypeValue;
	if (Settings.Get(FName("SessionType"), SessionTypeValue))
	{
		int32 TypeInt = FCString::Atoi(*SessionTypeValue);
		Info.SessionType = static_cast<EDLLMultiplayerSessionType>(FMath::Clamp(TypeInt, 0, static_cast<int32>(EDLLMultiplayerSessionType::Custom)));
	}

	FString VisibilityValue;
	if (Settings.Get(FName("Visibility"), VisibilityValue))
	{
		int32 VisInt = FCString::Atoi(*VisibilityValue);
		Info.Visibility = static_cast<EDLLMultiplayerSessionVisibility>(FMath::Clamp(VisInt, 0, static_cast<int32>(EDLLMultiplayerSessionVisibility::LAN)));
	}

	FString HasPasswordValue;
	if (Settings.Get(FName("HasPassword"), HasPasswordValue))
	{
		Info.bHasPassword = HasPasswordValue.ToBool();
	}

	FString CustomNameValue;
	if (Settings.Get(FName("SessionName"), CustomNameValue))
	{
		Info.SessionName = CustomNameValue;
	}

	// Determine state based on session info
	// Note: FOnlineSession no longer has SessionState in UE 5.5+
	// For search results, we infer state from available settings
	FString SessionStateValue;
	if (Settings.Get(FName("SessionState"), SessionStateValue))
	{
		int32 StateInt = FCString::Atoi(*SessionStateValue);
		Info.State = static_cast<EDLLMultiplayerSessionState>(FMath::Clamp(StateInt, 0, static_cast<int32>(EDLLMultiplayerSessionState::Destroying)));
	}
	else
	{
		// Default: if session allows join in progress and has players, it might be in progress
		// Otherwise assume lobby/pending state
		if (Info.bAllowJoinInProgress && Info.CurrentPlayers > 0)
		{
			// Could be in progress, mark as joinable
			Info.State = EDLLMultiplayerSessionState::InProgress;
		}
		else
		{
			// Default to lobby state for sessions we can join
			Info.State = EDLLMultiplayerSessionState::Lobby;
		}
	}

	return Info;
}

bool FDLLMultiplayerSessionSearchFilter::Matches(const FDLLMultiplayerSessionInfo& SessionInfo) const
{
	// Check session type filter
	if (SessionTypes.Num() > 0 && !SessionTypes.Contains(SessionInfo.SessionType))
	{
		return false;
	}

	// Check match type filter
	if (!MatchType.IsEmpty() && SessionInfo.MatchType != MatchType)
	{
		return false;
	}

	// Check map filter
	if (!MapName.IsEmpty() && !SessionInfo.MapName.Contains(MapName))
	{
		return false;
	}

	// Check game mode filter
	if (!GameMode.IsEmpty() && !SessionInfo.GameMode.Contains(GameMode))
	{
		return false;
	}

	// Check availability
	if (bOnlyShowAvailable && SessionInfo.CurrentPlayers >= SessionInfo.MaxPlayers)
	{
		return false;
	}

	// Check joinability
	if (bOnlyShowJoinable)
	{
		if (!SessionInfo.bAllowJoinInProgress && SessionInfo.State == EDLLMultiplayerSessionState::InProgress)
		{
			return false;
		}
	}

	// Check ping
	if (MaxPing > 0 && SessionInfo.PingMs > MaxPing)
	{
		return false;
	}

	// Check player count filters
	if (MinPlayers > 0 && SessionInfo.CurrentPlayers < MinPlayers)
	{
		return false;
	}

	if (MaxPlayers > 0 && SessionInfo.MaxPlayers > MaxPlayers)
	{
		return false;
	}

	return true;
}

// UDLLMultiplayerSessionTypeHelpers implementation

FString UDLLMultiplayerSessionTypeHelpers::GetSessionTypeDisplayName(EDLLMultiplayerSessionType SessionType)
{
	switch (SessionType)
	{
		case EDLLMultiplayerSessionType::CampaignCoop:		return TEXT("Campaign Co-Op");
		case EDLLMultiplayerSessionType::CompetitiveFFA:	return TEXT("Free For All");
		case EDLLMultiplayerSessionType::CompetitiveTeam:	return TEXT("Team Match");
		case EDLLMultiplayerSessionType::Horde:				return TEXT("Horde/Survival");
		case EDLLMultiplayerSessionType::Private:			return TEXT("Private Match");
		case EDLLMultiplayerSessionType::LAN:				return TEXT("LAN Game");
		case EDLLMultiplayerSessionType::Custom:			return TEXT("Custom Game");
		default:								return TEXT("Unknown");
	}
}

FString UDLLMultiplayerSessionTypeHelpers::GetVisibilityDisplayName(EDLLMultiplayerSessionVisibility Visibility)
{
	switch (Visibility)
	{
		case EDLLMultiplayerSessionVisibility::Public:		return TEXT("Public");
		case EDLLMultiplayerSessionVisibility::FriendsOnly:	return TEXT("Friends Only");
		case EDLLMultiplayerSessionVisibility::Private:		return TEXT("Invite Only");
		case EDLLMultiplayerSessionVisibility::LAN:			return TEXT("LAN Only");
		default:								return TEXT("Unknown");
	}
}

FString UDLLMultiplayerSessionTypeHelpers::GetSessionStateDisplayName(EDLLMultiplayerSessionState State)
{
	switch (State)
	{
		case EDLLMultiplayerSessionState::None:				return TEXT("No Session");
		case EDLLMultiplayerSessionState::Creating:			return TEXT("Creating...");
		case EDLLMultiplayerSessionState::Lobby:			return TEXT("In Lobby");
		case EDLLMultiplayerSessionState::Starting:			return TEXT("Starting...");
		case EDLLMultiplayerSessionState::InProgress:		return TEXT("In Progress");
		case EDLLMultiplayerSessionState::Ending:			return TEXT("Ending...");
		case EDLLMultiplayerSessionState::Destroying:		return TEXT("Closing...");
		default:								return TEXT("Unknown");
	}
}

FString UDLLMultiplayerSessionTypeHelpers::GetJoinResultDisplayName(EDLLMultiplayerJoinSessionResult Result)
{
	switch (Result)
	{
		case EDLLMultiplayerJoinSessionResult::Success:				return TEXT("Success");
		case EDLLMultiplayerJoinSessionResult::SessionFull:			return TEXT("Session is Full");
		case EDLLMultiplayerJoinSessionResult::SessionNotFound:		return TEXT("Session Not Found");
		case EDLLMultiplayerJoinSessionResult::NotInvited:			return TEXT("Not Invited");
		case EDLLMultiplayerJoinSessionResult::AlreadyInSession:	return TEXT("Already in Session");
		case EDLLMultiplayerJoinSessionResult::PasswordRequired:	return TEXT("Password Required");
		case EDLLMultiplayerJoinSessionResult::WrongPassword:		return TEXT("Wrong Password");
		case EDLLMultiplayerJoinSessionResult::VersionMismatch:		return TEXT("Version Mismatch");
		case EDLLMultiplayerJoinSessionResult::Banned:				return TEXT("Banned from Session");
		case EDLLMultiplayerJoinSessionResult::Failed:				return TEXT("Failed to Join");
		case EDLLMultiplayerJoinSessionResult::Unknown:				return TEXT("Unknown Error");
		default:										return TEXT("Unknown");
	}
}

bool UDLLMultiplayerSessionTypeHelpers::SupportsDropInDropOut(EDLLMultiplayerSessionType SessionType)
{
	switch (SessionType)
	{
		case EDLLMultiplayerSessionType::CampaignCoop:
		case EDLLMultiplayerSessionType::Horde:
			return true;

		case EDLLMultiplayerSessionType::CompetitiveFFA:
		case EDLLMultiplayerSessionType::CompetitiveTeam:
		case EDLLMultiplayerSessionType::Private:
		case EDLLMultiplayerSessionType::LAN:
		case EDLLMultiplayerSessionType::Custom:
			return false; // Configurable per match, default false

		default:
			return false;
	}
}

bool UDLLMultiplayerSessionTypeHelpers::RequiresAllPlayersReady(EDLLMultiplayerSessionType SessionType)
{
	switch (SessionType)
	{
		case EDLLMultiplayerSessionType::CompetitiveFFA:
		case EDLLMultiplayerSessionType::CompetitiveTeam:
			return true;

		case EDLLMultiplayerSessionType::CampaignCoop:
		case EDLLMultiplayerSessionType::Horde:
		case EDLLMultiplayerSessionType::Private:
		case EDLLMultiplayerSessionType::LAN:
		case EDLLMultiplayerSessionType::Custom:
			return false; // Host can start when ready

		default:
			return false;
	}
}

int32 UDLLMultiplayerSessionTypeHelpers::GetRecommendedMaxPlayers(EDLLMultiplayerSessionType SessionType)
{
	switch (SessionType)
	{
		case EDLLMultiplayerSessionType::CampaignCoop:		return 4;
		case EDLLMultiplayerSessionType::CompetitiveFFA:	return 8;
		case EDLLMultiplayerSessionType::CompetitiveTeam:	return 8;
		case EDLLMultiplayerSessionType::Horde:				return 4;
		case EDLLMultiplayerSessionType::Private:			return 4;
		case EDLLMultiplayerSessionType::LAN:				return 4;
		case EDLLMultiplayerSessionType::Custom:			return 16;
		default:								return 4;
	}
}
