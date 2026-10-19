// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Player/DLLMultiplayerLocalPlayerLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "DLLMultiplayerSubsystem.h"
#include "Types/DLLMultiplayerSessionTypes.h"

namespace DLLMultiplayerLocalPlayers
{
	/** The session subsystem, when the project runs one. Null is a supported state throughout. */
	static UDLLMultiplayerSubsystem* GetSessionSubsystem(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
			: nullptr;

		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UDLLMultiplayerSubsystem>() : nullptr;
	}

	static UGameInstance* GetGameInstance(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
			: nullptr;
		return World ? World->GetGameInstance() : nullptr;
	}
}

int32 UDLLMultiplayerLocalPlayerLibrary::GetLocalPlayerCount(UObject* WorldContextObject)
{
	// ========================================================================
	// THE GAME INSTANCE IS THE TRUTH; THE SUBSYSTEM IS A REGISTRY
	// ========================================================================
	// This used to ask the subsystem FIRST and return its answer. The subsystem only counts players
	// that were added THROUGH this library, so a project's boot player - created by the engine, never
	// registered here - was invisible: the count read 0 while GetNumLocalPlayers() read 1.
	//
	// Observed in a game's lobby (2026-08-08): it derived "next free controller id" from this count,
	// asked for controller 0, and every couch-join was refused because player 0 already held it.
	//
	// The seat that EXISTS is the authority; the registry can only ever be a subset of it. Taking the
	// larger of the two keeps any extra bookkeeping the subsystem does while never under-reporting a
	// real player.
	const UGameInstance* GameInstance = DLLMultiplayerLocalPlayers::GetGameInstance(WorldContextObject);
	const int32 EngineSeats = GameInstance ? GameInstance->GetNumLocalPlayers() : 0;

	int32 RegistrySeats = 0;
	if (const UDLLMultiplayerSubsystem* Subsystem = DLLMultiplayerLocalPlayers::GetSessionSubsystem(WorldContextObject))
	{
		RegistrySeats = const_cast<UDLLMultiplayerSubsystem*>(Subsystem)->GetLocalPlayerCount();
	}

	const int32 Seats = FMath::Max(EngineSeats, RegistrySeats);

	// One, not zero: callers size lobby UI from this, and a game always has at least one seat.
	return Seats > 0 ? Seats : 1;
}

int32 UDLLMultiplayerLocalPlayerLibrary::GetMaxLocalPlayers()
{
	// Config-driven so a shared plugin does not bake in one game's seat count. The default matches
	// the original hardcoded value, so projects that never set it behave exactly as before.
	int32 MaxPlayers = 4;
	if (GConfig)
	{
		GConfig->GetInt(TEXT("/Script/DLLMultiplayerRuntime.LocalPlayers"), TEXT("MaxLocalPlayers"), MaxPlayers, GGameIni);
	}

	// A zero or negative limit would produce an empty slot array and a lobby that cannot be joined,
	// which reads as a broken build rather than as a bad ini value.
	return FMath::Max(1, MaxPlayers);
}

TArray<FDLLMultiplayerPlayerSlotInfo> UDLLMultiplayerLocalPlayerLibrary::GetPlayerSlotInfo(UObject* WorldContextObject)
{
	TArray<FDLLMultiplayerPlayerSlotInfo> Slots;
	Slots.SetNum(GetMaxLocalPlayers());

	// Every seat is described, occupied or not - see the header for why empties are included.
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		Slots[i].SlotIndex = i;
		Slots[i].bIsOccupied = false;
		Slots[i].PlayerName = FString::Printf(TEXT("Player %d"), i + 1);
		Slots[i].bIsHost = (i == 0);
		Slots[i].bIsReady = false;
		Slots[i].ControllerId = -1;
	}

	UGameInstance* GameInstance = DLLMultiplayerLocalPlayers::GetGameInstance(WorldContextObject);
	if (!GameInstance)
	{
		return Slots;
	}

	UDLLMultiplayerSubsystem* Subsystem = DLLMultiplayerLocalPlayers::GetSessionSubsystem(WorldContextObject);
	const TArray<FDLLMultiplayerSessionPlayer> SessionPlayers = Subsystem ? Subsystem->GetSessionPlayers() : TArray<FDLLMultiplayerSessionPlayer>();

	for (int32 i = 0; i < GameInstance->GetNumLocalPlayers() && i < Slots.Num(); ++i)
	{
		const ULocalPlayer* LP = GameInstance->GetLocalPlayerByIndex(i);
		if (!LP)
		{
			continue;
		}

		Slots[i].bIsOccupied = true;
		Slots[i].PlayerName = LP->GetNickname();
		Slots[i].bIsHost = (i == 0);
		Slots[i].ControllerId = LP->GetControllerId();

		// Ready state is session-owned; absent a session every seat simply reads not-ready.
		for (const FDLLMultiplayerSessionPlayer& SP : SessionPlayers)
		{
			if (SP.bIsLocalPlayer && SP.LocalPlayerIndex == i)
			{
				Slots[i].bIsReady = SP.bIsReady;
				break;
			}
		}
	}

	return Slots;
}

bool UDLLMultiplayerLocalPlayerLibrary::AddLocalPlayer(UObject* WorldContextObject, int32 ControllerId, int32& OutPlayerIndex)
{
	OutPlayerIndex = -1;

	UGameInstance* GameInstance = DLLMultiplayerLocalPlayers::GetGameInstance(WorldContextObject);
	if (!GameInstance)
	{
		return false;
	}

	if (GameInstance->GetNumLocalPlayers() >= GetMaxLocalPlayers())
	{
		return false;
	}

	// Already seated: report WHERE while returning false, so a join handler can distinguish
	// "joined now" from "was already in" without a second lookup.
	for (int32 i = 0; i < GameInstance->GetNumLocalPlayers(); ++i)
	{
		const ULocalPlayer* LP = GameInstance->GetLocalPlayerByIndex(i);
		if (LP && LP->GetControllerId() == ControllerId)
		{
			OutPlayerIndex = i;
			return false;
		}
	}

	FString Error;
	ULocalPlayer* NewPlayer = GameInstance->CreateLocalPlayer(ControllerId, Error, true);
	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DLLMultiplayer LocalPlayers] CreateLocalPlayer failed for controller %d: %s"),
			ControllerId, *Error);
		return false;
	}

	OutPlayerIndex = GameInstance->GetNumLocalPlayers() - 1;

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (UDLLMultiplayerSubsystem* Subsystem = DLLMultiplayerLocalPlayers::GetSessionSubsystem(WorldContextObject))
	{
		if (APlayerController* PC = World ? NewPlayer->GetPlayerController(World) : nullptr)
		{
			Subsystem->RegisterLocalPlayer(PC);
		}
	}

	return true;
}

bool UDLLMultiplayerLocalPlayerLibrary::RemoveLocalPlayer(UObject* WorldContextObject, int32 PlayerIndex)
{
	// Slot 0 owns the viewport; removing it leaves the game with no primary player.
	if (PlayerIndex <= 0)
	{
		return false;
	}

	UGameInstance* GameInstance = DLLMultiplayerLocalPlayers::GetGameInstance(WorldContextObject);
	if (!GameInstance || PlayerIndex >= GameInstance->GetNumLocalPlayers())
	{
		return false;
	}

	ULocalPlayer* LP = GameInstance->GetLocalPlayerByIndex(PlayerIndex);
	if (!LP)
	{
		return false;
	}

	// Unregister BEFORE removal, while the controller still exists to be unregistered.
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (UDLLMultiplayerSubsystem* Subsystem = DLLMultiplayerLocalPlayers::GetSessionSubsystem(WorldContextObject))
	{
		if (APlayerController* PC = World ? LP->GetPlayerController(World) : nullptr)
		{
			Subsystem->UnregisterLocalPlayer(PC);
		}
	}

	GameInstance->RemoveLocalPlayer(LP);
	return true;
}

bool UDLLMultiplayerLocalPlayerLibrary::IsControllerRegistered(UObject* WorldContextObject, int32 ControllerId)
{
	return GetPlayerIndexForController(WorldContextObject, ControllerId) != INDEX_NONE;
}

int32 UDLLMultiplayerLocalPlayerLibrary::GetPlayerIndexForController(UObject* WorldContextObject, int32 ControllerId)
{
	const UGameInstance* GameInstance = DLLMultiplayerLocalPlayers::GetGameInstance(WorldContextObject);
	if (!GameInstance)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < GameInstance->GetNumLocalPlayers(); ++i)
	{
		const ULocalPlayer* LP = GameInstance->GetLocalPlayerByIndex(i);
		if (LP && LP->GetControllerId() == ControllerId)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

int32 UDLLMultiplayerLocalPlayerLibrary::FindFreeControllerId(UObject* WorldContextObject)
{
	const int32 MaxLocal = GetMaxLocalPlayers();
	for (int32 Id = 0; Id < MaxLocal; ++Id)
	{
		if (!IsControllerRegistered(WorldContextObject, Id))
		{
			return Id;
		}
	}
	return INDEX_NONE;
}

bool UDLLMultiplayerLocalPlayerLibrary::AddNextLocalPlayer(UObject* WorldContextObject, int32& OutPlayerIndex, int32& OutControllerId)
{
	OutPlayerIndex = INDEX_NONE;
	OutControllerId = FindFreeControllerId(WorldContextObject);

	if (OutControllerId == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DLLMultiplayer LocalPlayers] No free seat - all %d local slots are taken."),
			GetMaxLocalPlayers());
		return false;
	}

	return AddLocalPlayer(WorldContextObject, OutControllerId, OutPlayerIndex);
}
