// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "DLLMultiplayerSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/GameInstance.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"
#include "Config/DLLMultiplayerSessionConfiguration.h"
#include "Config/DLLMultiplayerMatchSettings.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Player/DLLMultiplayerLocalPlayerLibrary.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Travel/DLLMultiplayerTravelLibrary.h"
#include "UObject/StrongObjectPtr.h"

namespace DLLMultiplayerSessionCore
{
	/** Advertised session keys. FDLLMultiplayerSessionInfo reads the same names back (BuildSessionInfoFromSettings, FromSearchResult). */
	static const FName KeyMatchType(TEXT("MatchType"));
	static const FName KeySessionType(TEXT("SessionType"));
	static const FName KeyVisibility(TEXT("Visibility"));
	static const FName KeyGameMode(TEXT("GameMode"));
	static const FName KeySessionName(TEXT("SessionName"));
	static const FName KeyMapName(TEXT("MapName"));
	static const FName KeyHasPassword(TEXT("HasPassword"));

	/**
	 * The local player that owns session calls (create/find/join are issued on its behalf).
	 * Returns null and names the reason in OutErrorCode ("NoWorld" / "NoLocalPlayer").
	 */
	static ULocalPlayer* GetSessionOwner(UWorld* World, FString& OutErrorCode)
	{
		if (!World)
		{
			OutErrorCode = TEXT("NoWorld");
			return nullptr;
		}
		ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
		if (!LocalPlayer)
		{
			OutErrorCode = TEXT("NoLocalPlayer");
		}
		return LocalPlayer;
	}

	/** Passwords ride in the connect URL as "?Password=...", so URL separators and spaces cannot be part of one. */
	static bool IsTravelSafePassword(const FString& Password)
	{
		for (const TCHAR Char : Password)
		{
			if (FChar::IsWhitespace(Char) || Char == TEXT('?') || Char == TEXT('=') || Char == TEXT('#') || Char == TEXT('&'))
			{
				return false;
			}
		}
		return true;
	}

	/** Same seat: local players by seat index, remote players by unique id (by name when a service gives no ids). */
	static bool IsSamePlayer(const FDLLMultiplayerSessionPlayer& A, const FDLLMultiplayerSessionPlayer& B)
	{
		if (A.bIsLocalPlayer || B.bIsLocalPlayer)
		{
			return A.bIsLocalPlayer && B.bIsLocalPlayer && A.LocalPlayerIndex == B.LocalPlayerIndex;
		}
		if (A.UniqueNetId.IsValid() || B.UniqueNetId.IsValid())
		{
			return A.UniqueNetId == B.UniqueNetId;
		}
		return !A.DisplayName.IsEmpty() && A.DisplayName == B.DisplayName;
	}
}

const FString UDLLMultiplayerSubsystem::ErrorWrongPassword(TEXT("WrongPassword"));

void UDLLMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Resolve now only outside PIE. A PIE game instance has no world yet at this point, and resolving
	// early would lock onto the PROCESS-WIDE online subsystem instead of this window's own context
	// (see GetWorldOnlineSubsystem). In PIE the lookup happens on first use instead. A standalone or
	// packaged game resolves here, so a Steam "+connect_lobby" launch still reaches the invite handler.
	if (!IsPlayInEditorInstance())
	{
		HasSessionInterface();
	}

	// Engine-wide events, filtered to this game instance in the handlers (PIE runs several in one process).
	PostLoginDelegateHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &ThisClass::HandleGameModePostLogin);
	LogoutDelegateHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &ThisClass::HandleGameModeLogout);
	if (GEngine)
	{
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
}

bool UDLLMultiplayerSubsystem::IsPlayInEditorInstance() const
{
	const UGameInstance* GI = GetGameInstance();
	const FWorldContext* Context = GI ? GI->GetWorldContext() : nullptr;
	return Context && Context->WorldType == EWorldType::PIE;
}

IOnlineSubsystem* UDLLMultiplayerSubsystem::GetWorldOnlineSubsystem() const
{
	// ========================================================================
	// PER-WORLD, NOT PROCESS-WIDE
	// ========================================================================
	// PIE gives every window its own online subsystem context (log: "Created online subsystem
	// instance for: :Context_N"). IOnlineSubsystem::Get() ignores that and returns the default
	// instance, so two PIE windows shared ONE Null session interface - one LAN session object that
	// was simultaneously hosting and searching, so a browser never listed the other window's game.
	// Online::GetSubsystem(World) picks the right context in PIE and is identical to Get() in a
	// standalone or packaged game.
	const UGameInstance* GI = GetGameInstance();
	if (UWorld* World = GI ? GI->GetWorld() : nullptr)
	{
		if (IOnlineSubsystem* ForWorld = Online::GetSubsystem(World))
		{
			return ForWorld;
		}
	}

	// No world yet: in PIE refuse rather than hand back the shared default instance.
	return IsPlayInEditorInstance() ? nullptr : IOnlineSubsystem::Get();
}

void UDLLMultiplayerSubsystem::Deinitialize()
{
	// Nothing may call back into a dead subsystem: unbind every in-flight completion and drop any action
	// that was waiting for a leave to finish.
	UnbindAllOps();
	PendingAfterLeave.Reset();
	ActiveSearch.Reset();
	bIsSearching = false;

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegateHandle);
		SessionInterface->ClearOnSessionInviteReceivedDelegate_Handle(SessionInviteReceivedDelegateHandle);
	}
	SessionInterface.Reset();

	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginDelegateHandle);
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutDelegateHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
	}
	PendingKicks.Reset();
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(RosterReconcileTimer);
	}

	Super::Deinitialize();
}

bool UDLLMultiplayerSubsystem::HasSessionInterface()
{
	if (!SessionInterface.IsValid())
	{
		// Lazy: in PIE the world (and with it this window's online context) only exists after Initialize.
		IOnlineSubsystem* OnlineSubsystem = GetWorldOnlineSubsystem();
		SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;

		if (SessionInterface.IsValid() && !SessionUserInviteAcceptedDelegateHandle.IsValid())
		{
			SessionUserInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSessionUserInviteAccepted));
			SessionInviteReceivedDelegateHandle = SessionInterface->AddOnSessionInviteReceivedDelegate_Handle(
				FOnSessionInviteReceivedDelegate::CreateUObject(this, &ThisClass::HandleSessionInviteReceived));
			UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: session interface resolved on '%s'"),
				*OnlineSubsystem->GetSubsystemName().ToString());
		}
	}
	return SessionInterface.IsValid();
}

// ============================================================================
// OPERATION TRACKER
// ============================================================================

bool UDLLMultiplayerSubsystem::IsOpInFlight(EDLLMultiplayerSessionOpKind Kind) const
{
	return InFlightOps.ContainsByPredicate([Kind](const FDLLMultiplayerSessionOp& Op) { return Op.Kind == Kind; });
}

void UDLLMultiplayerSubsystem::TrackOp(EDLLMultiplayerSessionOpKind Kind, FDelegateHandle Handle)
{
	ensureMsgf(!IsOpInFlight(Kind), TEXT("DLLMultiplayer: operation %d started twice"), static_cast<int32>(Kind));

	FDLLMultiplayerSessionOp& Op = InFlightOps.AddDefaulted_GetRef();
	Op.Kind = Kind;
	Op.Handle = Handle;
	Op.Interface = SessionInterface;
}

bool UDLLMultiplayerSubsystem::FinishOp(EDLLMultiplayerSessionOpKind Kind)
{
	const int32 Index = InFlightOps.IndexOfByPredicate([Kind](const FDLLMultiplayerSessionOp& Op) { return Op.Kind == Kind; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	// Removing a binding from inside its own broadcast is supported by the engine's multicast delegates.
	const FDLLMultiplayerSessionOp Op = InFlightOps[Index];
	InFlightOps.RemoveAtSwap(Index);
	UnbindOp(Op);
	return true;
}

void UDLLMultiplayerSubsystem::UnbindOp(const FDLLMultiplayerSessionOp& Op)
{
	const IOnlineSessionPtr Interface = Op.Interface.Pin();
	if (!Interface.IsValid())
	{
		return;
	}

	FDelegateHandle Handle = Op.Handle;
	switch (Op.Kind)
	{
		case EDLLMultiplayerSessionOpKind::Create:
			Interface->ClearOnCreateSessionCompleteDelegate_Handle(Handle);
			break;
		case EDLLMultiplayerSessionOpKind::Find:
			Interface->ClearOnFindSessionsCompleteDelegate_Handle(Handle);
			break;
		case EDLLMultiplayerSessionOpKind::Join:
			Interface->ClearOnJoinSessionCompleteDelegate_Handle(Handle);
			break;
		case EDLLMultiplayerSessionOpKind::Destroy:
			Interface->ClearOnDestroySessionCompleteDelegate_Handle(Handle);
			break;
		case EDLLMultiplayerSessionOpKind::Start:
			Interface->ClearOnStartSessionCompleteDelegate_Handle(Handle);
			break;
	}
}

void UDLLMultiplayerSubsystem::UnbindAllOps()
{
	for (const FDLLMultiplayerSessionOp& Op : InFlightOps)
	{
		UnbindOp(Op);
	}
	InFlightOps.Reset();
}

// ============================================================================
// LEAVE CONTINUATION
// ============================================================================

void UDLLMultiplayerSubsystem::LeaveThen(TUniqueFunction<void(bool)>&& Then)
{
	if (PendingAfterLeave)
	{
		UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: a newer action replaces the one queued behind leaving the session"));
	}
	PendingAfterLeave = MoveTemp(Then);

	// A destroy already under way will run the continuation when it finishes.
	if (!IsOpInFlight(EDLLMultiplayerSessionOpKind::Destroy))
	{
		DestroySession();
	}
}

void UDLLMultiplayerSubsystem::RunLeaveContinuation(bool bLeft)
{
	// Take it out first: the continuation may itself queue a new one (or start another destroy).
	TUniqueFunction<void(bool)> Then = MoveTemp(PendingAfterLeave);
	PendingAfterLeave.Reset();
	if (Then)
	{
		Then(bLeft);
	}
}

void UDLLMultiplayerSubsystem::HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId,
	FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	// Fired by the platform (Steam overlay Accept / "Join Game" on a friend / a +connect_lobby launch).
	// Null never fires it, so LAN behaviour is unchanged.
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: invite accepted but the session is not joinable (success=%d, valid=%d)"),
			bWasSuccessful ? 1 : 0, InviteResult.IsValid() ? 1 : 0);
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::SessionNotFound, FDLLMultiplayerSessionInfo());
		return;
	}

	// The engine refuses JoinSession while NAME_GameSession exists (e.g. the player's own hosted hub), so
	// leave first. LeaveThen replaces anything already queued, which is what lets the invite win over a
	// pending re-create.
	if (HasSessionInterface() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: invite accepted while in a session - leaving it, then joining"));
		TWeakObjectPtr<UDLLMultiplayerSubsystem> WeakThis(this);
		LeaveThen([WeakThis, InviteResult](bool bLeft)
		{
			UDLLMultiplayerSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			if (bLeft)
			{
				Self->CurrentSessionPassword.Empty();
				Self->JoinSearchResult(InviteResult);
				return;
			}
			Self->OnSessionError.Broadcast(TEXT("DestroyFailed"),
				TEXT("Could not leave the current session, so the invite was not joined."));
			Self->OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::AlreadyInSession, FDLLMultiplayerSessionInfo());
		});
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: invite accepted - joining"));
	CurrentSessionPassword.Empty();
	JoinSearchResult(InviteResult);
}

// ============================================================================
// CREATE-PATH HELPERS
// ============================================================================

void UDLLMultiplayerSubsystem::BroadcastCreateFailed()
{
	SetSessionState(EDLLMultiplayerSessionState::None);
	OnSessionCreated.Broadcast(false, FDLLMultiplayerSessionInfo());
}

UDLLMultiplayerSessionConfiguration* UDLLMultiplayerSubsystem::MakeTransientSessionConfiguration(
	int32 NumConnections,
	const FString& MatchType,
	EDLLMultiplayerSessionVisibility Visibility)
{
	UDLLMultiplayerSessionConfiguration* Config = NewObject<UDLLMultiplayerSessionConfiguration>(this);
	if (!Config)
	{
		return nullptr;
	}

	// ValidateConfiguration requires a display name, a match type id and at least one connection;
	// everything else keeps the data asset's own defaults.
	const FString EffectiveMatchType = MatchType.IsEmpty() ? TEXT("Campaign") : MatchType;
	Config->DisplayName = FText::FromString(EffectiveMatchType);
	Config->MatchTypeId = EffectiveMatchType;
	Config->MaxPublicConnections = FMath::Clamp(NumConnections, 1, 64);
	Config->MinPlayersToStart = 1;
	Config->DefaultVisibility = Visibility;

	return Config;
}

FOnlineSessionSettings UDLLMultiplayerSubsystem::BuildOnlineSettings(const FDLLMultiplayerSessionCreationSettings& Settings) const
{
	using namespace DLLMultiplayerSessionCore;

	// Callers have validated Settings, so Configuration is set.
	const UDLLMultiplayerSessionConfiguration* Config = Settings.Configuration.Get();
	check(Config);

	FOnlineSessionSettings Online;

	// The Null service can only do LAN, so it turns any request into a LAN session.
	const bool bLAN = Config->SessionType == EDLLMultiplayerSessionType::LAN || IsUsingLAN();
	Online.bIsLANMatch = bLAN;

	Online.NumPublicConnections = Settings.GetEffectiveMaxPublicConnections();
	Online.NumPrivateConnections = Config->MaxPrivateConnections;
	Online.bAllowJoinInProgress = Config->bAllowJoinInProgress;
	Online.bShouldAdvertise = Config->bShouldAdvertise;

	// Presence and lobbies are platform features; a LAN session has neither.
	Online.bAllowJoinViaPresence = !bLAN && Config->bAllowJoinViaPresence;
	Online.bUsesPresence = !bLAN && Config->bUsesPresence;
	Online.bUseLobbiesIfAvailable = !bLAN && Config->bUseLobbiesIfAvailable;

	// Always the engine's value, never the configuration's: the Null LAN search silently drops any result
	// whose build id differs from the searcher's own GetBuildUniqueId(), and Steam overwrites the field anyway.
	// A configured id could therefore only ever make sessions invisible.
	Online.BuildUniqueId = GetBuildUniqueId();

	const EOnlineDataAdvertisementType::Type Advertise = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
	Online.Set(KeyMatchType, Config->MatchTypeId, Advertise);
	Online.Set(KeySessionType, FString::FromInt(static_cast<int32>(Config->SessionType)), Advertise);
	Online.Set(KeyVisibility, FString::FromInt(static_cast<int32>(Settings.GetEffectiveVisibility())), Advertise);
	Online.Set(KeyGameMode, Config->DefaultGameMode, Advertise);

	const FString SessionName = Settings.GetEffectiveSessionName();
	if (!SessionName.IsEmpty())
	{
		Online.Set(KeySessionName, SessionName, Advertise);
	}

	const FSoftObjectPath Map = Settings.GetEffectiveMap();
	if (Map.IsValid())
	{
		Online.Set(KeyMapName, Map.GetAssetName(), Advertise);
	}

	if (Settings.HasPassword())
	{
		// Only the fact is advertised; the password itself stays on the host (CurrentSessionPassword).
		Online.Set(KeyHasPassword, FString(TEXT("true")), Advertise);
	}

	for (const TPair<FString, FString>& Custom : Settings.GetMergedCustomSettings())
	{
		Online.Set(FName(*Custom.Key), Custom.Value, Advertise);
	}

	return Online;
}

bool UDLLMultiplayerSubsystem::SubmitCreate(const FDLLMultiplayerSessionCreationSettings& Settings)
{
	if (!HasSessionInterface())
	{
		OnSessionError.Broadcast(TEXT("NoSessionInterface"), TEXT("No online session service is available."));
		BroadcastCreateFailed();
		return false;
	}

	FString ErrorCode;
	ULocalPlayer* Host = DLLMultiplayerSessionCore::GetSessionOwner(GetWorld(), ErrorCode);
	if (!Host)
	{
		OnSessionError.Broadcast(ErrorCode, TEXT("No local player to host the session."));
		BroadcastCreateFailed();
		return false;
	}

	const FOnlineSessionSettings OnlineSettings = BuildOnlineSettings(Settings);
	CurrentSessionPassword = Settings.HasPassword() ? Settings.Password : FString();

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: creating %s session (match '%s', %d public slots)"),
		OnlineSettings.bIsLANMatch ? TEXT("LAN") : TEXT("online"),
		*Settings.Configuration->MatchTypeId, OnlineSettings.NumPublicConnections);

	SetSessionState(EDLLMultiplayerSessionState::Creating);
	TrackOp(EDLLMultiplayerSessionOpKind::Create, SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateComplete)));

	// Prefer the player's net id; a service without an identity login has none, so fall back to the controller.
	const FUniqueNetIdRepl HostId = Host->GetPreferredUniqueNetId();
	const bool bStarted = HostId.IsValid()
		? SessionInterface->CreateSession(*HostId, NAME_GameSession, OnlineSettings)
		: SessionInterface->CreateSession(Host->GetControllerId(), NAME_GameSession, OnlineSettings);

	if (!bStarted)
	{
		OnSessionError.Broadcast(TEXT("CreateFailed"), TEXT("The online service refused to create the session."));
		// Some services (Null) fire the completion before returning false; only report if it has not.
		if (FinishOp(EDLLMultiplayerSessionOpKind::Create))
		{
			BroadcastCreateFailed();
		}
		return false;
	}
	return true;
}

void UDLLMultiplayerSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
{
	UDLLMultiplayerSessionConfiguration* Config = MakeTransientSessionConfiguration(NumPublicConnections, MatchType, EDLLMultiplayerSessionVisibility::Public);
	if (!Config)
	{
		OnSessionError.Broadcast(TEXT("InvalidConfig"), TEXT("Could not create a session configuration."));
		BroadcastCreateFailed();
		return;
	}

	// Advertise what will actually be created: on the Null service that is a LAN session.
	if (IsUsingLAN())
	{
		Config->SessionType = EDLLMultiplayerSessionType::LAN;
	}

	CreateSessionWithSettings(FDLLMultiplayerSessionCreationSettings(Config));
}

void UDLLMultiplayerSubsystem::FindSessions(int32 MaxSearchResults)
{
	FDLLMultiplayerSessionSearchFilter Filter;
	Filter.MaxResults = MaxSearchResults;
	Filter.bIncludeLAN = IsUsingLAN();
	FindSessionsFiltered(Filter);
}

void UDLLMultiplayerSubsystem::DestroySession()
{
	if (!HasSessionInterface())
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: cannot leave the session - no session interface"));
		RunLeaveContinuation(false);
		OnSessionDestroyed.Broadcast(false);
		return;
	}

	if (IsOpInFlight(EDLLMultiplayerSessionOpKind::Destroy))
	{
		UE_LOG(LogTemp, Verbose, TEXT("DLLMultiplayer: leave already in progress"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: leaving the game session"));

	const EDLLMultiplayerSessionState StateBeforeDestroy = CurrentSessionState;
	SetSessionState(EDLLMultiplayerSessionState::Destroying);
	TrackOp(EDLLMultiplayerSessionOpKind::Destroy, SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroyComplete, StateBeforeDestroy)));

	if (!SessionInterface->DestroySession(NAME_GameSession) && FinishOp(EDLLMultiplayerSessionOpKind::Destroy))
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: the online service refused to destroy the session"));
		SetSessionState(EDLLMultiplayerSessionState::None);
		RunLeaveContinuation(false);
		OnSessionDestroyed.Broadcast(false);
	}
}

void UDLLMultiplayerSubsystem::StartSession()
{
	if (!HasSessionInterface())
	{
		OnSessionStarted.Broadcast(false);
		return;
	}

	if (IsOpInFlight(EDLLMultiplayerSessionOpKind::Start))
	{
		UE_LOG(LogTemp, Verbose, TEXT("DLLMultiplayer: session start already in progress"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: starting the game session"));

	SetSessionState(EDLLMultiplayerSessionState::Starting);
	TrackOp(EDLLMultiplayerSessionOpKind::Start, SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleStartComplete)));

	if (!SessionInterface->StartSession(NAME_GameSession) && FinishOp(EDLLMultiplayerSessionOpKind::Start))
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: the online service refused to start the session"));
		SetSessionState(EDLLMultiplayerSessionState::Lobby);
		OnSessionStarted.Broadcast(false);
	}
}

// ============================================================================
// SESSION CREATION
// ============================================================================

bool UDLLMultiplayerSubsystem::CreateSessionWithConfig(UDLLMultiplayerSessionConfiguration* Config)
{
	if (!Config)
	{
		OnSessionError.Broadcast(TEXT("InvalidConfig"), TEXT("Session configuration is null."));
		BroadcastCreateFailed();
		return false;
	}

	FDLLMultiplayerSessionCreationSettings Settings(Config);
	return CreateSessionWithSettings(Settings);
}

bool UDLLMultiplayerSubsystem::CreateSessionWithSettings(const FDLLMultiplayerSessionCreationSettings& Settings)
{
	FString Error;
	if (!Settings.IsValid(Error))
	{
		OnSessionError.Broadcast(TEXT("InvalidSettings"), Error);
		BroadcastCreateFailed();
		return false;
	}

	if (Settings.HasPassword() && !DLLMultiplayerSessionCore::IsTravelSafePassword(Settings.Password))
	{
		OnSessionError.Broadcast(TEXT("InvalidPassword"), TEXT("A session password cannot contain spaces or ? = # & (it travels in the connect URL)."));
		BroadcastCreateFailed();
		return false;
	}

	if (!HasSessionInterface())
	{
		OnSessionError.Broadcast(TEXT("NoSessionInterface"), TEXT("No online session service is available."));
		BroadcastCreateFailed();
		return false;
	}

	FString ErrorCode;
	if (!DLLMultiplayerSessionCore::GetSessionOwner(GetWorld(), ErrorCode))
	{
		OnSessionError.Broadcast(ErrorCode, TEXT("No local player to host the session."));
		BroadcastCreateFailed();
		return false;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return SubmitCreate(Settings);
	}

	// Already in a session: leave it, then create. A transient configuration has nothing else holding it,
	// so the continuation keeps it alive across an asynchronous destroy.
	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: a session already exists - leaving it before creating the new one"));
	TWeakObjectPtr<UDLLMultiplayerSubsystem> WeakThis(this);
	TStrongObjectPtr<UDLLMultiplayerSessionConfiguration> KeepConfigAlive(Settings.Configuration.Get());
	LeaveThen([WeakThis, Settings, KeepConfigAlive = MoveTemp(KeepConfigAlive)](bool bLeft)
	{
		UDLLMultiplayerSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (!bLeft)
		{
			Self->OnSessionError.Broadcast(TEXT("DestroyFailed"),
				TEXT("Could not leave the previous session, so the new one was not created."));
			Self->BroadcastCreateFailed();
			return;
		}
		Self->SubmitCreate(Settings);
	});
	return true;
}

bool UDLLMultiplayerSubsystem::CreatePrivateSession(UDLLMultiplayerSessionConfiguration* Config, const FString& Password)
{
	// A null Config is the common case from a menu button ("host a private game") that holds no
	// configuration data asset. Fall back to a transient one rather than failing: before this, the
	// private path returned false every time and HostSessionAndTravelToLevel(bPrivate) never hosted.
	if (!Config)
	{
		Config = MakeTransientSessionConfiguration(4, TEXT("Campaign"), EDLLMultiplayerSessionVisibility::Private);
	}

	if (!Config)
	{
		OnSessionError.Broadcast(TEXT("InvalidConfig"), TEXT("Session configuration is null."));
		BroadcastCreateFailed();
		return false;
	}

	FDLLMultiplayerSessionCreationSettings Settings(Config);
	Settings.VisibilityOverride = EDLLMultiplayerSessionVisibility::Private;
	Settings.Password = Password;

	return CreateSessionWithSettings(Settings);
}

bool UDLLMultiplayerSubsystem::CreateLANSession(int32 NumConnections, const FString& SessionName)
{
	UDLLMultiplayerSessionConfiguration* Config = MakeTransientSessionConfiguration(NumConnections, SessionName, EDLLMultiplayerSessionVisibility::Public);
	if (!Config)
	{
		OnSessionError.Broadcast(TEXT("InvalidConfig"), TEXT("Could not create a session configuration."));
		BroadcastCreateFailed();
		return false;
	}
	Config->SessionType = EDLLMultiplayerSessionType::LAN;

	FDLLMultiplayerSessionCreationSettings Settings(Config);
	Settings.SessionName = SessionName;
	return CreateSessionWithSettings(Settings);
}

// ============================================================================
// DISCOVERY
// ============================================================================

void UDLLMultiplayerSubsystem::FindSessionsFiltered(const FDLLMultiplayerSessionSearchFilter& Filter)
{
	LastSearchFilter = Filter;

	if (bIsSearching || IsOpInFlight(EDLLMultiplayerSessionOpKind::Find))
	{
		UE_LOG(LogTemp, Verbose, TEXT("DLLMultiplayer: search already running - request ignored"));
		return;
	}

	FString ErrorCode;
	ULocalPlayer* Searcher = HasSessionInterface() ? DLLMultiplayerSessionCore::GetSessionOwner(GetWorld(), ErrorCode) : nullptr;
	if (!Searcher)
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: cannot search (%s)"),
			ErrorCode.IsEmpty() ? TEXT("NoSessionInterface") : *ErrorCode);
		OnSessionsFound.Broadcast(TArray<FDLLMultiplayerSessionInfo>(), false);
		return;
	}

	ActiveSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSearch->MaxSearchResults = FMath::Max(1, Filter.MaxResults);
	ActiveSearch->bIsLanQuery = Filter.bIncludeLAN;
	if (!Filter.bIncludeLAN)
	{
		// Hosts create presence lobbies online; without this flag Steam searches game servers instead.
		ActiveSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	if (!Filter.MatchType.IsEmpty())
	{
		ActiveSearch->QuerySettings.Set(DLLMultiplayerSessionCore::KeyMatchType, Filter.MatchType, EOnlineComparisonOp::Equals);
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: searching %s sessions (max %d, match '%s')"),
		Filter.bIncludeLAN ? TEXT("LAN") : TEXT("online"), ActiveSearch->MaxSearchResults, *Filter.MatchType);

	bIsSearching = true;
	TrackOp(EDLLMultiplayerSessionOpKind::Find, SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindComplete)));

	const TSharedRef<FOnlineSessionSearch> SearchRef = ActiveSearch.ToSharedRef();
	const FUniqueNetIdRepl SearcherId = Searcher->GetPreferredUniqueNetId();
	const bool bStarted = SearcherId.IsValid()
		? SessionInterface->FindSessions(*SearcherId, SearchRef)
		: SessionInterface->FindSessions(Searcher->GetControllerId(), SearchRef);

	if (!bStarted && FinishOp(EDLLMultiplayerSessionOpKind::Find))
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: the online service refused to search"));
		bIsSearching = false;
		ActiveSearch.Reset();
		OnSessionsFound.Broadcast(TArray<FDLLMultiplayerSessionInfo>(), false);
	}
}

void UDLLMultiplayerSubsystem::RefreshSessionList()
{
	FindSessionsFiltered(LastSearchFilter);
}

// ============================================================================
// JOINING
// ============================================================================

void UDLLMultiplayerSubsystem::JoinSessionByInfo(const FDLLMultiplayerSessionInfo& SessionInfo, const FString& Password)
{
	if (!SessionInfo.IsValid())
	{
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::SessionNotFound, FDLLMultiplayerSessionInfo());
		return;
	}

	// Check password requirement
	if (SessionInfo.bHasPassword && Password.IsEmpty())
	{
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::PasswordRequired, FDLLMultiplayerSessionInfo());
		return;
	}

	// Store password for potential validation
	CurrentSessionPassword = Password;

	JoinSearchResult(SessionInfo.SearchResult);
}

void UDLLMultiplayerSubsystem::JoinSearchResult(const FOnlineSessionSearchResult& SearchResult)
{
	FString ErrorCode;
	ULocalPlayer* Joiner = HasSessionInterface() ? DLLMultiplayerSessionCore::GetSessionOwner(GetWorld(), ErrorCode) : nullptr;
	if (!Joiner)
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: cannot join (%s)"),
			ErrorCode.IsEmpty() ? TEXT("NoSessionInterface") : *ErrorCode);
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::Unknown, FDLLMultiplayerSessionInfo());
		return;
	}

	if (!SearchResult.IsValid())
	{
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::SessionNotFound, FDLLMultiplayerSessionInfo());
		return;
	}

	if (IsOpInFlight(EDLLMultiplayerSessionOpKind::Join))
	{
		// Not answered on OnSessionJoined: that event belongs to the join already running.
		OnSessionError.Broadcast(TEXT("JoinInProgress"), TEXT("Already joining a session."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: joining session hosted by '%s'"), *SearchResult.Session.OwningUserName);

	TrackOp(EDLLMultiplayerSessionOpKind::Join, SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinComplete)));

	const FUniqueNetIdRepl JoinerId = Joiner->GetPreferredUniqueNetId();
	const bool bStarted = JoinerId.IsValid()
		? SessionInterface->JoinSession(*JoinerId, NAME_GameSession, SearchResult)
		: SessionInterface->JoinSession(Joiner->GetControllerId(), NAME_GameSession, SearchResult);

	if (!bStarted && FinishOp(EDLLMultiplayerSessionOpKind::Join))
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: the online service refused to join"));
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::Failed, FDLLMultiplayerSessionInfo());
	}
}

void UDLLMultiplayerSubsystem::JoinSessionByIndex(int32 Index, const FString& Password)
{
	if (Index < 0 || Index >= CachedSessions.Num())
	{
		OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::SessionNotFound, FDLLMultiplayerSessionInfo());
		return;
	}

	JoinSessionByInfo(CachedSessions[Index], Password);
}

// ============================================================================
// SESSION STATE
// ============================================================================

FDLLMultiplayerSessionInfo UDLLMultiplayerSubsystem::GetCurrentSessionInfo() const
{
	return CurrentSession;
}

bool UDLLMultiplayerSubsystem::IsInSession() const
{
	const bool bStateSaysInSession = CurrentSessionState != EDLLMultiplayerSessionState::None &&
		CurrentSessionState != EDLLMultiplayerSessionState::Creating &&
		CurrentSessionState != EDLLMultiplayerSessionState::Destroying;
	if (!bStateSaysInSession)
	{
		return false;
	}

	// The OSS is the truth: the cached state can outlive a session destroyed behind our back (see the header).
	return !SessionInterface.IsValid() || SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

bool UDLLMultiplayerSubsystem::ReconcileSessionState()
{
	const bool bStateSaysInSession = CurrentSessionState != EDLLMultiplayerSessionState::None &&
		CurrentSessionState != EDLLMultiplayerSessionState::Creating &&
		CurrentSessionState != EDLLMultiplayerSessionState::Destroying;
	if (!bStateSaysInSession || IsInSession())
	{
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: state said '%s' but the online subsystem has no session - it was destroyed outside this plugin (e.g. a menu's Destroy Session node). Resetting to None."),
		*UEnum::GetValueAsString(CurrentSessionState));
	CurrentSession = FDLLMultiplayerSessionInfo();
	SetSessionState(EDLLMultiplayerSessionState::None);
	return true;
}

bool UDLLMultiplayerSubsystem::IsSessionHost() const
{
	if (!SessionInterface.IsValid())
	{
		return false;
	}

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	return Session && Session->bHosting;
}

bool UDLLMultiplayerSubsystem::UpdateMatchSettings(const FDLLMultiplayerMatchSettings& NewSettings)
{
	if (!IsSessionHost())
	{
		OnSessionError.Broadcast(TEXT("NotHost"), TEXT("Only the host can update match settings."));
		return false;
	}

	if (CurrentSessionState != EDLLMultiplayerSessionState::Lobby)
	{
		OnSessionError.Broadcast(TEXT("WrongState"), TEXT("Match settings can only be updated in the lobby."));
		return false;
	}

	FString ValidationError;
	if (!NewSettings.Validate(ValidationError))
	{
		OnSessionError.Broadcast(TEXT("InvalidSettings"), ValidationError);
		return false;
	}

	CurrentMatchSettings = NewSettings;

	// Update session settings with match settings
	if (SessionInterface.IsValid())
	{
		FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(NAME_GameSession);
		if (Settings)
		{
			Settings->Set(FName("MatchSettings"), NewSettings.SerializeToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
			SessionInterface->UpdateSession(NAME_GameSession, *Settings, true);
		}
	}

	return true;
}

// ============================================================================
// LOCAL PLAYER REGISTRATION (SPLIT-SCREEN)
// ============================================================================

int32 UDLLMultiplayerSubsystem::RegisterLocalPlayer(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return -1;
	}

	// Check if already registered
	for (int32 i = 0; i < RegisteredLocalPlayers.Num(); ++i)
	{
		if (RegisteredLocalPlayers[i].Get() == PlayerController)
		{
			return i;
		}
	}

	// Same limit as the local-player library (DefaultGame.ini [/Script/DLLMultiplayerRuntime.LocalPlayers]).
	const int32 MaxLocalPlayers = UDLLMultiplayerLocalPlayerLibrary::GetMaxLocalPlayers();
	if (RegisteredLocalPlayers.Num() >= MaxLocalPlayers)
	{
		OnSessionError.Broadcast(TEXT("MaxLocalPlayers"),
			FString::Printf(TEXT("Maximum of %d local players already registered."), MaxLocalPlayers));
		return -1;
	}

	int32 Index = RegisteredLocalPlayers.Num();
	RegisteredLocalPlayers.Add(PlayerController);

	OnLocalPlayerRegistered.Broadcast(Index, PlayerController);

	return Index;
}

FString UDLLMultiplayerSubsystem::GetLocalPlayerNickname(int32 LocalPlayerIndex) const
{
	UWorld* World = GetWorld();
	if (World)
	{
		// 1) Try the online identity nickname (Steam/EOS account name)
		if (IOnlineSubsystem* OSS = GetWorldOnlineSubsystem())
		{
			if (IOnlineIdentityPtr Identity = OSS->GetIdentityInterface())
			{
				FString Nick = Identity->GetPlayerNickname(LocalPlayerIndex);
				if (!Nick.IsEmpty())
				{
					return Nick;
				}
			}
		}

		// 2) Try the PlayerState name from the local player's controller
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, LocalPlayerIndex))
		{
			if (APlayerState* PS = PC->PlayerState)
			{
				const FString PSName = PS->GetPlayerName();
				if (!PSName.IsEmpty())
				{
					return PSName;
				}
			}

			// 3) Try the ULocalPlayer nickname
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				const FString LPName = LP->GetNickname();
				if (!LPName.IsEmpty())
				{
					return LPName;
				}
			}
		}
	}

	// 4) Fallback so the lobby never shows a blank row
	return FString::Printf(TEXT("Player %d"), LocalPlayerIndex + 1);
}

void UDLLMultiplayerSubsystem::UnregisterLocalPlayer(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	for (int32 i = 0; i < RegisteredLocalPlayers.Num(); ++i)
	{
		if (RegisteredLocalPlayers[i].Get() == PlayerController)
		{
			RegisteredLocalPlayers.RemoveAt(i);
			OnLocalPlayerUnregistered.Broadcast(i);
			return;
		}
	}
}

TArray<APlayerController*> UDLLMultiplayerSubsystem::GetRegisteredLocalPlayers() const
{
	TArray<APlayerController*> Result;
	for (const TWeakObjectPtr<APlayerController>& WeakPC : RegisteredLocalPlayers)
	{
		if (APlayerController* PC = WeakPC.Get())
		{
			Result.Add(PC);
		}
	}
	return Result;
}

// ============================================================================
// PLAYER MANAGEMENT
// ============================================================================

bool UDLLMultiplayerSubsystem::KickPlayer(const FUniqueNetIdRepl& PlayerId)
{
	if (!IsSessionHost())
	{
		OnSessionError.Broadcast(TEXT("NotHost"), TEXT("Only the host can kick players."));
		return false;
	}

	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (PlayerId.IsValid() && GameMode && GameMode->GameSession)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (!PlayerController || PlayerController->IsLocalController() || !PlayerController->PlayerState
				|| !(PlayerController->PlayerState->GetUniqueId() == PlayerId))
			{
				continue;
			}
			// Recorded first: the logout that the kick causes reports Kicked instead of Voluntary.
			PendingKicks.Add(PlayerId);
			if (GameMode->GameSession->KickPlayer(PlayerController, FText::FromString(TEXT("Removed from the session by the host."))))
			{
				// The kick destroys the controller synchronously, so its logout has already consumed the entry; remove it
				// anyway so a logout that never matched cannot turn a later ordinary leave into "Kicked".
				PendingKicks.RemoveSingle(PlayerId);
				UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: kicked %s"), *PlayerController->PlayerState->GetPlayerName());
				return true;
			}
			PendingKicks.RemoveSingle(PlayerId);
			break;
		}
	}

	OnSessionError.Broadcast(TEXT("KickFailed"), TEXT("No connected remote player with that id could be removed."));
	return false;
}

void UDLLMultiplayerSubsystem::SetLocalPlayerReady(int32 LocalPlayerIndex, bool bIsReady)
{
	if (LocalPlayerIndex < 0)
	{
		return;
	}

	// Find the player in session players
	for (FDLLMultiplayerSessionPlayer& Player : SessionPlayers)
	{
		if (Player.bIsLocalPlayer && Player.LocalPlayerIndex == LocalPlayerIndex)
		{
			if (Player.bIsReady != bIsReady)
			{
				Player.bIsReady = bIsReady;
				OnPlayerReadyStateChanged.Broadcast(Player, bIsReady);

				// Check if all players are ready
				if (AreAllPlayersReady())
				{
					OnAllPlayersReady.Broadcast();
				}
			}
			return;
		}
	}
}

bool UDLLMultiplayerSubsystem::AreAllPlayersReady() const
{
	if (SessionPlayers.Num() == 0)
	{
		return false;
	}

	for (const FDLLMultiplayerSessionPlayer& Player : SessionPlayers)
	{
		if (!Player.bIsReady && !Player.bIsHost)
		{
			return false;
		}
	}
	return true;
}

// ============================================================================
// INVITES
// ============================================================================

void UDLLMultiplayerSubsystem::SendInvite(const FUniqueNetIdRepl& FriendId)
{
	if (!IsInSession())
	{
		OnSessionError.Broadcast(TEXT("NotInSession"), TEXT("Must be in a session to send invites."));
		OnInviteSent.Broadcast(false, FriendId);
		return;
	}

	// The online service delivers the invite (Steam, EOS...). Services without invites, such as Null for LAN,
	// refuse, and that is reported as a failure rather than a success that never arrives.
	const bool bSent = FriendId.IsValid() && HasSessionInterface()
		&& SessionInterface->SendSessionInviteToFriend(0, NAME_GameSession, *FriendId.GetUniqueNetId());
	if (!bSent)
	{
		OnSessionError.Broadcast(TEXT("InviteFailed"), TEXT("The online service did not send the invite."));
	}
	OnInviteSent.Broadcast(bSent, FriendId);
}

void UDLLMultiplayerSubsystem::AcceptInvite(const FDLLMultiplayerGameInvite& Invite)
{
	if (!Invite.bIsValid)
	{
		OnSessionError.Broadcast(TEXT("InvalidInvite"), TEXT("Invite is no longer valid."));
		return;
	}

	JoinSessionByInfo(Invite.SessionInfo);
}

void UDLLMultiplayerSubsystem::DeclineInvite(const FDLLMultiplayerGameInvite& Invite)
{
	// Just log - no action needed
	UE_LOG(LogTemp, Log, TEXT("Invite declined from %s"), *Invite.FromPlayer.DisplayName);
}

// ============================================================================
// PLATFORM INFO
// ============================================================================

EDLLMultiplayerOnlineServiceType UDLLMultiplayerSubsystem::GetCurrentServiceType() const
{
	IOnlineSubsystem* Subsystem = GetWorldOnlineSubsystem();
	if (!Subsystem)
	{
		return EDLLMultiplayerOnlineServiceType::LAN;
	}

	FString SubsystemName = Subsystem->GetSubsystemName().ToString();

	if (SubsystemName == TEXT("Steam"))
	{
		return EDLLMultiplayerOnlineServiceType::Steam;
	}
	else if (SubsystemName == TEXT("EOS") || SubsystemName == TEXT("EOSPlus"))
	{
		return EDLLMultiplayerOnlineServiceType::EOS;
	}
	else if (SubsystemName == TEXT("PS4") || SubsystemName == TEXT("PS5"))
	{
		return EDLLMultiplayerOnlineServiceType::PlayStation;
	}
	else if (SubsystemName == TEXT("Live") || SubsystemName == TEXT("GDK"))
	{
		return EDLLMultiplayerOnlineServiceType::Xbox;
	}
	else if (SubsystemName == TEXT("Switch"))
	{
		return EDLLMultiplayerOnlineServiceType::Nintendo;
	}
	else if (SubsystemName == TEXT("NULL"))
	{
		return EDLLMultiplayerOnlineServiceType::LAN;
	}

	return EDLLMultiplayerOnlineServiceType::Auto;
}

FString UDLLMultiplayerSubsystem::GetServiceName() const
{
	IOnlineSubsystem* Subsystem = GetWorldOnlineSubsystem();
	if (Subsystem)
	{
		return Subsystem->GetSubsystemName().ToString();
	}
	return TEXT("None");
}

bool UDLLMultiplayerSubsystem::IsUsingLAN() const
{
	IOnlineSubsystem* Subsystem = GetWorldOnlineSubsystem();
	return Subsystem && Subsystem->GetSubsystemName() == "NULL";
}

// ============================================================================
// ONLINE SERVICE COMPLETIONS
// ============================================================================
// Each one first ignores other session names (a party or voice session on the same interface), then claims
// its operation with FinishOp. A false return means the result was already reported (the immediate-failure
// path in the starting function got there first), so it is not reported twice.

void UDLLMultiplayerSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || !FinishOp(EDLLMultiplayerSessionOpKind::Create))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: create finished (%s)"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (!bWasSuccessful)
	{
		SetSessionState(EDLLMultiplayerSessionState::None);
		OnSessionCreated.Broadcast(false, CurrentSession);
		return;
	}

	SetSessionState(EDLLMultiplayerSessionState::Lobby);
	UpdateSessionInfo();

	FDLLMultiplayerSessionPlayer HostPlayer;
	HostPlayer.DisplayName = GetLocalPlayerNickname(0);
	HostPlayer.bIsHost = true;
	HostPlayer.bIsLocalPlayer = true;
	HostPlayer.LocalPlayerIndex = 0;
	HostPlayer.bIsReady = true;
	FString IgnoredErrorCode;
	if (ULocalPlayer* Host = DLLMultiplayerSessionCore::GetSessionOwner(GetWorld(), IgnoredErrorCode))
	{
		HostPlayer.UniqueNetId = Host->GetPreferredUniqueNetId();
	}
	SessionPlayers.Reset();
	SessionPlayers.Add(HostPlayer);

	OnSessionCreated.Broadcast(true, CurrentSession);
}

void UDLLMultiplayerSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	if (!FinishOp(EDLLMultiplayerSessionOpKind::Find))
	{
		return;
	}

	bIsSearching = false;
	CachedSessions.Reset();

	int32 RawCount = 0;
	if (ActiveSearch.IsValid())
	{
		RawCount = ActiveSearch->SearchResults.Num();
		for (const FOnlineSessionSearchResult& Result : ActiveSearch->SearchResults)
		{
			if (!Result.IsValid())
			{
				continue;
			}
			FDLLMultiplayerSessionInfo Info = FDLLMultiplayerSessionInfo::FromSearchResult(Result);
			if (LastSearchFilter.Matches(Info))
			{
				CachedSessions.Add(MoveTemp(Info));
			}
		}
	}
	ActiveSearch.Reset();

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: search finished (%s) - %d raw results, %d after filtering"),
		bWasSuccessful ? TEXT("success") : TEXT("failed"), RawCount, CachedSessions.Num());

	OnSessionsFound.Broadcast(CachedSessions, bWasSuccessful);
}

void UDLLMultiplayerSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession || !FinishOp(EDLLMultiplayerSessionOpKind::Join))
	{
		return;
	}

	const EDLLMultiplayerJoinSessionResult JoinResult = ConvertJoinResult(Result);
	const bool bJoined = JoinResult == EDLLMultiplayerJoinSessionResult::Success;

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: join finished (%s)"), *UDLLMultiplayerSessionTypeHelpers::GetJoinResultDisplayName(JoinResult));

	if (bJoined)
	{
		SetSessionState(EDLLMultiplayerSessionState::Lobby);
		UpdateSessionInfo();

		// Joining the online session only registers us with it; the network connection to the host is a
		// separate client travel to the address the service resolves for that session.
		FString HostAddress;
		const UGameInstance* GameInstance = GetGameInstance();
		APlayerController* LocalController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
		if (LocalController && SessionInterface.IsValid()
			&& SessionInterface->GetResolvedConnectString(NAME_GameSession, HostAddress) && !HostAddress.IsEmpty())
		{
			// The host checks the password at login (ADLLMultiplayerGameSession::ApproveLogin).
			const FString TravelURL = CurrentSessionPassword.IsEmpty()
				? HostAddress
				: FString::Printf(TEXT("%s?Password=%s"), *HostAddress, *CurrentSessionPassword);
			LocalController->ClientTravel(TravelURL, TRAVEL_Absolute);
		}
		else
		{
			OnSessionError.Broadcast(TEXT("JoinTravelFailed"), TEXT("Joined session, but could not resolve the host travel address."));
		}
	}

	OnSessionJoined.Broadcast(JoinResult, bJoined ? CurrentSession : FDLLMultiplayerSessionInfo());
}

void UDLLMultiplayerSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful, EDLLMultiplayerSessionState StateBeforeDestroy)
{
	if (SessionName != NAME_GameSession || !FinishOp(EDLLMultiplayerSessionOpKind::Destroy))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: leave finished (%s)"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (bWasSuccessful)
	{
		SetSessionState(EDLLMultiplayerSessionState::None);
		CurrentSession = FDLLMultiplayerSessionInfo();
		SessionPlayers.Empty();
		PendingKicks.Reset();
		CurrentMatchSettings = FDLLMultiplayerMatchSettings();
	}
	else
	{
		// Still in the session as far as we know; do not leave the state stuck on Destroying.
		SetSessionState(StateBeforeDestroy);
	}

	RunLeaveContinuation(bWasSuccessful);
	OnSessionDestroyed.Broadcast(bWasSuccessful);
}

void UDLLMultiplayerSubsystem::HandleStartComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || !FinishOp(EDLLMultiplayerSessionOpKind::Start))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: start finished (%s)"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	SetSessionState(bWasSuccessful ? EDLLMultiplayerSessionState::InProgress : EDLLMultiplayerSessionState::Lobby);
	OnSessionStarted.Broadcast(bWasSuccessful);
}

// ============================================================================
// INTERNAL STATE MANAGEMENT
// ============================================================================

void UDLLMultiplayerSubsystem::SetSessionState(EDLLMultiplayerSessionState NewState)
{
	if (CurrentSessionState != NewState)
	{
		EDLLMultiplayerSessionState OldState = CurrentSessionState;
		CurrentSessionState = NewState;
		OnSessionStateChanged.Broadcast(NewState, OldState);
	}
}

void UDLLMultiplayerSubsystem::UpdateSessionInfo()
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session)
	{
		return;
	}

	BuildSessionInfoFromSettings(Session->SessionSettings, CurrentSession);
	CurrentSession.State = CurrentSessionState;

	// Update player counts
	CurrentSession.MaxPlayers = Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections;
	CurrentSession.CurrentPlayers = CurrentSession.MaxPlayers - Session->NumOpenPublicConnections - Session->NumOpenPrivateConnections;
}

void UDLLMultiplayerSubsystem::BuildSessionInfoFromSettings(const FOnlineSessionSettings& Settings, FDLLMultiplayerSessionInfo& OutInfo) const
{
	OutInfo.MaxPlayers = Settings.NumPublicConnections + Settings.NumPrivateConnections;
	OutInfo.bIsLAN = Settings.bIsLANMatch;
	OutInfo.bIsDedicatedServer = Settings.bIsDedicated;
	OutInfo.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
	OutInfo.BuildUniqueId = Settings.BuildUniqueId;

	FString Value;
	if (Settings.Get(FName("MatchType"), Value))
	{
		OutInfo.MatchType = Value;
	}
	if (Settings.Get(FName("SessionName"), Value))
	{
		OutInfo.SessionName = Value;
	}
	if (Settings.Get(FName("MapName"), Value))
	{
		OutInfo.MapName = Value;
	}
	if (Settings.Get(FName("GameMode"), Value))
	{
		OutInfo.GameMode = Value;
	}
	if (Settings.Get(FName("HasPassword"), Value))
	{
		OutInfo.bHasPassword = Value.ToBool();
	}
	if (Settings.Get(FName("SessionType"), Value))
	{
		OutInfo.SessionType = static_cast<EDLLMultiplayerSessionType>(FCString::Atoi(*Value));
	}
	if (Settings.Get(FName("Visibility"), Value))
	{
		OutInfo.Visibility = static_cast<EDLLMultiplayerSessionVisibility>(FCString::Atoi(*Value));
	}
}

EDLLMultiplayerJoinSessionResult UDLLMultiplayerSubsystem::ConvertJoinResult(EOnJoinSessionCompleteResult::Type Result) const
{
	switch (Result)
	{
		case EOnJoinSessionCompleteResult::Success:
			return EDLLMultiplayerJoinSessionResult::Success;
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return EDLLMultiplayerJoinSessionResult::SessionFull;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return EDLLMultiplayerJoinSessionResult::SessionNotFound;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return EDLLMultiplayerJoinSessionResult::AlreadyInSession;
		default:
			return EDLLMultiplayerJoinSessionResult::Unknown;
	}
}

// ============================================================================
// DEBUG
// ============================================================================

void UDLLMultiplayerSubsystem::DrawDebugInfo()
{
	if (!bEnableVisualDebug || !GEngine)
	{
		return;
	}

	int32 Key = DebugMessageKey;
	const float Duration = 0.0f;

	if (bDebugShowSessionState)
	{
		GEngine->AddOnScreenDebugMessage(Key++, Duration, FColor::Cyan, TEXT("=== MULTIPLAYER SESSION DEBUG ==="));
		GEngine->AddOnScreenDebugMessage(Key++, Duration, FColor::White,
			FString::Printf(TEXT("Service: %s | State: %s"),
				*GetServiceName(),
				*UDLLMultiplayerSessionTypeHelpers::GetSessionStateDisplayName(CurrentSessionState)));

		if (IsInSession())
		{
			GEngine->AddOnScreenDebugMessage(Key++, Duration, FColor::Green,
				FString::Printf(TEXT("Session: %s | Players: %d/%d"),
					*CurrentSession.SessionName,
					CurrentSession.CurrentPlayers,
					CurrentSession.MaxPlayers));

			GEngine->AddOnScreenDebugMessage(Key++, Duration, FColor::Yellow,
				FString::Printf(TEXT("Map: %s | Mode: %s | Host: %s"),
					*CurrentSession.MapName,
					*CurrentSession.GameMode,
					IsSessionHost() ? TEXT("Yes") : TEXT("No")));
		}

		GEngine->AddOnScreenDebugMessage(Key++, Duration, FColor::White,
			FString::Printf(TEXT("Local Players: %d | Searching: %s"),
				GetLocalPlayerCount(),
				bIsSearching ? TEXT("Yes") : TEXT("No")));
	}

	if (bDebugShowPlayerList && SessionPlayers.Num() > 0)
	{
		GEngine->AddOnScreenDebugMessage(Key++, Duration, FColor::Cyan, TEXT("--- Players ---"));
		for (const FDLLMultiplayerSessionPlayer& Player : SessionPlayers)
		{
			FColor PlayerColor = Player.bIsHost ? FColor::Yellow : (Player.bIsLocalPlayer ? FColor::Green : FColor::White);
			FString ReadyStatus = Player.bIsReady ? TEXT("[Ready]") : TEXT("[Not Ready]");
			FString HostStatus = Player.bIsHost ? TEXT("[Host]") : TEXT("");

			GEngine->AddOnScreenDebugMessage(Key++, Duration, PlayerColor,
				FString::Printf(TEXT("  %s %s %s Ping:%dms"),
					*Player.DisplayName,
					*HostStatus,
					*ReadyStatus,
					Player.PingMs));
		}
	}
}

// ============================================================================
// HOST-A-LEVEL
// ============================================================================
// Moved here from the game module (2026-08-09). It contained no game-specific types, and this is
// the co-op mechanic every project rebuilds: open the hub you are standing in to friends. The game
// keeps its travel-time behaviour through OnPreHostTravel rather than by owning the flow.
// ============================================================================

bool UDLLMultiplayerSubsystem::HostSessionAndTravelToLevel(
	const FString& LevelPath,
	int32 NumConnections,
	const FString& MatchType,
	bool bLAN,
	bool bPrivate)
{
	if (LevelPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DLLMultiplayer] HostSessionAndTravelToLevel - LevelPath is empty."));
		return false;
	}

	if (IsInSession())
	{
		// Refused rather than restarted: reopening the level here would drop everyone already
		// connected, and a host pressing the button twice must not evict their own party.
		UE_LOG(LogTemp, Warning,
			TEXT("[DLLMultiplayer] HostSessionAndTravelToLevel - already in a session. Leave it first."));
		return false;
	}

	PendingHostLevelPath = LevelPath;

	// One-shot; removed in the handler so a later host cannot fire this twice.
	OnSessionCreated.AddDynamic(this, &UDLLMultiplayerSubsystem::HandleHostForLevelCreated);

	UE_LOG(LogTemp, Log, TEXT("[DLLMultiplayer] Hosting '%s' as a %s session (%d slots, match '%s')."),
		*LevelPath, bLAN ? TEXT("LAN") : (bPrivate ? TEXT("private") : TEXT("online")),
		NumConnections, *MatchType);

	bool bStarted = false;
	if (bLAN)
	{
		bStarted = CreateLANSession(NumConnections, MatchType);
	}
	else if (bPrivate)
	{
		// Pass the caller's real slot count and match type; CreatePrivateSession's own null fallback
		// only knows a fixed 4-slot "Campaign" default.
		bStarted = CreatePrivateSession(
			MakeTransientSessionConfiguration(NumConnections, MatchType, EDLLMultiplayerSessionVisibility::Private),
			TEXT(""));
	}
	else
	{
		CreateSession(NumConnections, MatchType);
		bStarted = true;
	}

	if (!bStarted)
	{
		// Unbind immediately: a creation that never started will never call back, and leaving the
		// binding would make the NEXT unrelated session creation travel this level.
		OnSessionCreated.RemoveDynamic(this, &UDLLMultiplayerSubsystem::HandleHostForLevelCreated);
		PendingHostLevelPath.Reset();
		UE_LOG(LogTemp, Warning, TEXT("[DLLMultiplayer] Session creation did not start; not travelling."));
	}

	return bStarted;
}

void UDLLMultiplayerSubsystem::HandleHostForLevelCreated(bool bWasSuccessful, const FDLLMultiplayerSessionInfo& SessionInfo)
{
	OnSessionCreated.RemoveDynamic(this, &UDLLMultiplayerSubsystem::HandleHostForLevelCreated);

	const FString LevelPath = PendingHostLevelPath;
	PendingHostLevelPath.Reset();

	if (!bWasSuccessful)
	{
		// Loud: from the player's side this is a button that did nothing, and travelling anyway
		// would produce a level that looks hosted and accepts nobody.
		UE_LOG(LogTemp, Error, TEXT("[DLLMultiplayer] Session creation FAILED - staying put."));
		return;
	}

	if (LevelPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[DLLMultiplayer] Session created but no pending level to host."));
		return;
	}

	// The project's own transition work (loading screens, menu hooks) happens here, synchronously,
	// because the very next call tears this world down.
	OnPreHostTravel.Broadcast(LevelPath);

	// IsInSession() is true now, so TravelToLevel takes its ServerTravel branch even from Standalone
	// and re-states `?listen` - which is what makes the level joinable rather than merely reloaded.
	const EDLLMultiplayerTravelMethod Method = UDLLMultiplayerTravelLibrary::TravelToLevel(this, LevelPath, FString(), IsInSession());

	UE_LOG(LogTemp, Log, TEXT("[DLLMultiplayer] Hosting '%s' (travel method %d)."),
		*LevelPath, static_cast<int32>(Method));
}

// ============================================================================
// PRIVATE SESSIONS, PASSWORDS, ROSTER, INVITES (2026-10-18)
// ============================================================================

bool UDLLMultiplayerSubsystem::CreateQuickPrivateSession(int32 NumConnections, const FString& MatchType, const FString& Password)
{
	return CreatePrivateSession(
		MakeTransientSessionConfiguration(FMath::Max(1, NumConnections), MatchType, EDLLMultiplayerSessionVisibility::Private),
		Password);
}

bool UDLLMultiplayerSubsystem::IsSessionPasswordAccepted(const FString& Supplied) const
{
	if (!IsSessionHost() || CurrentSessionPassword.IsEmpty())
	{
		return true;
	}
	return Supplied.Equals(CurrentSessionPassword, ESearchCase::CaseSensitive);
}

FDLLMultiplayerSessionPlayer UDLLMultiplayerSubsystem::MakeSessionPlayer(const APlayerController* PlayerController) const
{
	FDLLMultiplayerSessionPlayer Player;
	if (!PlayerController)
	{
		return Player;
	}
	if (const APlayerState* State = PlayerController->PlayerState)
	{
		Player.DisplayName = State->GetPlayerName();
		Player.UniqueNetId = State->GetUniqueId();
	}
	Player.bIsLocalPlayer = PlayerController->IsLocalController();
	if (Player.bIsLocalPlayer)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		Player.LocalPlayerIndex = (GameInstance && LocalPlayer) ? GameInstance->GetLocalPlayers().IndexOfByKey(LocalPlayer) : INDEX_NONE;
		Player.bIsHost = Player.LocalPlayerIndex == 0;
	}
	return Player;
}

void UDLLMultiplayerSubsystem::HandleGameModePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	// The game mode only exists on the server, so this is host-side by construction; it is also limited to this
	// game instance (PIE) and to while a session is hosted.
	if (!GameMode || !NewPlayer || GameMode->GetGameInstance() != GetGameInstance() || !IsSessionHost())
	{
		return;
	}

	const FDLLMultiplayerSessionPlayer Player = MakeSessionPlayer(NewPlayer);
	for (FDLLMultiplayerSessionPlayer& Existing : SessionPlayers)
	{
		if (DLLMultiplayerSessionCore::IsSamePlayer(Existing, Player))
		{
			// A level change logs everyone in again: refresh the entry, never announce the player twice.
			if (!Player.DisplayName.IsEmpty())
			{
				Existing.DisplayName = Player.DisplayName;
			}
			if (Player.UniqueNetId.IsValid())
			{
				Existing.UniqueNetId = Player.UniqueNetId;
			}
			return;
		}
	}

	SessionPlayers.Add(Player);
	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: %s joined (%s)"), *Player.DisplayName,
		Player.bIsLocalPlayer ? TEXT("local seat") : TEXT("remote"));
	OnPlayerJoined.Broadcast(Player, Player.bIsLocalPlayer);
}

void UDLLMultiplayerSubsystem::HandleGameModeLogout(AGameModeBase* GameMode, AController* Exiting)
{
	const APlayerController* PlayerController = Cast<APlayerController>(Exiting);
	if (!GameMode || !PlayerController || GameMode->GetGameInstance() != GetGameInstance() || !IsSessionHost())
	{
		return;
	}

	// A map change logs everyone out; those players are not leaving the session. Non-seamless ServerTravel (always,
	// in PIE) does it while NextURL is pending, seconds BEFORE the old world tears down; seamless travel while
	// IsInSeamlessTravel. Players who really left during a travel are reported by the reconcile after it.
	const UWorld* World = GameMode->GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}
	if (World->IsInSeamlessTravel() || !World->NextURL.IsEmpty())
	{
		ScheduleRosterReconcile();
		return;
	}

	const FDLLMultiplayerSessionPlayer Leaving = MakeSessionPlayer(PlayerController);
	const int32 Index = SessionPlayers.IndexOfByPredicate([&Leaving](const FDLLMultiplayerSessionPlayer& Entry)
	{
		return DLLMultiplayerSessionCore::IsSamePlayer(Entry, Leaving);
	});
	if (Index == INDEX_NONE)
	{
		return;
	}

	const FDLLMultiplayerSessionPlayer Player = SessionPlayers[Index];
	SessionPlayers.RemoveAt(Index);
	const bool bKicked = Player.UniqueNetId.IsValid() && PendingKicks.RemoveSingle(Player.UniqueNetId) > 0;
	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: %s left (%s)"), *Player.DisplayName, bKicked ? TEXT("kicked") : TEXT("left"));
	OnPlayerLeft.Broadcast(Player, bKicked ? EDLLMultiplayerPlayerLeaveReason::Kicked : EDLLMultiplayerPlayerLeaveReason::Voluntary);
}

void UDLLMultiplayerSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	// The online join had already succeeded when the host refused the login, so report it as a join result and
	// leave the session again instead of sitting "in session" with no connection.
	if (!GEngine || !ErrorString.Equals(ErrorWrongPassword))
	{
		return;
	}
	// The failure is broadcast before the pending connection is cancelled, so its driver still maps to its context.
	// Without this, every game instance in a PIE process would react to one client's refusal.
	const FWorldContext* Context = World
		? GEngine->GetWorldContextFromWorld(World)
		: GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver);
	if (!Context || Context->OwningGameInstance != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayer: the host refused the session password"));
	CurrentSessionPassword.Empty();
	OnSessionJoined.Broadcast(EDLLMultiplayerJoinSessionResult::WrongPassword, FDLLMultiplayerSessionInfo());
	if (IsInSession() && !IsSessionHost())
	{
		DestroySession();
	}
}

void UDLLMultiplayerSubsystem::HandleSessionInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId,
	const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
{
	FDLLMultiplayerGameInvite Invite;
	Invite.FromPlayer.UniqueNetId = FUniqueNetIdRepl(FromId);
	// The service-specific friendly name needs a friends lookup; the id is the stable fallback a UI can resolve.
	Invite.FromPlayer.DisplayName = FromId.ToString();
	Invite.SessionInfo = FDLLMultiplayerSessionInfo::FromSearchResult(InviteResult);
	Invite.ReceivedTime = FDateTime::UtcNow();
	Invite.bIsValid = InviteResult.IsValid();
	UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: invite received from %s"), *Invite.FromPlayer.DisplayName);
	OnGameInviteReceived.Broadcast(Invite);
}

void UDLLMultiplayerSubsystem::ScheduleRosterReconcile()
{
	// The game instance's timer manager outlives map changes, which is exactly the window this has to span.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().SetTimer(RosterReconcileTimer, this, &ThisClass::ReconcileRosterAfterTravel,
			RosterReconcileDelaySeconds, false);
	}
}

void UDLLMultiplayerSubsystem::ReconcileRosterAfterTravel()
{
	UWorld* World = GetWorld();
	if (!World || !IsSessionHost())
	{
		return;
	}

	TArray<FDLLMultiplayerSessionPlayer> Present;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PlayerController = It->Get())
		{
			Present.Add(MakeSessionPlayer(PlayerController));
		}
	}

	for (int32 Index = SessionPlayers.Num() - 1; Index >= 0; --Index)
	{
		const FDLLMultiplayerSessionPlayer Player = SessionPlayers[Index];
		if (Player.bIsLocalPlayer)
		{
			continue;
		}
		const bool bBack = Present.ContainsByPredicate([&Player](const FDLLMultiplayerSessionPlayer& Entry)
		{
			return DLLMultiplayerSessionCore::IsSamePlayer(Entry, Player);
		});
		if (!bBack)
		{
			SessionPlayers.RemoveAt(Index);
			UE_LOG(LogTemp, Log, TEXT("DLLMultiplayer: %s did not return after the map change"), *Player.DisplayName);
			OnPlayerLeft.Broadcast(Player, EDLLMultiplayerPlayerLeaveReason::ConnectionLost);
		}
	}
}
