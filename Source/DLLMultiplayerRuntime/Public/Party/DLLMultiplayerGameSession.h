// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "Party/DLLMultiplayerPartyRules.h"
#include "DLLMultiplayerGameSession.generated.h"

/**
 * Game session that understands split-screen guests playing ONLINE.
 *
 * Use by setting GameSessionClass on a GameMode (or subclassing this and doing the same).
 *
 * What it changes versus AGameSession, and why each matters (all verified against UE 5.7 source):
 *
 *  1. CAPACITY COUNTS PEOPLE AND JUDGES THE WHOLE PARTY AT THE DOOR.
 *     The base session admits one seat at a time. A machine with guests is admitted, then a guest is
 *     refused, and the engine closes the ENTIRE connection (JoinSplitFailure) - so the host's lobby
 *     sees a player appear and vanish. Here the first seat is admitted only if every seat of its party
 *     fits, and the guests' seats are RESERVED until they arrive or the reservation times out.
 *
 *  2. A SHARED PLATFORM ID IS NEVER UNREGISTERED WHILE SOMEONE STILL USES IT.
 *     Steam gives every local seat the owner's id. The base UnregisterPlayer removes that id from the
 *     online session when a guest leaves - taking the owner out of the session's member list with it.
 *
 *  3. THE HOST ADVERTISES ITS HEADCOUNT (MP_HUMANS / MP_MAXHUMANS).
 *     An online session's connection count is per machine (Steam dedupes by id), so a browser would
 *     show a 4-person game as "1/8". Search results read these back (FDLLMultiplayerSessionInfo::FromSearchResult).
 *
 * Debug: console variable dll.Multiplayer.PartyDebug 1 prints the seat/reservation table on every login/logout.
 */
UCLASS(Config = Game)
class DLLMULTIPLAYERRUNTIME_API ADLLMultiplayerGameSession : public AGameSession
{
	GENERATED_BODY()

public:

	ADLLMultiplayerGameSession();

	// The base class overloads these by session name/id; keep those visible alongside the overrides.
	using Super::NotifyLogout;
	using Super::UnregisterPlayer;

	virtual FString ApproveLogin(const FString& Options) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void NotifyLogout(const APlayerController* PC) override;
	virtual void UnregisterPlayer(const APlayerController* ExitingPlayer) override;

	/** Seats held for guests who have not logged in yet. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	int32 GetReservedSeatCount() const;

	/** Seats a NEW party could take right now. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	int32 GetFreeSeatCount() const;

	/** How long a machine's guest seats stay held after its first seat is admitted. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "DLL Multiplayer|Party", meta = (ClampMin = "5.0"))
	float PartyReservationSeconds = 30.f;

	/** Write MP_HUMANS / MP_MAXHUMANS into the hosted online session on every join and leave. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "DLL Multiplayer|Party")
	bool bAdvertiseHeadcount = true;

protected:

	/**
	 * Called on the server after a seat has logged in and its reservation (if any) was consumed.
	 * Subclasses stamp game identity here - the login options are available in Info.
	 */
	virtual void OnPartyPlayerLoggedIn(APlayerController* NewPlayer, const FDLLMultiplayerLoginPartyInfo& Info) {}

	/** Pushes the current headcount into the named game session. Excluded controller is mid-logout. */
	void RefreshAdvertisedHeadcount(const APlayerController* ExcludedController = nullptr) const;

	/** dll.Multiplayer.PartyDebug: on-screen seat and reservation table. Event-driven - only drawn on changes. */
	void DrawPartyDebug(const TCHAR* Reason) const;

private:

	struct FSeatReservation
	{
		int32 Seats = 0;
		double ExpiresAtSeconds = 0.0;
	};

	/** Party id -> seats held for its guests. Tiny (one entry per joining machine) and short-lived. */
	TMap<FString, FSeatReservation> Reservations;

	void PruneExpiredReservations();
	int32 CountPlayers(const APlayerController* ExcludedController = nullptr) const;
	double GetNowSeconds() const;
};
