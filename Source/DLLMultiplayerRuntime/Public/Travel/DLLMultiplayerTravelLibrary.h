// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DLLMultiplayerTravelLibrary.generated.h"

/** How a travel request was carried out - for callers that want to log or branch on it. */
UENUM(BlueprintType)
enum class EDLLMultiplayerTravelMethod : uint8
{
	/** Nothing happened: empty path, no world, or refused on a client. */
	None,
	/** ServerTravel - the whole connected party moved together. */
	ServerTravel,
	/** OpenLevel - standalone, nobody to carry. */
	OpenLevel
};

/**
 * ============================================================================
 * LEVEL TRAVEL THAT DOES NOT STRAND CLIENTS
 * ============================================================================
 * One function, and it exists because getting this right is neither obvious nor
 * forgiving. Every co-op project rewrites it, and the two ways it goes wrong
 * both look fine on the host and break only for the people who joined.
 *
 * ----------------------------------------------------------------------------
 * MISTAKE 1: ROUTING ON SESSION STATE INSTEAD OF NET MODE
 * ----------------------------------------------------------------------------
 * "Am I in an online session?" is the intuitive test and it is wrong. A listen
 * server with no online session - which is exactly what split-screen and PIE
 * multiplayer are - fails that test, falls through to OpenLevel, and moves the
 * host alone while every client sits in the old level. Route on NET MODE; treat
 * an active session only as an additional reason to ServerTravel.
 *
 * ----------------------------------------------------------------------------
 * MISTAKE 2: LOSING `?listen` ON ABSOLUTE TRAVEL
 * ----------------------------------------------------------------------------
 * UWorld::ServerTravel -> ProcessServerTravel builds its URL with
 * FURL(&LastURL, *URL, TRAVEL_Absolute), and FURL only inherits the previous
 * URL's options for RELATIVE travel. An absolute travel therefore DROPS
 * `?listen`. UEngine::LoadMap only re-listens when
 * `!GIsClient || URL.HasOption("Listen")`, and GIsClient is true on a listen
 * server - so without the option the host silently stops accepting connections,
 * and the clients it just told to follow cannot reconnect. They fall back to
 * their default map, which reads as "the client randomly loaded the wrong
 * level" and sends you hunting in entirely the wrong place.
 *
 * A dedicated server does not need the option (GIsClient is false there) but is
 * unharmed by it, so one branch covers both rather than two that can drift.
 *
 * The option is emitted for every non-dedicated server rather than only when
 * the net mode reads as NM_ListenServer. That is deliberate and was a real bug:
 * a travel reached the ServerTravel branch via an active session while the net
 * mode reported otherwise, so no option was emitted and the connected client
 * was dropped to its default map. Keying off "not dedicated" makes the
 * guarantee independent of how the branch was entered.
 * ============================================================================
 */
UCLASS()
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerTravelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Travel to a level, carrying connected clients when there are any.
	 *
	 * @param WorldContextObject  Any object with world context.
	 * @param LevelPath           Long package name ("/Game/Levels/Foo"), NOT an object path.
	 * @param Options             URL options to append, e.g. "?game=/Script/X.Y". May be empty.
	 * @param bHasActiveSession   Pass true when an online session is live. Only widens the
	 *                            ServerTravel condition; net mode is still the primary test. Callers
	 *                            without a session concept can pass false.
	 * @return How the travel was performed - None means nothing happened.
	 *
	 * Refuses on a client rather than travelling alone: level travel is server-authoritative, and a
	 * client that quietly moved itself would desync from the party it belongs to.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Travel", meta = (WorldContext = "WorldContextObject"))
	static EDLLMultiplayerTravelMethod TravelToLevel(UObject* WorldContextObject, const FString& LevelPath,
		const FString& Options, bool bHasActiveSession = false);
};
