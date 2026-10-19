// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DLLMultiplayerLocalPlayerLibrary.generated.h"

/**
 * One couch-co-op seat.
 *
 * Project-neutral counterpart to a game's own slot struct. Games are free to keep their own type for
 * Blueprint compatibility and convert at the boundary, so their existing widget bindings never have to
 * change.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerPlayerSlotInfo
{
	GENERATED_BODY()

	/** Seat index, 0-based. Slot 0 is the primary player and cannot be vacated. */
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 SlotIndex = 0;

	/** Whether a local player currently occupies this seat. */
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	bool bIsOccupied = false;

	/** Display name. Falls back to "Player N" for an empty seat, so lobby UI always has something to draw. */
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	FString PlayerName;

	/** True for slot 0 - the player who owns the session. */
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	bool bIsHost = false;

	/** Ready state, resolved from the session subsystem when one is running. */
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	bool bIsReady = false;

	/** Owning gamepad, or -1 for keyboard/mouse. */
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 ControllerId = -1;
};

/**
 * ============================================================================
 * LOCAL PLAYERS / SPLIT-SCREEN - the reusable half
 * ============================================================================
 * Everything here is about seats on ONE machine: how many there are, who holds
 * which gamepad, and adding or dropping a player mid-session. None of it knows
 * anything about a particular game, which is why it belongs in the plugin
 * rather than being rewritten per project.
 *
 * It cooperates with UDLLMultiplayerSubsystem when one exists (ready
 * states, register/unregister on join and leave) and degrades to plain
 * UGameInstance behaviour when it does not - so this is usable in a project
 * that has not set up online sessions at all.
 *
 * ----------------------------------------------------------------------------
 * MAX SEATS IS CONFIGURABLE, NOT BAKED IN
 * ----------------------------------------------------------------------------
 * The seat limit is read from DefaultGame.ini and defaults to 4:
 *
 *     [/Script/DLLMultiplayerRuntime.LocalPlayers]
 *     MaxLocalPlayers=4
 * ============================================================================
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerLocalPlayerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * How many local players are currently seated.
	 *
	 * Prefers the session subsystem's count, falling back to the game instance. The fallback
	 * returns 1 rather than 0 when there is no world at all: callers use this to size UI, and a
	 * zero would render an empty lobby for a game that always has at least one player.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static int32 GetLocalPlayerCount(UObject* WorldContextObject);

	/** Maximum seats on this machine. Config-driven; see the class comment. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|SplitScreen")
	static int32 GetMaxLocalPlayers();

	/**
	 * Every seat, occupied or not, always exactly GetMaxLocalPlayers() entries.
	 *
	 * Returning empty seats rather than only the filled ones is deliberate: a lobby screen wants to
	 * draw "Press A to join" placeholders, and building those from a short array means every caller
	 * reimplements the padding.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDLLMultiplayerPlayerSlotInfo> GetPlayerSlotInfo(UObject* WorldContextObject);

	/**
	 * Seat a new local player for a gamepad.
	 *
	 * @param OutPlayerIndex  The seat taken, or the seat already held when the controller was
	 *                        registered previously.
	 * @return True only when a NEW player was created. An already-registered controller returns
	 *         false while still reporting its index - so a join handler can tell "joined" from
	 *         "already in" without a second query.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static bool AddLocalPlayer(UObject* WorldContextObject, int32 ControllerId, int32& OutPlayerIndex);

	/**
	 * Seat the next couch player without the caller having to work out which controller is free.
	 *
	 * THE ONE-BUTTON "PRESS START TO JOIN" CALL. Every lobby needs it, and the obvious
	 * implementation is wrong: deriving the next id from a player COUNT breaks the moment a seat is
	 * vacated (remove player 1 of 3 and the count no longer maps to a free index), and it broke
	 * outright in a real lobby when GetLocalPlayerCount under-reported an engine-created boot player -
	 * it asked for controller 0, which player 0 already held, and refused every join.
	 *
	 * This searches for a genuinely free seat instead, via IsControllerRegistered, which resolves
	 * against the GameInstance's real local players.
	 *
	 * @param OutPlayerIndex   Seat taken, or INDEX_NONE when none was.
	 * @param OutControllerId  Controller seated, or INDEX_NONE when every seat is full.
	 * @return True only when a NEW player was created.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static bool AddNextLocalPlayer(UObject* WorldContextObject, int32& OutPlayerIndex, int32& OutControllerId);

	/** First controller id with no local player on it, or INDEX_NONE when the game is full. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static int32 FindFreeControllerId(UObject* WorldContextObject);

	/**
	 * Drop a local player. Refuses slot 0 - removing the primary player would leave the game with no
	 * viewport owner.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static bool RemoveLocalPlayer(UObject* WorldContextObject, int32 PlayerIndex);

	/** Whether a gamepad already holds a seat - the guard a "press to join" prompt needs. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static bool IsControllerRegistered(UObject* WorldContextObject, int32 ControllerId);

	/** Seat index for a gamepad, or -1 if it holds none. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|SplitScreen", meta = (WorldContext = "WorldContextObject"))
	static int32 GetPlayerIndexForController(UObject* WorldContextObject, int32 ControllerId);
};
