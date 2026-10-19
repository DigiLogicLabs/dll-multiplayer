// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "DLLMultiplayerRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FDLLMultiplayerRuntimeModule"

void FDLLMultiplayerRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FDLLMultiplayerRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDLLMultiplayerRuntimeModule, DLLMultiplayerRuntime)