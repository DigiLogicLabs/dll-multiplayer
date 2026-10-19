// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#include "Widgets/DLLMultiplayerSessionBrowserWidget.h"
#include "DLLMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Kismet/GameplayStatics.h"

void UDLLMultiplayerSessionBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Get subsystem
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(GetWorld());
	if (GameInstance)
	{
		SessionsSubsystem = GameInstance->GetSubsystem<UDLLMultiplayerSubsystem>();
	}

	// Bind buttons
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &ThisClass::OnRefreshClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::OnJoinClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &ThisClass::OnBackClicked);
	}

	// Bind to subsystem events
	if (SessionsSubsystem)
	{
		SessionsSubsystem->OnSessionsFound.AddDynamic(this, &ThisClass::HandleSessionsFound);
		SessionsSubsystem->OnSessionJoined.AddDynamic(this, &ThisClass::HandleJoinComplete);
		SessionsSubsystem->OnSessionError.AddDynamic(this, &ThisClass::HandleSessionError);
	}
}

void UDLLMultiplayerSessionBrowserWidget::NativeDestruct()
{
	// Unbind from subsystem events
	if (SessionsSubsystem)
	{
		SessionsSubsystem->OnSessionsFound.RemoveDynamic(this, &ThisClass::HandleSessionsFound);
		SessionsSubsystem->OnSessionJoined.RemoveDynamic(this, &ThisClass::HandleJoinComplete);
		SessionsSubsystem->OnSessionError.RemoveDynamic(this, &ThisClass::HandleSessionError);
	}

	Super::NativeDestruct();
}

void UDLLMultiplayerSessionBrowserWidget::SetupBrowser(const FDLLMultiplayerSessionSearchFilter& DefaultFilter)
{
	CurrentFilter = DefaultFilter;
	bIsInitialized = true;
	UpdateUI();
	RefreshSessionList();
}

void UDLLMultiplayerSessionBrowserWidget::SetupBrowserDefaults()
{
	FDLLMultiplayerSessionSearchFilter DefaultFilter;
	DefaultFilter.MaxResults = 100;
	DefaultFilter.bOnlyShowAvailable = true;
	DefaultFilter.bOnlyShowJoinable = true;
	DefaultFilter.bIncludeLAN = true;
	SetupBrowser(DefaultFilter);
}

void UDLLMultiplayerSessionBrowserWidget::RefreshSessionList()
{
	if (!SessionsSubsystem)
	{
		OnBrowserError(TEXT("NoSubsystem"), TEXT("Session subsystem not available."));
		return;
	}

	ClearSelection();

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Searching...")));
	}

	SessionsSubsystem->FindSessionsFiltered(CurrentFilter);
}

void UDLLMultiplayerSessionBrowserWidget::ApplyFilter(const FDLLMultiplayerSessionSearchFilter& NewFilter)
{
	CurrentFilter = NewFilter;
	RefreshSessionList();
}

void UDLLMultiplayerSessionBrowserWidget::SetMatchTypeFilter(const FString& MatchType)
{
	CurrentFilter.MatchType = MatchType;
	RefreshSessionList();
}

bool UDLLMultiplayerSessionBrowserWidget::IsSearching() const
{
	return SessionsSubsystem ? SessionsSubsystem->IsSearchingForSessions() : false;
}

void UDLLMultiplayerSessionBrowserWidget::SelectSessionByIndex(int32 Index)
{
	if (Index < 0 || Index >= CachedSessions.Num())
	{
		ClearSelection();
		return;
	}

	SelectedIndex = Index;
	SelectedSession = CachedSessions[Index];

	UpdateUI();
	OnSessionSelected(SelectedSession);
}

void UDLLMultiplayerSessionBrowserWidget::ClearSelection()
{
	SelectedIndex = -1;
	SelectedSession = FDLLMultiplayerSessionInfo();
	UpdateUI();
}

void UDLLMultiplayerSessionBrowserWidget::JoinSelectedSession(const FString& Password)
{
	if (!HasSelectedSession())
	{
		OnBrowserError(TEXT("NoSelection"), TEXT("No session selected."));
		return;
	}

	OnJoiningSession(SelectedSession);

	if (SessionsSubsystem)
	{
		SessionsSubsystem->JoinSessionByInfo(SelectedSession, Password);
	}
}

void UDLLMultiplayerSessionBrowserWidget::JoinSessionByIndex(int32 Index, const FString& Password)
{
	SelectSessionByIndex(Index);
	if (HasSelectedSession())
	{
		JoinSelectedSession(Password);
	}
}

TArray<FDLLMultiplayerSessionInfo> UDLLMultiplayerSessionBrowserWidget::GetSessionList() const
{
	return CachedSessions;
}

int32 UDLLMultiplayerSessionBrowserWidget::GetSessionCount() const
{
	return CachedSessions.Num();
}

FDLLMultiplayerSessionInfo UDLLMultiplayerSessionBrowserWidget::GetSessionAtIndex(int32 Index) const
{
	if (Index >= 0 && Index < CachedSessions.Num())
	{
		return CachedSessions[Index];
	}
	return FDLLMultiplayerSessionInfo();
}

void UDLLMultiplayerSessionBrowserWidget::SortSessions(EDLLMultiplayerSessionSortMode SortMode, bool bAscending)
{
	auto SortPredicate = [SortMode, bAscending](const FDLLMultiplayerSessionInfo& A, const FDLLMultiplayerSessionInfo& B) -> bool
	{
		bool Result = false;

		switch (SortMode)
		{
			case EDLLMultiplayerSessionSortMode::ByPing:
				Result = A.PingMs < B.PingMs;
				break;
			case EDLLMultiplayerSessionSortMode::ByPlayerCount:
				Result = A.CurrentPlayers < B.CurrentPlayers;
				break;
			case EDLLMultiplayerSessionSortMode::ByName:
				Result = A.SessionName < B.SessionName;
				break;
			case EDLLMultiplayerSessionSortMode::ByMapName:
				Result = A.MapName < B.MapName;
				break;
			case EDLLMultiplayerSessionSortMode::ByGameMode:
				Result = A.GameMode < B.GameMode;
				break;
		}

		return bAscending ? Result : !Result;
	};

	CachedSessions.Sort(SortPredicate);

	// Update selection if needed
	if (SelectedSession.IsValid())
	{
		for (int32 i = 0; i < CachedSessions.Num(); ++i)
		{
			if (CachedSessions[i].SessionName == SelectedSession.SessionName)
			{
				SelectedIndex = i;
				break;
			}
		}
	}

	OnSessionListUpdated(CachedSessions, true);
}

void UDLLMultiplayerSessionBrowserWidget::HandleSessionsFound(const TArray<FDLLMultiplayerSessionInfo>& Sessions, bool bWasSuccessful)
{
	CachedSessions = Sessions;

	if (StatusText)
	{
		if (bWasSuccessful)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("Found %d sessions"), Sessions.Num())));
		}
		else
		{
			StatusText->SetText(FText::FromString(TEXT("Search failed")));
		}
	}

	UpdateUI();
	OnSessionListUpdated(Sessions, bWasSuccessful);
}

void UDLLMultiplayerSessionBrowserWidget::HandleJoinComplete(EDLLMultiplayerJoinSessionResult Result, const FDLLMultiplayerSessionInfo& SessionInfo)
{
	OnJoinSessionComplete(Result, SessionInfo);
}

void UDLLMultiplayerSessionBrowserWidget::HandleSessionError(const FString& ErrorCode, const FString& ErrorMessage)
{
	OnBrowserError(ErrorCode, ErrorMessage);
}

void UDLLMultiplayerSessionBrowserWidget::OnRefreshClicked()
{
	RefreshSessionList();
}

void UDLLMultiplayerSessionBrowserWidget::OnJoinClicked()
{
	FString Password;
	if (PasswordInput)
	{
		Password = PasswordInput->GetText().ToString();
	}

	JoinSelectedSession(Password);
}

void UDLLMultiplayerSessionBrowserWidget::OnBackClicked()
{
	RemoveFromParent();
}

void UDLLMultiplayerSessionBrowserWidget::UpdateUI()
{
	// Update session count
	if (SessionCountText)
	{
		SessionCountText->SetText(FText::FromString(FString::Printf(TEXT("%d Sessions"), CachedSessions.Num())));
	}

	// Update join button state
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(HasSelectedSession() && !IsSearching());
	}

	// Update refresh button state
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(!IsSearching());
	}

	// Show/hide password input
	if (PasswordInput)
	{
		bool bShowPassword = HasSelectedSession() && SelectedSession.bHasPassword;
		PasswordInput->SetVisibility(bShowPassword ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
