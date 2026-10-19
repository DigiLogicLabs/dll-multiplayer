// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Config/DLLMultiplayerSessionConfiguration.h"

UDLLMultiplayerSessionConfiguration::UDLLMultiplayerSessionConfiguration()
{
	DisplayName = FText::FromString(TEXT("Default Session"));
	Description = FText::FromString(TEXT("Default multiplayer session configuration."));
	MatchTypeId = TEXT("Default");
}

FString UDLLMultiplayerSessionConfiguration::GetLobbyMapPathWithOptions() const
{
	FString MapPath = DefaultLobbyMap.GetAssetPathString();
	if (MapPath.IsEmpty())
	{
		return FString();
	}

	// Remove asset path prefix if present
	if (MapPath.StartsWith(TEXT("/Game/")))
	{
		// Path is already in correct format
	}
	else if (MapPath.Contains(TEXT(".")))
	{
		// Convert package path to map path
		int32 DotIndex;
		if (MapPath.FindLastChar('.', DotIndex))
		{
			MapPath = MapPath.Left(DotIndex);
		}
	}

	return FString::Printf(TEXT("%s?listen"), *MapPath);
}

bool UDLLMultiplayerSessionConfiguration::IsMapAllowed(const FSoftObjectPath& MapPath) const
{
	// If no allowed maps specified, all maps are allowed
	if (AllowedMaps.Num() == 0)
	{
		return true;
	}

	// Check if map is in allowed list
	for (const FSoftObjectPath& AllowedMap : AllowedMaps)
	{
		if (AllowedMap == MapPath)
		{
			return true;
		}
	}

	return false;
}

bool UDLLMultiplayerSessionConfiguration::ValidateConfiguration(FString& OutError) const
{
	// Check display name
	if (DisplayName.IsEmpty())
	{
		OutError = TEXT("Display name is required.");
		return false;
	}

	// Check match type ID
	if (MatchTypeId.IsEmpty())
	{
		OutError = TEXT("Match type ID is required.");
		return false;
	}

	// Check player limits
	if (MaxPublicConnections < 1)
	{
		OutError = TEXT("Max public connections must be at least 1.");
		return false;
	}

	if (MinPlayersToStart < 1)
	{
		OutError = TEXT("Min players to start must be at least 1.");
		return false;
	}

	if (MinPlayersToStart > MaxPublicConnections + MaxPrivateConnections)
	{
		OutError = TEXT("Min players to start cannot exceed max total connections.");
		return false;
	}

	if (MaxLocalPlayers < 1 || MaxLocalPlayers > 4)
	{
		OutError = TEXT("Max local players must be between 1 and 4.");
		return false;
	}

	// Check countdown settings
	if (bAutoStartWhenReady && AutoStartCountdownSeconds < 0.0f)
	{
		OutError = TEXT("Auto-start countdown cannot be negative.");
		return false;
	}

	return true;
}

FPrimaryAssetId UDLLMultiplayerSessionConfiguration::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("MLSessionConfiguration"), GetFName());
}

#if WITH_EDITOR
void UDLLMultiplayerSessionConfiguration::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-generate match type ID from display name if empty
	if (MatchTypeId.IsEmpty() && !DisplayName.IsEmpty())
	{
		MatchTypeId = DisplayName.ToString().Replace(TEXT(" "), TEXT(""));
	}

	// Validate after changes
	FString Error;
	if (!ValidateConfiguration(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("DLLMultiplayerSessionConfiguration '%s' validation warning: %s"), *GetName(), *Error);
	}
}
#endif

// FDLLMultiplayerSessionCreationSettings implementation

FString FDLLMultiplayerSessionCreationSettings::GetEffectiveSessionName() const
{
	if (!SessionName.IsEmpty())
	{
		return SessionName;
	}
	return FString(); // Will use host name
}

FSoftObjectPath FDLLMultiplayerSessionCreationSettings::GetEffectiveMap() const
{
	if (MapOverride.IsValid())
	{
		return MapOverride;
	}
	if (Configuration)
	{
		return Configuration->DefaultLobbyMap;
	}
	return FSoftObjectPath();
}

EDLLMultiplayerSessionVisibility FDLLMultiplayerSessionCreationSettings::GetEffectiveVisibility() const
{
	if (VisibilityOverride.IsSet())
	{
		return VisibilityOverride.GetValue();
	}
	if (Configuration)
	{
		return Configuration->DefaultVisibility;
	}
	return EDLLMultiplayerSessionVisibility::Public;
}

int32 FDLLMultiplayerSessionCreationSettings::GetEffectiveMaxPublicConnections() const
{
	if (MaxPublicConnectionsOverride > 0)
	{
		return MaxPublicConnectionsOverride;
	}
	if (Configuration)
	{
		return Configuration->MaxPublicConnections;
	}
	return 4; // Fallback default
}

TMap<FString, FString> FDLLMultiplayerSessionCreationSettings::GetMergedCustomSettings() const
{
	TMap<FString, FString> Merged;

	// Start with config custom settings
	if (Configuration)
	{
		Merged = Configuration->CustomSettings;
	}

	// Override/add with creation settings
	for (const auto& Pair : AdditionalCustomSettings)
	{
		Merged.Add(Pair.Key, Pair.Value);
	}

	return Merged;
}

bool FDLLMultiplayerSessionCreationSettings::IsValid(FString& OutError) const
{
	if (!Configuration)
	{
		OutError = TEXT("Configuration is required.");
		return false;
	}

	// Validate the base configuration
	if (!Configuration->ValidateConfiguration(OutError))
	{
		return false;
	}

	// Check map override if specified
	if (MapOverride.IsValid() && !Configuration->IsMapAllowed(MapOverride))
	{
		OutError = TEXT("Selected map is not allowed for this session type.");
		return false;
	}

	// Check max connections override
	if (MaxPublicConnectionsOverride > 0)
	{
		if (MaxPublicConnectionsOverride > 64)
		{
			OutError = TEXT("Max public connections cannot exceed 64.");
			return false;
		}
	}

	return true;
}
