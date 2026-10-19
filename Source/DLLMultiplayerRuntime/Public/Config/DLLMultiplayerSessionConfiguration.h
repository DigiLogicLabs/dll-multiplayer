// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/DLLMultiplayerSessionTypes.h"
#include "DLLMultiplayerSessionConfiguration.generated.h"

/**
 * Data asset for configuring multiplayer session settings.
 * Create different configurations for different game modes (Campaign, Deathmatch, etc.)
 *
 * Usage:
 * 1. Create a new DLL Multiplayer Session Configuration asset in the Content Browser
 * 2. Configure the settings for your game mode
 * 3. Pass to UDLLMultiplayerSubsystem::CreateSessionWithConfig()
 */
UCLASS(BlueprintType, Blueprintable)
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerSessionConfiguration : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UDLLMultiplayerSessionConfiguration();

	// ========================================================================
	// IDENTITY
	// ========================================================================

	/** Human-readable name for this configuration (e.g., "Campaign Co-Op", "Team Deathmatch") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Identity")
	FText DisplayName;

	/** Description of this session type */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Identity", meta = (MultiLine = true))
	FText Description;

	/** Unique identifier for this session type (used for matchmaking) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Identity")
	FString MatchTypeId;

	// ========================================================================
	// SESSION TYPE & VISIBILITY
	// ========================================================================

	/** The type of session this configuration creates */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Type")
	EDLLMultiplayerSessionType SessionType = EDLLMultiplayerSessionType::CampaignCoop;

	/** Default visibility for sessions using this configuration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Type")
	EDLLMultiplayerSessionVisibility DefaultVisibility = EDLLMultiplayerSessionVisibility::Public;

	// ========================================================================
	// PLAYER LIMITS
	// ========================================================================

	/** Maximum number of public connections (server browser joinable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Players", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxPublicConnections = 4;

	/** Maximum number of private connections (invite-only slots) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Players", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxPrivateConnections = 0;

	/** Minimum players required to start the session */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Players", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MinPlayersToStart = 1;

	/** Maximum local split-screen players allowed per machine */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Players", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxLocalPlayers = 4;

	// ========================================================================
	// JOIN SETTINGS
	// ========================================================================

	/** Allow players to join while game is in progress */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Joining")
	bool bAllowJoinInProgress = true;

	/** Allow joining via presence (Steam friends list, etc.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Joining")
	bool bAllowJoinViaPresence = true;

	/** Advertise session in server browser/matchmaking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Joining")
	bool bShouldAdvertise = true;

	/** Use platform presence features */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Joining")
	bool bUsesPresence = true;

	/** Use modern lobby system if available (UE5+) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Joining")
	bool bUseLobbiesIfAvailable = true;

	/** Allow password protection for this session type */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Joining")
	bool bAllowPasswordProtection = true;

	// ========================================================================
	// MAP & MODE
	// ========================================================================

	/** Default lobby map path (with ?listen appended automatically) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Map", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath DefaultLobbyMap;

	/** Default game mode for this session type */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Map")
	FString DefaultGameMode;

	/** List of allowed maps for this session type (empty = all maps allowed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Map")
	TArray<FSoftObjectPath> AllowedMaps;

	// ========================================================================
	// ADVANCED SETTINGS
	// ========================================================================

	/** Support host migration when host disconnects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Advanced")
	bool bSupportsHostMigration = false;

	/** Use dedicated server (if available) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Advanced")
	bool bPreferDedicatedServer = false;

	/** Require all players to be ready before host can start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Advanced")
	bool bRequireAllPlayersReady = false;

	/** Auto-start when all players are ready */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Advanced")
	bool bAutoStartWhenReady = false;

	/** Countdown time before auto-start (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Advanced", meta = (ClampMin = "0", ClampMax = "60", EditCondition = "bAutoStartWhenReady"))
	float AutoStartCountdownSeconds = 5.0f;

	/** Build unique ID for version matching (0 = use engine default) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Advanced")
	int32 BuildUniqueId = 0;

	// ========================================================================
	// CUSTOM SETTINGS
	// ========================================================================

	/** Custom key-value settings that will be advertised with the session */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Custom")
	TMap<FString, FString> CustomSettings;

	/** Gameplay tags associated with this session type */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Custom")
	FGameplayTagContainer SessionTags;

	// ========================================================================
	// METHODS
	// ========================================================================

	/** Get the total maximum players (public + private) */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	int32 GetTotalMaxPlayers() const { return MaxPublicConnections + MaxPrivateConnections; }

	/** Get the lobby map path with ?listen suffix */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	FString GetLobbyMapPathWithOptions() const;

	/** Check if a map is allowed for this session type */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session")
	bool IsMapAllowed(const FSoftObjectPath& MapPath) const;

	/** Validate the configuration for errors */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session")
	bool ValidateConfiguration(FString& OutError) const;

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * Runtime session settings built from configuration + player overrides.
 * This is what actually gets passed to session creation.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerSessionCreationSettings
{
	GENERATED_BODY()

	/** The base configuration to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	TObjectPtr<UDLLMultiplayerSessionConfiguration> Configuration;

	/** Override: Session name (empty = use host name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Overrides")
	FString SessionName;

	/** Override: Map to use (empty = use config default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Overrides")
	FSoftObjectPath MapOverride;

	/** Override: Password (empty = no password) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Overrides")
	FString Password;

	/** Override: Visibility (if different from config default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Overrides")
	TOptional<EDLLMultiplayerSessionVisibility> VisibilityOverride;

	/** Override: Max public connections (0 = use config default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Overrides", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaxPublicConnectionsOverride = 0;

	/** Additional custom settings to merge with config */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Overrides")
	TMap<FString, FString> AdditionalCustomSettings;

	FDLLMultiplayerSessionCreationSettings() = default;
	explicit FDLLMultiplayerSessionCreationSettings(UDLLMultiplayerSessionConfiguration* InConfig) : Configuration(InConfig) {}

	/** Get effective session name */
	FString GetEffectiveSessionName() const;

	/** Get effective map path */
	FSoftObjectPath GetEffectiveMap() const;

	/** Get effective visibility */
	EDLLMultiplayerSessionVisibility GetEffectiveVisibility() const;

	/** Get effective max public connections */
	int32 GetEffectiveMaxPublicConnections() const;

	/** Check if password is set */
	bool HasPassword() const { return !Password.IsEmpty(); }

	/** Build merged custom settings (config + overrides) */
	TMap<FString, FString> GetMergedCustomSettings() const;

	/** Validate settings are complete and correct */
	bool IsValid(FString& OutError) const;
};
