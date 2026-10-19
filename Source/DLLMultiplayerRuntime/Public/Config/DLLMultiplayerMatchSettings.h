// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Types/DLLMultiplayerSessionTypes.h"
#include "DLLMultiplayerMatchSettings.generated.h"

/**
 * Team configuration for team-based game modes.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerTeamConfig
{
	GENERATED_BODY()

	/** Team index (0-based) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	int32 TeamIndex = 0;

	/** Display name of the team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FText TeamName;

	/** Team color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FLinearColor TeamColor = FLinearColor::White;

	/** Maximum players on this team (0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaxPlayers = 0;

	/** Tag the game uses to find this team's spawn points. Empty by default: the plugin defines no tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FGameplayTag SpawnTag;

	FDLLMultiplayerTeamConfig() = default;
	FDLLMultiplayerTeamConfig(int32 InIndex, const FText& InName, const FLinearColor& InColor)
		: TeamIndex(InIndex), TeamName(InName), TeamColor(InColor) {}
};

/**
 * Match rule set that can be modified in the lobby.
 * Used for competitive modes with configurable rules.
 */
USTRUCT(BlueprintType)
struct DLLMULTIPLAYERRUNTIME_API FDLLMultiplayerMatchSettings
{
	GENERATED_BODY()

	// ========================================================================
	// CORE SETTINGS
	// ========================================================================

	/** Human-readable name for these settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Core")
	FString SettingsName;

	/** Session type these settings apply to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Core")
	EDLLMultiplayerSessionType SessionType = EDLLMultiplayerSessionType::CompetitiveFFA;

	/** Map path for this match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Core")
	FSoftObjectPath MapPath;

	/** Game mode class path */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Core")
	FSoftClassPath GameModeClass;

	// ========================================================================
	// WIN CONDITIONS
	// ========================================================================

	/** Score limit (0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Win Conditions", meta = (ClampMin = "0"))
	int32 ScoreLimit = 25;

	/** Time limit in seconds (0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Win Conditions", meta = (ClampMin = "0"))
	int32 TimeLimitSeconds = 600;

	/** Round limit for round-based modes (0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Win Conditions", meta = (ClampMin = "0"))
	int32 RoundLimit = 0;

	/** Stock/lives per player for elimination modes (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Win Conditions", meta = (ClampMin = "0"))
	int32 StockCount = 0;

	// ========================================================================
	// GAMEPLAY RULES
	// ========================================================================

	/** Enable friendly fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules")
	bool bFriendlyFire = false;

	/** Friendly fire damage multiplier (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bFriendlyFire"))
	float FriendlyFireDamageMultiplier = 0.5f;

	/** Enable weapon pickups on the map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules")
	bool bWeaponPickups = true;

	/** Enable health pickups on the map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules")
	bool bHealthPickups = true;

	/** Enable power-ups on the map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules")
	bool bPowerUps = true;

	/** Respawn time in seconds (0 = instant) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float RespawnTimeSeconds = 3.0f;

	/** Invulnerability time after spawn in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Rules", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float SpawnInvulnerabilitySeconds = 2.0f;

	// ========================================================================
	// TEAM SETTINGS
	// ========================================================================

	/** Team configuration for team-based modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Teams")
	TArray<FDLLMultiplayerTeamConfig> Teams;

	/** Auto-balance teams when players join/leave */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Teams")
	bool bAutoBalanceTeams = true;

	/** Allow players to switch teams */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Teams")
	bool bAllowTeamSwitching = true;

	// ========================================================================
	// HORDE/SURVIVAL SETTINGS
	// ========================================================================

	/** Starting wave for horde modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Horde", meta = (ClampMin = "1"))
	int32 StartingWave = 1;

	/** Target wave for horde modes (0 = endless) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Horde", meta = (ClampMin = "0"))
	int32 TargetWave = 0;

	/** Difficulty scaling multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Horde", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float DifficultyMultiplier = 1.0f;

	// ========================================================================
	// MUTATORS
	// ========================================================================

	/** Enabled mutators/modifiers for this match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Mutators")
	FGameplayTagContainer EnabledMutators;

	// ========================================================================
	// CUSTOM
	// ========================================================================

	/** Custom settings for game-mode-specific rules */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match|Custom")
	TMap<FString, FString> CustomSettings;

	// ========================================================================
	// METHODS
	// ========================================================================

	FDLLMultiplayerMatchSettings();

	/** Get time limit as FTimespan */
	FTimespan GetTimeLimit() const { return FTimespan::FromSeconds(TimeLimitSeconds); }

	/** Check if this is a team-based mode */
	bool IsTeamMode() const { return SessionType == EDLLMultiplayerSessionType::CompetitiveTeam; }

	/** Check if this is a survival/horde mode */
	bool IsSurvivalMode() const { return SessionType == EDLLMultiplayerSessionType::Horde; }

	/** Get team by index */
	const FDLLMultiplayerTeamConfig* GetTeam(int32 Index) const;

	/** Get number of teams */
	int32 GetTeamCount() const { return Teams.Num(); }

	/** Create default team setup (2 teams) */
	void SetupDefaultTeams();

	/** Validate settings */
	bool Validate(FString& OutError) const;

	/** Serialize to string for session settings */
	FString SerializeToString() const;

	/** Deserialize from string */
	static FDLLMultiplayerMatchSettings DeserializeFromString(const FString& Data);
};

/**
 * Preset match settings that can be selected in lobby.
 */
UCLASS(BlueprintType, Blueprintable)
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerMatchSettingsPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Display name for this preset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preset")
	FText PresetName;

	/** Description of this preset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preset", meta = (MultiLine = true))
	FText Description;

	/** The match settings for this preset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preset")
	FDLLMultiplayerMatchSettings Settings;

	/** Icon for UI display */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preset")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Is this preset visible in the UI? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preset")
	bool bIsVisible = true;

	/** Required session types for this preset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preset")
	TArray<EDLLMultiplayerSessionType> ApplicableSessionTypes;

	/** Check if this preset applies to a session type */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Preset")
	bool AppliesToSessionType(EDLLMultiplayerSessionType SessionType) const;
};
