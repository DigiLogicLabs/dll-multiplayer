// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Travel/DLLMultiplayerTravelLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

EDLLMultiplayerTravelMethod UDLLMultiplayerTravelLibrary::TravelToLevel(
	UObject* WorldContextObject, const FString& LevelPath, const FString& Options, bool bHasActiveSession)
{
	if (LevelPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DLLMultiplayer Travel] TravelToLevel - LevelPath is empty."));
		return EDLLMultiplayerTravelMethod::None;
	}

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World)
	{
		return EDLLMultiplayerTravelMethod::None;
	}

	// NET MODE is the primary test, not session state - see the header for why session state alone
	// strands every client of a sessionless listen server (split-screen, PIE).
	const ENetMode NetMode = World->GetNetMode();

	if (NetMode == NM_Client)
	{
		// Refused rather than travelling alone: travel is server-authoritative.
		UE_LOG(LogTemp, Warning,
			TEXT("[DLLMultiplayer Travel] TravelToLevel refused on a client - level travel is server-authoritative."));
		return EDLLMultiplayerTravelMethod::None;
	}

	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer || bHasActiveSession)
	{
		// ====================================================================
		// `?listen` IS RE-STATED FOR EVERY NON-DEDICATED SERVER
		// ====================================================================
		// It must be re-stated at all because absolute ServerTravel does not inherit it (see the
		// header). The condition is deliberately `!= NM_DedicatedServer` rather than
		// `== NM_ListenServer`, and that difference is a fixed BUG, not a style choice.
		//
		// Observed 2026-08-04: a menu-to-hub travel with a client connected produced the URL
		// "/Game/Levels/Village?game=..." with no `?listen`. The server arrived in 0.1s; the client
		// did `Browse: /Game/Levels/Sandbox?closed` and fell back to GameDefaultMap. So this branch
		// had been entered via `bHasActiveSession` while GetNetMode() reported something other than
		// NM_ListenServer - and the old condition therefore emitted no option at all.
		//
		// Keying off "not dedicated" makes the guarantee robust to however we got here: any server
		// that is also a client (GIsClient true) needs the option to keep listening, and a dedicated
		// server does not need it but is unharmed by it. The previous form silently depended on
		// GetNetMode() being authoritative at this exact moment, which it demonstrably was not.
		const FString ListenOption = (NetMode != NM_DedicatedServer) ? TEXT("?listen") : TEXT("");
		const FString URL = LevelPath + Options + ListenOption;

		// Logged because a dropped client is diagnosed from the URL and nothing else: the host's log
		// looks identical either way, and the client's symptom points at the wrong level entirely.
		UE_LOG(LogTemp, Log, TEXT("[DLLMultiplayer Travel] ServerTravel (netmode %d, session %s): %s"),
			static_cast<int32>(NetMode), bHasActiveSession ? TEXT("yes") : TEXT("no"), *URL);

		World->ServerTravel(URL);
		return EDLLMultiplayerTravelMethod::ServerTravel;
	}

	// Standalone: nobody to carry, so a direct open is correct and cheaper.
	UGameplayStatics::OpenLevel(World, FName(*LevelPath), /*bAbsolute=*/true, Options);
	return EDLLMultiplayerTravelMethod::OpenLevel;
}
