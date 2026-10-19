// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/DLLMultiplayerSessionUITypes.h"
#include "DLLMultiplayerSessionLibrary.generated.h"

class UDLLMultiplayerSubsystem;

/**
 * ============================================================================
 * SESSION OPERATIONS FOR MENUS - the reusable half
 * ============================================================================
 * A menu-shaped facade over UDLLMultiplayerSubsystem: host, find, join,
 * leave, and the handful of queries and formatters a lobby or server browser
 * needs. None of it knows anything about a particular game.
 *
 * ----------------------------------------------------------------------------
 * WHY A FACADE AND NOT "JUST CALL THE SUBSYSTEM"
 * ----------------------------------------------------------------------------
 * The subsystem is the transport. These functions add the parts every project
 * re-invents on top of it and gets subtly wrong:
 *
 *   - **Guard order.** Host and join check "is there a subsystem" before
 *     "am I already in a session", so a project without online support reports
 *     SubsystemUnavailable rather than a misleading AlreadyInSession.
 *   - **Clamping.** Player counts and search sizes are clamped here, so a UI
 *     slider wired straight to a spinbox cannot ask for 0 or 100000 players.
 *   - **A UI-shaped result.** Everything returns EDLLMultiplayerSessionOperationResult, so
 *     a button has one thing to switch on. Note that a successfully STARTED
 *     async operation returns InProgress, never Success - callers must wait on
 *     the subsystem's delegates rather than treating the return as completion.
 *   - **Join by index.** Sessions are joined by their index into the cached
 *     search results, because names are neither unique nor stable.
 * ============================================================================
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerSessionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================================================
	// ACCESS AND AVAILABILITY
	// ========================================================================

	/** The session subsystem for this world, or null when the project runs without one. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static UDLLMultiplayerSubsystem* GetSessionSubsystem(UObject* WorldContextObject);

	/** Whether sessions can actually be used - a subsystem exists AND its interface is valid. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static bool IsSessionSystemAvailable(UObject* WorldContextObject);

	/** Display name of the online service ("Steam", "NULL", ...), or "None". */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static FString GetOnlineServiceName(UObject* WorldContextObject);

	/**
	 * Whether sessions are running over LAN.
	 * Defaults to TRUE with no subsystem: LAN needs no online service, so it is the mode most
	 * likely to actually work, and the safer thing to offer.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static bool IsUsingLAN(UObject* WorldContextObject);

	// ========================================================================
	// HOST / FIND / JOIN / LEAVE
	// ========================================================================

	/**
	 * Host an online session. MaxPlayers is clamped to 1-16. With LobbyMapPath set (e.g. "/Game/Maps/Lobby") the host
	 * travels there as a listen server once the session exists; empty = stay on the current level.
	 * Returns InProgress on success - await the subsystem's delegates.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult QuickHostSession(UObject* WorldContextObject, int32 MaxPlayers,
		const FString& MatchType, const FString& LobbyMapPath);

	/** Host a LAN session. Needs no online service, so this works with Steam offline. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult QuickHostLANSession(UObject* WorldContextObject, int32 MaxPlayers,
		const FString& MatchType);

	/** Host an invite-only session with MaxPlayers (clamped to 1-16) and MatchType. Stays on the current level. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult QuickHostPrivateSession(UObject* WorldContextObject, int32 MaxPlayers,
		const FString& MatchType);

	/** Search for sessions. MaxResults is clamped to 1-10000. Results land in the cached list. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult QuickFindSessions(UObject* WorldContextObject, int32 MaxResults, bool bSearchLAN);

	/** Join by index into the cached search results - see GetSessionListForUI. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult QuickJoinSession(UObject* WorldContextObject, int32 SessionIndex);

	/**
	 * Join a raw "ip[:port]" address, bypassing sessions entirely.
	 * The escape hatch for LAN testing and dedicated servers that were never advertised. Appends
	 * DefaultPort when the address carries none.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult DirectJoinByAddress(UObject* WorldContextObject, const FString& Address,
		int32 DefaultPort = 7777);

	/** Leave/destroy the current session. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerSessionOperationResult LeaveCurrentSession(UObject* WorldContextObject);

	// ========================================================================
	// STATE QUERIES
	// ========================================================================

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static bool IsInSession(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static bool IsSessionHost(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static FString GetCurrentSessionName(UObject* WorldContextObject);

	/** @return False (and zeroed outputs) when not in a session. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static bool GetCurrentSessionPlayerCount(UObject* WorldContextObject, int32& OutCurrentPlayers, int32& OutMaxPlayers);

	// ========================================================================
	// BROWSER
	// ========================================================================

	/**
	 * The cached search results as browser rows.
	 *
	 * Joinability is DERIVED here rather than trusted from the session: a row is joinable only with
	 * a free slot AND either join-in-progress allowed or the session still in its lobby. Without
	 * that, a browser happily offers rows that will refuse the join.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDLLMultiplayerSessionEntry> GetSessionListForUI(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFoundSessionCount(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session", meta = (WorldContext = "WorldContextObject"))
	static bool IsSearchingForSessions(UObject* WorldContextObject);

	// ========================================================================
	// PRESENTATION HELPERS
	// ========================================================================

	/** Human-readable text for a result code, for a status line. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Utility")
	static FString GetOperationResultMessage(EDLLMultiplayerSessionOperationResult Result);

	/** "42ms", or "N/A" for an unknown ping. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Utility")
	static FString FormatPingForDisplay(int32 PingMs);

	/** Green/yellow/orange/red by latency band; grey when unknown. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Utility")
	static FLinearColor GetPingColor(int32 PingMs);

	/**
	 * Whether hosting is possible right now, with a player-facing reason when not.
	 *
	 * Note the deliberate asymmetry: an invalid session interface is NOT fatal when the subsystem is
	 * in LAN mode, because LAN hosting needs no online service. Treating it as fatal would block
	 * couch/LAN play whenever Steam happens to be offline.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Utility", meta = (WorldContext = "WorldContextObject"))
	static bool CanHostSession(UObject* WorldContextObject, FString& OutReason);

	/** Open the platform's friends/invite overlay. False when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Utility")
	static bool OpenSteamOverlay();
};
