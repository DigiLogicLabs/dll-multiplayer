// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DLLMultiplayerSessionTypes.generated.h"

/**
 * Kind of session.
 * Determines session behavior, matchmaking, and available features.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerSessionType : uint8
{
	/** Campaign co-op: 1-4 players, host-driven progression, drop-in/drop-out */
	CampaignCoop		UMETA(DisplayName = "Campaign Co-Op"),

	/** Free-for-all competitive modes (Deathmatch, etc.) */
	CompetitiveFFA		UMETA(DisplayName = "Competitive FFA"),

	/** Team-based competitive modes */
	CompetitiveTeam		UMETA(DisplayName = "Competitive Team"),

	/** Co-op survival/horde mode */
	Horde				UMETA(DisplayName = "Horde/Survival"),

	/** Private invite-only custom game */
	Private				UMETA(DisplayName = "Private Match"),

	/** Local network only (LAN party) */
	LAN					UMETA(DisplayName = "LAN"),

	/** Custom/modded session type */
	Custom				UMETA(DisplayName = "Custom")
};

/**
 * Session visibility determines who can see and join the session.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerSessionVisibility : uint8
{
	/** Anyone can find and join via server browser */
	Public				UMETA(DisplayName = "Public"),

	/** Only friends can see, requires invite or friend join */
	FriendsOnly			UMETA(DisplayName = "Friends Only"),

	/** Invite-only, not visible in any browser */
	Private				UMETA(DisplayName = "Private/Invite Only"),

	/** Local network only */
	LAN					UMETA(DisplayName = "LAN Only")
};

/**
 * Reason a player left the session.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerPlayerLeaveReason : uint8
{
	/** Player voluntarily disconnected */
	Voluntary			UMETA(DisplayName = "Left Voluntarily"),

	/** Player was kicked by host */
	Kicked				UMETA(DisplayName = "Kicked"),

	/** Player lost connection */
	ConnectionLost		UMETA(DisplayName = "Connection Lost"),

	/** Player timed out */
	Timeout				UMETA(DisplayName = "Timeout"),

	/** Session was destroyed */
	SessionEnded		UMETA(DisplayName = "Session Ended"),

	/** Unknown reason */
	Unknown				UMETA(DisplayName = "Unknown")
};

/**
 * Current state of the session.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerSessionState : uint8
{
	/** No active session */
	None				UMETA(DisplayName = "None"),

	/** Session is being created */
	Creating			UMETA(DisplayName = "Creating"),

	/** Session is in lobby, waiting for players */
	Lobby				UMETA(DisplayName = "In Lobby"),

	/** Session is starting (loading map, etc.) */
	Starting			UMETA(DisplayName = "Starting"),

	/** Session is in progress (game running) */
	InProgress			UMETA(DisplayName = "In Progress"),

	/** Session is ending */
	Ending				UMETA(DisplayName = "Ending"),

	/** Session is being destroyed */
	Destroying			UMETA(DisplayName = "Destroying")
};

/**
 * Result of a join attempt with detailed failure reasons.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerJoinSessionResult : uint8
{
	/** Successfully joined */
	Success				UMETA(DisplayName = "Success"),

	/** Session is full */
	SessionFull			UMETA(DisplayName = "Session Full"),

	/** Session not found or no longer exists */
	SessionNotFound		UMETA(DisplayName = "Session Not Found"),

	/** Session is private and player not invited */
	NotInvited			UMETA(DisplayName = "Not Invited"),

	/** Already in this session */
	AlreadyInSession	UMETA(DisplayName = "Already In Session"),

	/** Password required */
	PasswordRequired	UMETA(DisplayName = "Password Required"),

	/** Wrong password */
	WrongPassword		UMETA(DisplayName = "Wrong Password"),

	/** Build version mismatch */
	VersionMismatch		UMETA(DisplayName = "Version Mismatch"),

	/** Banned from session */
	Banned				UMETA(DisplayName = "Banned"),

	/** Generic failure */
	Failed				UMETA(DisplayName = "Failed"),

	/** Unknown error */
	Unknown				UMETA(DisplayName = "Unknown Error")
};

/**
 * Online service/platform type for abstraction.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerOnlineServiceType : uint8
{
	/** Auto-detect based on platform */
	Auto				UMETA(DisplayName = "Auto-Detect"),

	/** Steam */
	Steam				UMETA(DisplayName = "Steam"),

	/** Epic Online Services */
	EOS					UMETA(DisplayName = "Epic Online Services"),

	/** PlayStation Network */
	PlayStation			UMETA(DisplayName = "PlayStation Network"),

	/** Xbox Live */
	Xbox				UMETA(DisplayName = "Xbox Live"),

	/** Nintendo Online */
	Nintendo			UMETA(DisplayName = "Nintendo Online"),

	/** LAN/Null subsystem */
	LAN					UMETA(DisplayName = "LAN/Offline")
};

/**
 * Information about a player in the session.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerSessionPlayer
{
	GENERATED_BODY()

	/** Player's display name */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	FString DisplayName;

	/** Player's unique network ID */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	FUniqueNetIdRepl UniqueNetId;

	/** Is this player the session host? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	bool bIsHost = false;

	/** Is this a local player on this machine? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	bool bIsLocalPlayer = false;

	/** Local player index (0-3 for split-screen, -1 if remote) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	int32 LocalPlayerIndex = -1;

	/** Player's ping in milliseconds (-1 if local or unknown) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	int32 PingMs = -1;

	/** Is the player ready (in lobby)? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	bool bIsReady = false;

	/** Player's team index (-1 for FFA or unassigned) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Player")
	int32 TeamIndex = -1;

	FDLLMultiplayerSessionPlayer() = default;
};

/**
 * Detailed information about a session (for browser and current session display).
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerSessionInfo
{
	GENERATED_BODY()

	/** Session display name (host name or custom name) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	FString SessionName;

	/** Host player name */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	FString HostName;

	/** Current player count */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	int32 CurrentPlayers = 0;

	/** Maximum player count */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	int32 MaxPlayers = 4;

	/** Session type */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	EDLLMultiplayerSessionType SessionType = EDLLMultiplayerSessionType::CampaignCoop;

	/** Session visibility */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	EDLLMultiplayerSessionVisibility Visibility = EDLLMultiplayerSessionVisibility::Public;

	/** Current map name */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	FString MapName;

	/** Current game mode */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	FString GameMode;

	/** Match type string (for filtering) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	FString MatchType;

	/** Ping to server in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	int32 PingMs = -1;

	/** Is the session password protected? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	bool bHasPassword = false;

	/** Can players join while in progress? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	bool bAllowJoinInProgress = true;

	/** Is this a dedicated server? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	bool bIsDedicatedServer = false;

	/** Is this a LAN session? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	bool bIsLAN = false;

	/** Session state */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	EDLLMultiplayerSessionState State = EDLLMultiplayerSessionState::None;

	/** Build/version ID for compatibility checking */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	int32 BuildUniqueId = 0;

	/** Online session search result (for joining) */
	FOnlineSessionSearchResult SearchResult;

	/** List of players in the session */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	TArray<FDLLMultiplayerSessionPlayer> Players;

	/** Custom session settings (key-value pairs) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Info")
	TMap<FString, FString> CustomSettings;

	FDLLMultiplayerSessionInfo() = default;

	/** Create from OnlineSessionSearchResult */
	static FDLLMultiplayerSessionInfo FromSearchResult(const FOnlineSessionSearchResult& Result);

	/** Check if this session info is valid */
	bool IsValid() const { return MaxPlayers > 0; }
};

/**
 * Filter criteria for session searches.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerSessionSearchFilter
{
	GENERATED_BODY()

	/** Maximum number of results to return */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 MaxResults = 100;

	/** Filter by session type (empty = all types) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	TArray<EDLLMultiplayerSessionType> SessionTypes;

	/** Filter by match type string (empty = all) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	FString MatchType;

	/** Filter by map name (empty = all) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	FString MapName;

	/** Filter by game mode (empty = all) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	FString GameMode;

	/** Only show sessions with available slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	bool bOnlyShowAvailable = true;

	/** Only show sessions that allow join in progress */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	bool bOnlyShowJoinable = true;

	/** Include LAN sessions in search */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search")
	bool bIncludeLAN = true;

	/** Maximum acceptable ping (0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search", meta = (ClampMin = "0"))
	int32 MaxPing = 0;

	/** Minimum player count (0 = no minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search", meta = (ClampMin = "0"))
	int32 MinPlayers = 0;

	/** Maximum player count (0 = no maximum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Search", meta = (ClampMin = "0"))
	int32 MaxPlayers = 0;

	FDLLMultiplayerSessionSearchFilter() = default;

	/** Check if a session matches this filter */
	bool Matches(const FDLLMultiplayerSessionInfo& SessionInfo) const;
};

/**
 * Game invite information.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerGameInvite
{
	GENERATED_BODY()

	/** Player who sent the invite */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Invite")
	FDLLMultiplayerSessionPlayer FromPlayer;

	/** Session to join */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Invite")
	FDLLMultiplayerSessionInfo SessionInfo;

	/** Invite message (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Invite")
	FString Message;

	/** Time invite was received */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Invite")
	FDateTime ReceivedTime;

	/** Is the invite still valid? */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Invite")
	bool bIsValid = true;

	FDLLMultiplayerGameInvite() : ReceivedTime(FDateTime::Now()) {}
};

/**
 * Handle to a session for internal tracking.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerSessionHandle
{
	GENERATED_BODY()

	/** Internal session ID */
	UPROPERTY(BlueprintReadOnly, Category = "Session|Handle")
	FString SessionId;

	/** Is this handle valid? */
	bool IsValid() const { return !SessionId.IsEmpty(); }

	FDLLMultiplayerSessionHandle() = default;
	explicit FDLLMultiplayerSessionHandle(const FString& InSessionId) : SessionId(InSessionId) {}

	bool operator==(const FDLLMultiplayerSessionHandle& Other) const { return SessionId == Other.SessionId; }
	bool operator!=(const FDLLMultiplayerSessionHandle& Other) const { return !(*this == Other); }
};

/**
 * Utility functions for session types
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerSessionTypeHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get display name for session type */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static FString GetSessionTypeDisplayName(EDLLMultiplayerSessionType SessionType);

	/** Get display name for visibility */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static FString GetVisibilityDisplayName(EDLLMultiplayerSessionVisibility Visibility);

	/** Get display name for session state */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static FString GetSessionStateDisplayName(EDLLMultiplayerSessionState State);

	/** Get display name for join result */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static FString GetJoinResultDisplayName(EDLLMultiplayerJoinSessionResult Result);

	/** Check if session type supports drop-in/drop-out */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static bool SupportsDropInDropOut(EDLLMultiplayerSessionType SessionType);

	/** Check if session type requires all players ready to start */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static bool RequiresAllPlayersReady(EDLLMultiplayerSessionType SessionType);

	/** Get recommended max players for session type */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Types")
	static int32 GetRecommendedMaxPlayers(EDLLMultiplayerSessionType SessionType);
};
