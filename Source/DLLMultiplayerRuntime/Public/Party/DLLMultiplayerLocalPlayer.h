// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "DLLMultiplayerLocalPlayer.generated.h"

/**
 * Local player that tells the server which machine-party it belongs to.
 *
 * Enable with, in DefaultEngine.ini:
 *   [/Script/Engine.Engine]
 *   LocalPlayerClassName=/Script/<YourModule>.<YourSubclass>   (or this class directly)
 *
 * The engine sends GetGameLoginOptions() on BOTH the primary login (PendingNetGame) and every guest's
 * split join (ULocalPlayer::SendSplitJoin), and re-sends both after every non-seamless travel. So
 * party info rides the one path that is guaranteed to reach the server before the player exists
 * there - no RPC handshake and no window where the server knows a seat but not its party.
 *
 * It also fixes guest names: platforms such as Steam report the owner's nickname for every seat.
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:

	virtual FString GetGameLoginOptions() const override;
	virtual FString GetNickname() const override;

	/** 0 for the machine's first seat, 1..3 for split-screen guests. INDEX_NONE if not registered. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	int32 GetSeatIndex() const;

	/** The id every seat on this machine shares. Minted once per game instance by the first seat. */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Party")
	FString GetMachinePartyId() const;

protected:

	/**
	 * Extension point: an opaque per-player key to send with the login (e.g. a travel-stable identity
	 * the game mints). Empty by default. The server reads it back from FDLLMultiplayerLoginPartyInfo::PlayerKey.
	 */
	virtual FString GetLoginPlayerKey() const { return FString(); }

private:

	/** Only meaningful on the machine's first seat; guests read the first seat's value. */
	UPROPERTY(Transient)
	mutable FGuid MachinePartyId;
};
