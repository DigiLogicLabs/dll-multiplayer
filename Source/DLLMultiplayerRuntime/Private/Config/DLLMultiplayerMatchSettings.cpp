// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Config/DLLMultiplayerMatchSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

FDLLMultiplayerMatchSettings::FDLLMultiplayerMatchSettings()
{
	SettingsName = TEXT("Default Match");
}

const FDLLMultiplayerTeamConfig* FDLLMultiplayerMatchSettings::GetTeam(int32 Index) const
{
	if (Index >= 0 && Index < Teams.Num())
	{
		return &Teams[Index];
	}
	return nullptr;
}

void FDLLMultiplayerMatchSettings::SetupDefaultTeams()
{
	Teams.Empty();

	// Red Team
	FDLLMultiplayerTeamConfig RedTeam;
	RedTeam.TeamIndex = 0;
	RedTeam.TeamName = FText::FromString(TEXT("Red Team"));
	RedTeam.TeamColor = FLinearColor(1.0f, 0.2f, 0.2f);
	Teams.Add(RedTeam);

	// Blue Team
	FDLLMultiplayerTeamConfig BlueTeam;
	BlueTeam.TeamIndex = 1;
	BlueTeam.TeamName = FText::FromString(TEXT("Blue Team"));
	BlueTeam.TeamColor = FLinearColor(0.2f, 0.4f, 1.0f);
	Teams.Add(BlueTeam);
}

bool FDLLMultiplayerMatchSettings::Validate(FString& OutError) const
{
	// Check for at least one win condition if not survival
	if (!IsSurvivalMode())
	{
		if (ScoreLimit <= 0 && TimeLimitSeconds <= 0 && StockCount <= 0)
		{
			OutError = TEXT("At least one win condition (score, time, or stock) must be set.");
			return false;
		}
	}

	// Check team mode has teams configured
	if (IsTeamMode() && Teams.Num() < 2)
	{
		OutError = TEXT("Team mode requires at least 2 teams.");
		return false;
	}

	// Check respawn time is valid
	if (RespawnTimeSeconds < 0.0f)
	{
		OutError = TEXT("Respawn time cannot be negative.");
		return false;
	}

	// Check difficulty multiplier is in range
	if (DifficultyMultiplier < 0.5f || DifficultyMultiplier > 3.0f)
	{
		OutError = TEXT("Difficulty multiplier must be between 0.5 and 3.0.");
		return false;
	}

	return true;
}

FString FDLLMultiplayerMatchSettings::SerializeToString() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	// Core settings
	JsonObject->SetStringField(TEXT("SettingsName"), SettingsName);
	JsonObject->SetNumberField(TEXT("SessionType"), static_cast<int32>(SessionType));
	JsonObject->SetStringField(TEXT("MapPath"), MapPath.ToString());
	JsonObject->SetStringField(TEXT("GameModeClass"), GameModeClass.ToString());

	// Win conditions
	JsonObject->SetNumberField(TEXT("ScoreLimit"), ScoreLimit);
	JsonObject->SetNumberField(TEXT("TimeLimitSeconds"), TimeLimitSeconds);
	JsonObject->SetNumberField(TEXT("RoundLimit"), RoundLimit);
	JsonObject->SetNumberField(TEXT("StockCount"), StockCount);

	// Gameplay rules
	JsonObject->SetBoolField(TEXT("bFriendlyFire"), bFriendlyFire);
	JsonObject->SetNumberField(TEXT("FriendlyFireDamageMultiplier"), FriendlyFireDamageMultiplier);
	JsonObject->SetBoolField(TEXT("bWeaponPickups"), bWeaponPickups);
	JsonObject->SetBoolField(TEXT("bHealthPickups"), bHealthPickups);
	JsonObject->SetBoolField(TEXT("bPowerUps"), bPowerUps);
	JsonObject->SetNumberField(TEXT("RespawnTimeSeconds"), RespawnTimeSeconds);
	JsonObject->SetNumberField(TEXT("SpawnInvulnerabilitySeconds"), SpawnInvulnerabilitySeconds);

	// Team settings
	JsonObject->SetBoolField(TEXT("bAutoBalanceTeams"), bAutoBalanceTeams);
	JsonObject->SetBoolField(TEXT("bAllowTeamSwitching"), bAllowTeamSwitching);

	// Horde settings
	JsonObject->SetNumberField(TEXT("StartingWave"), StartingWave);
	JsonObject->SetNumberField(TEXT("TargetWave"), TargetWave);
	JsonObject->SetNumberField(TEXT("DifficultyMultiplier"), DifficultyMultiplier);

	// Mutators
	JsonObject->SetStringField(TEXT("EnabledMutators"), EnabledMutators.ToString());

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

FDLLMultiplayerMatchSettings FDLLMultiplayerMatchSettings::DeserializeFromString(const FString& Data)
{
	FDLLMultiplayerMatchSettings Settings;

	if (Data.IsEmpty())
	{
		return Settings;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FDLLMultiplayerMatchSettings::DeserializeFromString: Failed to parse JSON"));
		return Settings;
	}

	// Core settings
	Settings.SettingsName = JsonObject->GetStringField(TEXT("SettingsName"));
	Settings.SessionType = static_cast<EDLLMultiplayerSessionType>(JsonObject->GetIntegerField(TEXT("SessionType")));
	Settings.MapPath = FSoftObjectPath(JsonObject->GetStringField(TEXT("MapPath")));
	Settings.GameModeClass = FSoftClassPath(JsonObject->GetStringField(TEXT("GameModeClass")));

	// Win conditions
	Settings.ScoreLimit = JsonObject->GetIntegerField(TEXT("ScoreLimit"));
	Settings.TimeLimitSeconds = JsonObject->GetIntegerField(TEXT("TimeLimitSeconds"));
	Settings.RoundLimit = JsonObject->GetIntegerField(TEXT("RoundLimit"));
	Settings.StockCount = JsonObject->GetIntegerField(TEXT("StockCount"));

	// Gameplay rules
	Settings.bFriendlyFire = JsonObject->GetBoolField(TEXT("bFriendlyFire"));
	Settings.FriendlyFireDamageMultiplier = JsonObject->GetNumberField(TEXT("FriendlyFireDamageMultiplier"));
	Settings.bWeaponPickups = JsonObject->GetBoolField(TEXT("bWeaponPickups"));
	Settings.bHealthPickups = JsonObject->GetBoolField(TEXT("bHealthPickups"));
	Settings.bPowerUps = JsonObject->GetBoolField(TEXT("bPowerUps"));
	Settings.RespawnTimeSeconds = JsonObject->GetNumberField(TEXT("RespawnTimeSeconds"));
	Settings.SpawnInvulnerabilitySeconds = JsonObject->GetNumberField(TEXT("SpawnInvulnerabilitySeconds"));

	// Team settings
	Settings.bAutoBalanceTeams = JsonObject->GetBoolField(TEXT("bAutoBalanceTeams"));
	Settings.bAllowTeamSwitching = JsonObject->GetBoolField(TEXT("bAllowTeamSwitching"));

	// Horde settings
	Settings.StartingWave = JsonObject->GetIntegerField(TEXT("StartingWave"));
	Settings.TargetWave = JsonObject->GetIntegerField(TEXT("TargetWave"));
	Settings.DifficultyMultiplier = JsonObject->GetNumberField(TEXT("DifficultyMultiplier"));

	// Mutators
	FString MutatorString = JsonObject->GetStringField(TEXT("EnabledMutators"));
	Settings.EnabledMutators.FromExportString(MutatorString);

	return Settings;
}

// UDLLMultiplayerMatchSettingsPreset implementation

bool UDLLMultiplayerMatchSettingsPreset::AppliesToSessionType(EDLLMultiplayerSessionType SessionType) const
{
	// If no applicable types specified, applies to all
	if (ApplicableSessionTypes.Num() == 0)
	{
		return true;
	}

	return ApplicableSessionTypes.Contains(SessionType);
}
