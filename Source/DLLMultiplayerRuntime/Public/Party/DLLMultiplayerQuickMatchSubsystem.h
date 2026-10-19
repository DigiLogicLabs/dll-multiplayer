// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/DLLMultiplayerSessionTypes.h"
#include "DLLMultiplayerQuickMatchSubsystem.generated.h"

class UDLLMultiplayerSubsystem;

UENUM(BlueprintType)
enum class EDLLMultiplayerQuickMatchState : uint8
{
	Idle,
	Searching,
	Joining,
	Hosting,
	/** Joined a game (the client is travelling) or started hosting one. */
	Succeeded,
	Failed,
	Cancelled
};

/** What Quick Match should look for, and what to do when it finds nothing. */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerQuickMatchRequest
{
	GENERATED_BODY()

	/** Only games advertising this match type. Empty = any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch")
	FString MatchType;

	/** Search/host on LAN instead of the online platform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch")
	bool bLAN = false;

	/** Seats the party needs. 0 = every local player on this machine (couch guests come along). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch", meta = (ClampMin = "0", ClampMax = "4"))
	int32 PartySize = 0;

	/** Ignore games slower than this (ms). 0 = any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch", meta = (ClampMin = "0"))
	int32 MaxPing = 0;

	/** How many search results to consider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch", meta = (ClampMin = "1", ClampMax = "200"))
	int32 MaxSearchResults = 50;

	/** Joins to attempt before giving up on joining (each failed join tries the next best game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxJoinAttempts = 3;

	/** With no suitable game, host one (so the next Quick Match finds it) instead of failing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch")
	bool bHostIfNoneFound = true;

	/** Level to host into when hosting. Required if bHostIfNoneFound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch")
	FString HostLevelPath;

	/** Online session slots when hosting (machines on account-counting platforms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickMatch", meta = (ClampMin = "1", ClampMax = "16"))
	int32 HostMaxConnections = 8;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLLMultiplayerOnQuickMatchStateChanged, EDLLMultiplayerQuickMatchState, State, const FText&, Message);

/**
 * Party-aware Quick Match on top of UDLLMultiplayerSubsystem (which is unchanged):
 *   search -> rank games that fit the WHOLE party (UDLLMultiplayerPartyRules::RankQuickMatchCandidates: fuller games first,
 *   then ping) -> join the best, falling through to the next on a failed join -> host if nothing fits.
 *
 * Merging SEPARATE groups into balanced matches needs a matchmaking service and is out of scope (engine-native
 * rule). Room is re-checked by the host at login (ADLLMultiplayerGameSession): a game that filled between search and join
 * refuses the party with a readable reason instead of splitting it.
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerQuickMatchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Start a Quick Match. Returns false (and broadcasts Failed) if one is running or the request is unusable. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|QuickMatch")
	bool StartQuickMatch(const FDLLMultiplayerQuickMatchRequest& Request);

	/** Stop searching/joining. A join already travelling cannot be recalled. */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|QuickMatch")
	void CancelQuickMatch();

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|QuickMatch")
	EDLLMultiplayerQuickMatchState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|QuickMatch")
	bool IsRunning() const { return State == EDLLMultiplayerQuickMatchState::Searching || State == EDLLMultiplayerQuickMatchState::Joining || State == EDLLMultiplayerQuickMatchState::Hosting; }

	/** Progress and outcome, with player-facing text ("Joining Conner's Game (3/8)..."). */
	UPROPERTY(BlueprintAssignable, Category = "DLL Multiplayer|QuickMatch")
	FDLLMultiplayerOnQuickMatchStateChanged OnQuickMatchStateChanged;

	virtual void Deinitialize() override;

private:

	UFUNCTION()
	void HandleSessionsFound(const TArray<FDLLMultiplayerSessionInfo>& Sessions, bool bWasSuccessful);

	UFUNCTION()
	void HandleSessionJoined(EDLLMultiplayerJoinSessionResult Result, const FDLLMultiplayerSessionInfo& SessionInfo);

	UFUNCTION()
	void HandleHostCreated(bool bWasSuccessful, const FDLLMultiplayerSessionInfo& SessionInfo);

	void TryNextCandidate();
	void HostOrFail(const FText& Reason);
	void SetState(EDLLMultiplayerQuickMatchState NewState, const FText& Message);
	void UnbindAll();
	UDLLMultiplayerSubsystem* GetSessions() const;

	FDLLMultiplayerQuickMatchRequest ActiveRequest;
	int32 ActivePartySize = 1;
	TArray<FDLLMultiplayerSessionInfo> Candidates;
	int32 NextCandidate = 0;
	int32 JoinAttempts = 0;
	EDLLMultiplayerQuickMatchState State = EDLLMultiplayerQuickMatchState::Idle;
};
