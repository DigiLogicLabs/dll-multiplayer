// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "DLLMultiplayerSessionUITypes.generated.h"

/**
 * ============================================================================
 * MENU-FACING SESSION TYPES
 * ============================================================================
 * DLLMultiplayerSessionTypes.h describes what a session IS
 * (EDLLMultiplayerSessionType, EDLLMultiplayerSessionState, EDLLMultiplayerJoinSessionResult...). These two
 * describe what a MENU needs to draw and report, which is a different job:
 * a browser row and a "did that work?" answer a button can act on.
 *
 * They live in the plugin because every project with a server browser needs
 * exactly these, and rewriting them per project is the duplication this plugin
 * exists to stop.
 * ============================================================================
 */

/**
 * Outcome of a session operation, shaped for UI rather than for diagnosis.
 *
 * Deliberately coarse: a menu button only needs to know whether to show a
 * spinner, a specific complaint, or a generic failure. Detailed causes stay in
 * the log and in the subsystem's own richer result types.
 */
UENUM(BlueprintType)
enum class EDLLMultiplayerSessionOperationResult : uint8
{
	/** Operation completed successfully. */
	Success,
	/** Started and still running - the caller should wait for a delegate, not retry. */
	InProgress,
	/** Failed for a reason the UI has nothing specific to say about. */
	Failed,
	/** The requested session no longer exists - usually a stale browser row. */
	SessionNotFound,
	/** The session is at capacity. */
	SessionFull,
	/** Refused because the player is already in a session. */
	AlreadyInSession,
	/** Refused because the player is not in a session. */
	NotInSession,
	/** No session subsystem - the project is running without online support. */
	SubsystemUnavailable
};

/**
 * One row in a server browser.
 *
 * `SessionIndex` is the load-bearing field: it indexes the subsystem's cached
 * search results, and joining is done BY INDEX rather than by name, because
 * names are neither unique nor stable across a refresh.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerSessionEntry
{
	GENERATED_BODY()

	/** Display name of the session. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString SessionName;

	/** Host player's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString HostName;

	/** Players currently connected. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	/** Capacity. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;

	/** Round-trip time in milliseconds, or -1 when unknown. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 PingMs = -1;

	/** Game-defined match type string, used to filter the browser. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString MatchType;

	/** Map being played. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString MapName;

	/** Whether this is a LAN session. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bIsLAN = false;

	/** Whether the row should be selectable. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bIsJoinable = true;

	/** Index into the subsystem's cached results - join by THIS, not by name. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 SessionIndex = -1;
};
