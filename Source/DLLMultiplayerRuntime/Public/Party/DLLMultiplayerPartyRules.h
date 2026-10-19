// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DLLMultiplayerPartyRules.generated.h"

class APlayerController;
class APlayerState;

/**
 * What one player said about their party when they logged in.
 *
 * A "party" here is ONE MACHINE's players: the primary seat plus any split-screen guests sharing
 * that screen. The engine logs each guest in separately (NMT_JoinSplit, a child connection), so the
 * server would otherwise see three unrelated logins where the players see one group.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerLoginPartyInfo
{
	GENERATED_BODY()

	/** Id shared by every seat on one machine for this game-instance lifetime. Empty when unknown. */
	UPROPERTY(BlueprintReadOnly, Category = "DLL Multiplayer|Party")
	FString PartyId;

	/** How many local seats the machine had when it logged in (1 = no guests). */
	UPROPERTY(BlueprintReadOnly, Category = "DLL Multiplayer|Party")
	int32 PartySize = 1;

	/** 0 = the machine's primary seat, 1..3 = split-screen guests. */
	UPROPERTY(BlueprintReadOnly, Category = "DLL Multiplayer|Party")
	int32 SeatIndex = 0;

	/** True for any seat other than the machine's first. */
	UPROPERTY(BlueprintReadOnly, Category = "DLL Multiplayer|Party")
	bool bIsGuest = false;

	/** Opaque per-player key a game may send along (e.g. a travel-stable identity). Empty if none. */
	UPROPERTY(BlueprintReadOnly, Category = "DLL Multiplayer|Party")
	FString PlayerKey;
};

/**
 * The pure rules behind couch guests joining online games. No world access: every function here is
 * arithmetic or string handling, which is what lets the automation tests pin it without a server.
 *
 * Why it exists (UE 5.7, verified in engine source):
 *   - A guest is logged in as a CHILD connection, and "if any splitscreen viewport fails to join, all
 *     viewports on that client also fail" (World.cpp PreLoginCompleteSplit). So admitting a machine
 *     whose guests will not fit gets the WHOLE machine kicked moments later. Room must be judged for
 *     the party at the first login, and held for the guests that follow.
 *   - Steam reports the SAME unique net id for every local seat, so a guest leaving would otherwise
 *     unregister its host machine from the session. Unregistering must check for a shared id.
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerPartyRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** URL option keys. Kept here so the client that writes them and the server that reads them agree. */
	static const TCHAR* OptionPartyId;
	static const TCHAR* OptionPartySize;
	static const TCHAR* OptionSeat;
	static const TCHAR* OptionPlayerKey;

	/** Session settings the host advertises so browsers and quick match count PEOPLE, not machines. */
	static const FName SettingHumans;
	static const FName SettingMaxHumans;

	/** Error tokens returned from login approval. Prefix-matched by DescribeJoinFailure. */
	static const TCHAR* ErrorFull;
	static const TCHAR* ErrorNoRoomPrefix;
	static const TCHAR* ErrorTooManyLocal;

	/** "?MPParty=..?MPPartySize=..?MPSeat=..[?MPKey=..]" - no leading '?' is required by callers. */
	static FString BuildLoginOptions(const FString& PartyId, int32 PartySize, int32 SeatIndex, const FString& PlayerKey);

	/** Reads the options above out of a login URL/options string. bIsGuest follows SeatIndex. */
	static FDLLMultiplayerLoginPartyInfo ParseLoginOptions(const FString& Options);

	/** Seats nobody has claimed or reserved. Never negative. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	static int32 GetFreeSeats(int32 MaxPlayers, int32 CurrentPlayers, int32 ReservedSeats);

	/** Whether a whole local party of this size fits in a game advertising these numbers. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	static bool CanPartyFit(int32 PartySize, int32 MaxPlayers, int32 CurrentPlayers);

	/**
	 * Login decision for one seat. Empty string = admit.
	 *
	 * @param ReservedByOthers  seats held for OTHER machines' guests who have not arrived yet
	 * @param ReservedForParty  seats held for THIS party's guests
	 */
	static FString EvaluateJoin(int32 MaxPlayers, int32 CurrentPlayers, int32 ReservedByOthers, int32 ReservedForParty,
		const FDLLMultiplayerLoginPartyInfo& Info, int32 MaxSeatsPerMachine);

	/** "Conner (2)" for seat 1 when the platform gives every seat the owner's name. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	static FString MakeGuestNickname(const FString& OwnerNickname, int32 SeatIndex);

	/** Player-facing text for a login refusal or network failure string. Unknown strings pass through. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	static FText DescribeJoinFailure(const FString& ErrorString);

	/**
	 * True when another PlayerState in the same game carries the same unique net id - i.e. this is a
	 * split-screen seat whose platform id belongs to the whole machine. Unregistering that id from the
	 * online session would remove the machine's other players too.
	 */
	static bool IsUniqueIdSharedWithAnotherPlayer(const APlayerState* PlayerState);

	/**
	 * Quick Match: indices into Sessions worth trying, best first. Excludes password-protected games, games
	 * without room for the WHOLE party (people, via the advertised headcount), and games over MaxPing (0 = any).
	 * Orders fuller games first - so parties fill games instead of spreading one player per lobby - then by ping.
	 */
	static TArray<int32> RankQuickMatchCandidates(const TArray<struct FDLLMultiplayerSessionInfo>& Sessions, int32 PartySize, int32 MaxPing);

	/**
	 * The party info for a controller, on the server: read from the login URL a remote seat sent, or
	 * built from the local player for a listen host's own seats. Zeroed info when neither applies.
	 */
	static FDLLMultiplayerLoginPartyInfo GetPartyInfoForController(const APlayerController* PC);
};
