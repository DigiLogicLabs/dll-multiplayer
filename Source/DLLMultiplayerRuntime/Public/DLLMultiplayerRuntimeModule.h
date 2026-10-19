// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * DLL Multiplayer runtime module (DLLMultiplayerRuntime).
 *
 * Co-op and online sessions for Unreal Engine 5: host / find / join / leave on any online subsystem (Null, Steam,
 * EOS), couch split-screen seats that join and leave as a party, party-aware login approval, quick match and
 * travel helpers. Start from UDLLMultiplayerSubsystem (a GameInstance subsystem) or the Blueprint libraries
 * UDLLMultiplayerSessionLibrary, UDLLMultiplayerLocalPlayerLibrary and UDLLMultiplayerTravelLibrary.
 * See the repo's README and docs/INTEGRATION_GUIDE.md.
 */
class FDLLMultiplayerRuntimeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
