// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Types/DLLMultiplayerSessionTypes.h"
#include "Config/DLLMultiplayerMatchSettings.h"

#include "DLLMultiplayerSubsystem.generated.h"

class IOnlineSubsystem;

// Forward declarations
class UDLLMultiplayerSessionConfiguration;
struct FDLLMultiplayerSessionCreationSettings;

// ============================================================================
// EVENT DELEGATE TYPES
// ============================================================================

/** Broadcast when session creation completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnSessionCreated, bool, bWasSuccessful, const FDLLMultiplayerSessionInfo&, SessionInfo);

/** Broadcast when session search completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnSessionsFound, const TArray<FDLLMultiplayerSessionInfo>&, Sessions, bool, bWasSuccessful);

/** Broadcast when joining a session completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnSessionJoined, EDLLMultiplayerJoinSessionResult, Result, const FDLLMultiplayerSessionInfo&, SessionInfo);

/** Broadcast when session destruction completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLLMultiplayerOnSessionDestroyed, bool, bWasSuccessful);

/** Broadcast when session start completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLLMultiplayerOnSessionStarted, bool, bWasSuccessful);

/** Broadcast when session state changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnSessionStateChanged, EDLLMultiplayerSessionState, NewState, EDLLMultiplayerSessionState, PreviousState);

/** Broadcast just before a host-a-level travel runs, so a project can add its own transition work */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLLMultiplayerOnPreHostTravel, const FString&, LevelPath);

/** Broadcast when a player joins the session */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnPlayerJoined, const FDLLMultiplayerSessionPlayer&, Player, bool, bIsLocalPlayer);

/** Broadcast when a player leaves the session */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnPlayerLeft, const FDLLMultiplayerSessionPlayer&, Player, EDLLMultiplayerPlayerLeaveReason, Reason);

/** Broadcast when player ready state changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnPlayerReadyStateChanged, const FDLLMultiplayerSessionPlayer&, Player, bool, bIsReady);

/** Broadcast when all players are ready */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDLLMultiplayerOnAllPlayersReady);

/** Broadcast when a game invite is received */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLLMultiplayerOnGameInviteReceived, const FDLLMultiplayerGameInvite&, Invite);

/** Broadcast when an invite is sent */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnInviteSent, bool, bWasSuccessful, const FUniqueNetIdRepl&, RecipientId);

/** Broadcast when a session error occurs */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnSessionError, const FString&, ErrorCode, const FString&, ErrorMessage);

/** Broadcast when a local player is registered */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnLocalPlayerRegistered, int32, LocalPlayerIndex, APlayerController*, PlayerController);

/** Broadcast when a local player is unregistered */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLLMultiplayerOnLocalPlayerUnregistered, int32, LocalPlayerIndex);

/**
 * Game instance subsystem for managing multiplayer sessions.
 *
 * This subsystem provides a unified interface for:
 * - Session creation (public, private, LAN)
 * - Session discovery and joining
 * - Session management (start, destroy, host migration)
 * - Local split-screen player registration
 * - Platform abstraction (Steam, EOS, LAN, etc.)
 *
 * Usage:
 * 1. Get the subsystem from GameInstance
 * 2. Bind to delegates for async operation results
 * 3. Call session management methods
 *
 * Example:
 *   UDLLMultiplayerSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UDLLMultiplayerSubsystem>();
 *   Subsystem->OnSessionCreated.AddDynamic(this, &MyClass::HandleSessionCreated);
 *   Subsystem->CreateSessionWithConfig(MyConfig);
 *
 * @see UDLLMultiplayerSessionConfiguration for session settings
 * @see FDLLMultiplayerMatchSettings for match rules
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Binds platform invite acceptance (Steam overlay "Accept"/"Join Game", +connect_lobby launches). */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** Unbinds the invite handler and any in-flight operation, and drops queued leave continuations. */
	virtual void Deinitialize() override;

	// ========================================================================
	// SESSION LIFECYCLE
	// ========================================================================

	/**
	 * Create a session from just a player cap and a match type (C++ convenience).
	 * Online (presence/lobby) session, or a LAN session when the active online service is Null.
	 * If a session already exists it is left first. Result arrives on OnSessionCreated.
	 * @param NumPublicConnections Player cap
	 * @param MatchType Advertised match type (empty = "Campaign")
	 */
	void CreateSession(int32 NumPublicConnections, FString MatchType);

	/**
	 * Search for sessions with default filtering: LAN when the active service is Null, otherwise online
	 * lobbies. Result arrives on OnSessionsFound.
	 * @param MaxSearchResults Upper bound on raw results requested from the online service
	 */
	void FindSessions(int32 MaxSearchResults);

	/** Leave/destroy the current game session. Result arrives on OnSessionDestroyed. */
	void DestroySession();

	/** Mark the current game session as started (in progress). Result arrives on OnSessionStarted. */
	void StartSession();

	/**
	 * Resolve (lazily) and report whether this game instance's online session interface is available.
	 * The first successful resolve also binds platform invite acceptance.
	 */
	bool HasSessionInterface();

	/**
	 * Create a session using a configuration data asset.
	 * @param Config The session configuration to use
	 * @return True if creation started, false if immediate failure
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool CreateSessionWithConfig(UDLLMultiplayerSessionConfiguration* Config);

	/**
	 * Create a session with full settings (config + overrides).
	 * @param Settings Complete session creation settings
	 * @return True if creation started, false if immediate failure
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool CreateSessionWithSettings(const FDLLMultiplayerSessionCreationSettings& Settings);

	/**
	 * Create a private/invite-only session.
	 * @param Config The session configuration to use
	 * @param Password Optional password protection
	 * @return True if creation started, false if immediate failure
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool CreatePrivateSession(UDLLMultiplayerSessionConfiguration* Config, const FString& Password = TEXT(""));

	/**
	 * Create a private/invite-only session from plain arguments - no configuration data asset needed.
	 * @param NumConnections Player cap (at least 1)
	 * @param MatchType Advertised match type
	 * @param Password Optional password. It travels in the connect URL, so it may not contain spaces or ? = # &.
	 * @return True if creation started, false if immediate failure
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool CreateQuickPrivateSession(int32 NumConnections = 4, const FString& MatchType = TEXT("Coop"), const FString& Password = TEXT(""));

	/**
	 * Host side of password protection: true when Supplied matches this host's session password, or when this
	 * machine is not hosting a password-protected session. ADLLMultiplayerGameSession::ApproveLogin calls it; a game
	 * session class of your own can do the same.
	 */
	bool IsSessionPasswordAccepted(const FString& Supplied) const;

	/** Login refusal reason sent to a client whose password did not match. The client reports it as WrongPassword. */
	static const FString ErrorWrongPassword;

	/**
	 * Create a LAN session (no online subsystem required).
	 * @param NumConnections Maximum number of players
	 * @param SessionName Display name for the session
	 * @return True if creation started, false if immediate failure
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool CreateLANSession(int32 NumConnections, const FString& SessionName);

	// ========================================================================
	// HOST-A-LEVEL - "open the map I am standing in to other players"
	// ========================================================================

	/**
	 * Create a session and then reopen a LEVEL as a listen server, so the hosting player never
	 * leaves the place they are standing - it comes back up joinable underneath them.
	 *
	 * This is the co-op lobby mechanic every project re-writes: hub, camp, ship, tavern. It is here
	 * rather than in a game module because nothing about it is game-specific.
	 *
	 * ASYNC, AND THAT IS THE WHOLE POINT. Session creation completes on a callback. Travelling first
	 * produces a level that LOOKS hosted and accepts nobody, because the travel would run while
	 * IsInSession() is still false and therefore never gain `?listen`. The level path is cached
	 * across the callback and the travel happens on success only.
	 *
	 * @param LevelPath  Long package name, e.g. "/Game/Levels/Village". Empty is refused.
	 * @param NumConnections Player cap for the session.
	 * @param MatchType  Advertised match type, so browsers can tell a hub lobby from a match lobby.
	 * @param bLAN       LAN session (no online subsystem login needed).
	 * @param bPrivate   Private/invite-only. Ignored when bLAN is true.
	 * @return True when session creation STARTED. Success arrives on OnSessionCreated.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Host")
	bool HostSessionAndTravelToLevel(
		const FString& LevelPath,
		int32 NumConnections = 4,
		const FString& MatchType = TEXT("Coop"),
		bool bLAN = true,
		bool bPrivate = false);

	/**
	 * Fired after the session exists and immediately BEFORE the host travel runs.
	 *
	 * Exists so a project can keep its own travel-time behaviour - loading screens, transition
	 * hooks, saving - without this plugin needing to know about any of it. A typical game binds it to
	 * its own "level transition starting" event, which its loading screen listens to.
	 *
	 * Bind, do synchronous work, return. The travel happens as soon as this returns.
	 */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Host")
	FDLLMultiplayerOnPreHostTravel OnPreHostTravel;

	// ========================================================================
	// DISCOVERY
	// ========================================================================

	/**
	 * Find sessions with filtering options.
	 * @param Filter Search filter criteria
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	void FindSessionsFiltered(const FDLLMultiplayerSessionSearchFilter& Filter);

	/**
	 * Refresh the session list using the last filter.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	void RefreshSessionList();

	/**
	 * Get the cached session list from last search.
	 * @return Array of session info (may be empty if no search performed)
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	TArray<FDLLMultiplayerSessionInfo> GetCachedSessionList() const { return CachedSessions; }

	/**
	 * Check if a session search is in progress.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	bool IsSearchingForSessions() const { return bIsSearching; }

	// ========================================================================
	// JOINING
	// ========================================================================

	/**
	 * Join a session using session info (from browser).
	 * @param SessionInfo The session to join
	 * @param Password Optional password if required
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	void JoinSessionByInfo(const FDLLMultiplayerSessionInfo& SessionInfo, const FString& Password = TEXT(""));

	/**
	 * Join a session by index in the cached list.
	 * @param Index Index in GetCachedSessionList()
	 * @param Password Optional password if required
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	void JoinSessionByIndex(int32 Index, const FString& Password = TEXT(""));

	// ========================================================================
	// SESSION STATE
	// ========================================================================

	/**
	 * Get information about the current session.
	 * @return Current session info (check IsValid())
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	FDLLMultiplayerSessionInfo GetCurrentSessionInfo() const;

	/**
	 * Get the current session state.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	EDLLMultiplayerSessionState GetSessionState() const { return CurrentSessionState; }

	/**
	 * Check if we are currently in a session.
	 *
	 * 2026-10-01: also requires the online subsystem to still HAVE the session. The cached state only changes
	 * through this subsystem's own calls, so a session destroyed directly through the OSS (for example by a
	 * menu's own Destroy Session node) left it reporting "in session" forever, and every
	 * later join or host was refused as AlreadyInSession.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	bool IsInSession() const;

	/**
	 * Drop a stale "in session" state when the online subsystem no longer has the session (see IsInSession).
	 * Called by UDLLMultiplayerSessionLibrary before every operation; safe to call any time. Returns true when it reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool ReconcileSessionState();

	/**
	 * Check if we are the host of the current session.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	bool IsSessionHost() const;

	/**
	 * Get the match settings for the current session.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	FDLLMultiplayerMatchSettings GetCurrentMatchSettings() const { return CurrentMatchSettings; }

	/**
	 * Update match settings (host only, before starting).
	 * @param NewSettings The new match settings
	 * @return True if settings were updated
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool UpdateMatchSettings(const FDLLMultiplayerMatchSettings& NewSettings);

	// ========================================================================
	// LOCAL PLAYER REGISTRATION (SPLIT-SCREEN)
	// ========================================================================

	/**
	 * Register a local player for multiplayer.
	 * Call for each split-screen player that should be part of the session.
	 * @param PlayerController The local player controller
	 * @return Local player index (0-3), or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|LocalPlayers")
	int32 RegisterLocalPlayer(APlayerController* PlayerController);

	/**
	 * Unregister a local player from multiplayer.
	 * @param PlayerController The local player controller to unregister
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|LocalPlayers")
	void UnregisterLocalPlayer(APlayerController* PlayerController);

	/**
	 * Get all registered local players.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|LocalPlayers")
	TArray<APlayerController*> GetRegisteredLocalPlayers() const;

	/**
	 * Get the number of registered local players.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|LocalPlayers")
	int32 GetLocalPlayerCount() const { return RegisteredLocalPlayers.Num(); }

	// ========================================================================
	// PLAYER MANAGEMENT
	// ========================================================================

	/**
	 * Get all players in the current session.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Players")
	TArray<FDLLMultiplayerSessionPlayer> GetSessionPlayers() const { return SessionPlayers; }

	/**
	 * Get the number of players in the session.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Players")
	int32 GetPlayerCount() const { return SessionPlayers.Num(); }

	/**
	 * Kick a player from the session (host only).
	 * @param PlayerId The unique ID of the player to kick
	 * @return True if kick was initiated
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Players")
	bool KickPlayer(const FUniqueNetIdRepl& PlayerId);

	/**
	 * Set local player ready state (lobby).
	 * @param LocalPlayerIndex Which local player (0-3)
	 * @param bIsReady Ready state
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Players")
	void SetLocalPlayerReady(int32 LocalPlayerIndex, bool bIsReady);

	/**
	 * Check if all players are ready (lobby).
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Players")
	bool AreAllPlayersReady() const;

	// ========================================================================
	// INVITES
	// ========================================================================

	/**
	 * Send a game invite to a friend.
	 * @param FriendId The unique ID of the friend to invite
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Invites")
	void SendInvite(const FUniqueNetIdRepl& FriendId);

	/**
	 * Accept a received game invite.
	 * @param Invite The invite to accept
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Invites")
	void AcceptInvite(const FDLLMultiplayerGameInvite& Invite);

	/**
	 * Decline a received game invite.
	 * @param Invite The invite to decline
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Invites")
	void DeclineInvite(const FDLLMultiplayerGameInvite& Invite);

	// ========================================================================
	// PLATFORM INFO
	// ========================================================================

	/**
	 * Get the current online service type.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Platform")
	EDLLMultiplayerOnlineServiceType GetCurrentServiceType() const;

	/**
	 * Get the platform-specific service name (e.g., "Steam", "NULL").
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Platform")
	FString GetServiceName() const;

	/**
	 * Check if we are using LAN (NULL subsystem).
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Platform")
	bool IsUsingLAN() const;

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Broadcast when session creation completes or fails (exactly once per create request) */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionCreated OnSessionCreated;

	/** Broadcast when session search completes */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionsFound OnSessionsFound;

	/** Broadcast when joining a session completes */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionJoined OnSessionJoined;

	/** Broadcast when session destruction completes */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionDestroyed OnSessionDestroyed;

	/** Broadcast when session start completes */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionStarted OnSessionStarted;

	/** Broadcast when session state changes */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionStateChanged OnSessionStateChanged;

	/** Broadcast when a player joins */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnPlayerJoined OnPlayerJoined;

	/** Broadcast when a player leaves */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnPlayerLeft OnPlayerLeft;

	/** Broadcast when player ready state changes */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnPlayerReadyStateChanged OnPlayerReadyStateChanged;

	/** Broadcast when all players are ready */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnAllPlayersReady OnAllPlayersReady;

	/** Broadcast when a game invite is received */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnGameInviteReceived OnGameInviteReceived;

	/** Broadcast when an invite is sent */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnInviteSent OnInviteSent;

	/** Broadcast when a session error occurs */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnSessionError OnSessionError;

	/** Broadcast when a local player is registered */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnLocalPlayerRegistered OnLocalPlayerRegistered;

	/** Broadcast when a local player is unregistered */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|Events")
	FDLLMultiplayerOnLocalPlayerUnregistered OnLocalPlayerUnregistered;

	// ========================================================================
	// DEBUG
	// ========================================================================

	/** Master toggle for debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DLL Multiplayer|Debug")
	bool bEnableVisualDebug = false;

	/** Show session state on screen HUD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DLL Multiplayer|Debug", meta = (EditCondition = "bEnableVisualDebug"))
	bool bDebugShowSessionState = true;

	/** Show player list on screen HUD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DLL Multiplayer|Debug", meta = (EditCondition = "bEnableVisualDebug"))
	bool bDebugShowPlayerList = true;

	/** Draw debug info (call from Tick) */
	void DrawDebugInfo();

	/**
	 * One-shot completion for HostSessionAndTravelToLevel. Separate from any other create handler so
	 * two host flows can never consume each other's completion.
	 */
	UFUNCTION()
	void HandleHostForLevelCreated(bool bWasSuccessful, const FDLLMultiplayerSessionInfo& SessionInfo);

	/** Level cached across the async session-creation callback. Cleared once travel is attempted. */
	UPROPERTY(Transient)
	FString PendingHostLevelPath;

protected:
	// ========================================================================
	// ONLINE SERVICE COMPLETIONS (bound only while their operation is in flight)
	// ========================================================================

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	/** StateBeforeDestroy is bound as a payload when the destroy starts, so a failed destroy can restore it. */
	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful, EDLLMultiplayerSessionState StateBeforeDestroy);
	void HandleStartComplete(FName SessionName, bool bWasSuccessful);

	// ========================================================================
	// INTERNAL STATE MANAGEMENT
	// ========================================================================

	void SetSessionState(EDLLMultiplayerSessionState NewState);
	void UpdateSessionInfo();
	void BuildSessionInfoFromSettings(const FOnlineSessionSettings& Settings, FDLLMultiplayerSessionInfo& OutInfo) const;
	EDLLMultiplayerJoinSessionResult ConvertJoinResult(EOnJoinSessionCompleteResult::Type Result) const;

	/**
	 * Resolve a display name for a local player.
	 * Tries the online identity nickname, then the PlayerState name, then the
	 * ULocalPlayer nickname, and finally a "Player N" fallback so lobby rows are
	 * never blank (important for the NULL/LAN subsystem which has no account name).
	 */
	FString GetLocalPlayerNickname(int32 LocalPlayerIndex) const;

private:
	// ========================================================================
	// ONLINE SESSION INTERFACE
	// ========================================================================

	IOnlineSessionPtr SessionInterface;

	// ========================================================================
	// OPERATION TRACKER
	// ========================================================================
	// Every async session call binds its completion when it starts and unbinds it when it finishes, so
	// nothing listens to the online service while idle, and a completion that belongs to some other
	// caller of the same interface is not handled while we are idle.

	/** The kinds of async session operation; at most one of each is in flight. */
	enum class EDLLMultiplayerSessionOpKind : uint8
	{
		Create,
		Find,
		Join,
		Destroy,
		Start
	};

	/** One in-flight operation: what it is and the completion binding to remove when it ends. */
	struct FDLLMultiplayerSessionOp
	{
		EDLLMultiplayerSessionOpKind Kind = EDLLMultiplayerSessionOpKind::Create;
		FDelegateHandle Handle;
		/** The interface the binding lives on, in case SessionInterface is re-resolved meanwhile. */
		TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> Interface;
	};

	TArray<FDLLMultiplayerSessionOp, TInlineAllocator<5>> InFlightOps;

	bool IsOpInFlight(EDLLMultiplayerSessionOpKind Kind) const;
	void TrackOp(EDLLMultiplayerSessionOpKind Kind, FDelegateHandle Handle);

	/**
	 * Unbind and forget an operation. Returns false when it was not in flight, which is how a caller
	 * whose online-service call returned false learns that the completion already fired synchronously
	 * (the Null subsystem does this) and the result has already been reported.
	 */
	bool FinishOp(EDLLMultiplayerSessionOpKind Kind);
	static void UnbindOp(const FDLLMultiplayerSessionOp& Op);
	void UnbindAllOps();

	// ========================================================================
	// LEAVE CONTINUATION
	// ========================================================================

	/**
	 * Leave the current game session, then run Then(bLeft) once destruction has finished. A newer
	 * continuation replaces a queued one (an accepted invite outranks a queued re-create).
	 */
	void LeaveThen(TUniqueFunction<void(bool /*bLeft*/)>&& Then);
	void RunLeaveContinuation(bool bLeft);

	/** Action waiting for the current session to be destroyed. Empty when nothing is queued. */
	TUniqueFunction<void(bool /*bLeft*/)> PendingAfterLeave;

	// ========================================================================
	// SINGLE CREATE / SEARCH / JOIN PATHS
	// ========================================================================

	/** Map creation settings onto the engine's session settings. LAN is a property of the result. */
	FOnlineSessionSettings BuildOnlineSettings(const FDLLMultiplayerSessionCreationSettings& Settings) const;

	/** Hand validated settings to the online service (no existing session at this point). */
	bool SubmitCreate(const FDLLMultiplayerSessionCreationSettings& Settings);

	/** Join a raw search result (browser row or accepted platform invite). */
	void JoinSearchResult(const FOnlineSessionSearchResult& SearchResult);

	/** The search object of the running find; held here because the service fills it asynchronously. */
	TSharedPtr<FOnlineSessionSearch> ActiveSearch;

	/** The online subsystem for THIS game instance's world (per-window in PIE; the default instance otherwise). */
	IOnlineSubsystem* GetWorldOnlineSubsystem() const;
	bool IsPlayInEditorInstance() const;

	void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId,
		FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	FDelegateHandle SessionUserInviteAcceptedDelegateHandle;

	/** A platform invite arrived (Steam, EOS...); the Null subsystem never sends one. Fires OnGameInviteReceived. */
	void HandleSessionInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId,
		const FOnlineSessionSearchResult& InviteResult);
	FDelegateHandle SessionInviteReceivedDelegateHandle;

	/** Host only: keeps SessionPlayers in step with logins and fires OnPlayerJoined / OnPlayerLeft. */
	void HandleGameModePostLogin(class AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleGameModeLogout(class AGameModeBase* GameMode, class AController* Exiting);
	FDelegateHandle PostLoginDelegateHandle;
	FDelegateHandle LogoutDelegateHandle;

	/** Client: the host refused our login (wrong password) after the online join had already succeeded. */
	void HandleNetworkFailure(UWorld* World, class UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	FDelegateHandle NetworkFailureDelegateHandle;

	/** Session-player entry for a controller (name and id from its PlayerState; local seat index for local players). */
	FDLLMultiplayerSessionPlayer MakeSessionPlayer(const APlayerController* PlayerController) const;

	/** Players removed with KickPlayer whose logout has not arrived yet, so OnPlayerLeft can report Kicked. */
	TArray<FUniqueNetIdRepl> PendingKicks;

	/**
	 * Map changes log every remote player out (non-seamless travel does it during ServerTravelPause, before the old
	 * world tears down). Those logouts are ignored; after the travel, ReconcileRosterAfterTravel removes and reports
	 * only the players that did not come back.
	 */
	void ScheduleRosterReconcile();
	void ReconcileRosterAfterTravel();
	FTimerHandle RosterReconcileTimer;

	/** Seconds after the last travel logout before missing players are reported as gone. */
	static constexpr float RosterReconcileDelaySeconds = 30.0f;

	/**
	 * Report "session creation did not happen": state back to None and OnSessionCreated(false), so a menu
	 * waiting on it stops spinning. Every early-out in a create path must end here, exactly once.
	 */
	void BroadcastCreateFailed();

	/**
	 * Build a throwaway UDLLMultiplayerSessionConfiguration from plain (connections, match type) arguments, so the
	 * settings-based create path can serve callers that hold no configuration data asset.
	 * Returns nullptr only if object creation fails.
	 */
	UDLLMultiplayerSessionConfiguration* MakeTransientSessionConfiguration(
		int32 NumConnections,
		const FString& MatchType,
		EDLLMultiplayerSessionVisibility Visibility);

	// ========================================================================
	// SESSION STATE
	// ========================================================================

	// Current session state
	EDLLMultiplayerSessionState CurrentSessionState = EDLLMultiplayerSessionState::None;

	// Current session info
	FDLLMultiplayerSessionInfo CurrentSession;

	// Current match settings
	FDLLMultiplayerMatchSettings CurrentMatchSettings;

	// Cached session list from last search
	TArray<FDLLMultiplayerSessionInfo> CachedSessions;

	// Last used search filter
	FDLLMultiplayerSessionSearchFilter LastSearchFilter;

	// Searching flag
	bool bIsSearching = false;

	// Registered local players (for split-screen)
	UPROPERTY()
	TArray<TWeakObjectPtr<APlayerController>> RegisteredLocalPlayers;

	// Players in current session
	TArray<FDLLMultiplayerSessionPlayer> SessionPlayers;


	// Password for current session (if private)
	FString CurrentSessionPassword;

	// Debug key counter for on-screen messages
	int32 DebugMessageKey = 5000;
};
