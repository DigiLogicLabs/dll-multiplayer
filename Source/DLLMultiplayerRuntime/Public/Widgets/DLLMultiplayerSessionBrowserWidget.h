// Copyright Digi Logic Labs LLC. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/DLLMultiplayerSessionTypes.h"
#include "DLLMultiplayerSessionBrowserWidget.generated.h"

class UDLLMultiplayerSubsystem;
class UButton;
class UTextBlock;
class UListView;
class UEditableTextBox;

/** Sort options for the session list */
UENUM(BlueprintType)
enum class EDLLMultiplayerSessionSortMode : uint8
{
	ByPing,
	ByPlayerCount,
	ByName,
	ByMapName,
	ByGameMode
};

/**
 * Base widget for browsing and joining multiplayer sessions.
 * Extend this in Blueprint to create your custom server browser UI.
 *
 * Features:
 * - Session list display
 * - Search filtering
 * - Sorting options
 * - Refresh functionality
 * - Join with password support
 *
 * Usage:
 * 1. Create a Widget Blueprint derived from this class
 * 2. Add UI elements bound via meta=(BindWidget)
 * 3. Call SetupBrowser() to initialize
 * 4. Implement OnSessionSelected in Blueprint
 */
UCLASS(Abstract, Blueprintable)
class DLLMULTIPLAYERRUNTIME_API UDLLMultiplayerSessionBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ========================================================================
	// SETUP
	// ========================================================================

	/**
	 * Initialize the session browser.
	 * @param DefaultFilter Initial search filter settings
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void SetupBrowser(const FDLLMultiplayerSessionSearchFilter& DefaultFilter);

	/**
	 * Setup with default filter settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void SetupBrowserDefaults();

	// ========================================================================
	// SEARCH & REFRESH
	// ========================================================================

	/**
	 * Refresh the session list using current filter.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void RefreshSessionList();

	/**
	 * Apply new filter settings and refresh.
	 * @param NewFilter The new filter to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void ApplyFilter(const FDLLMultiplayerSessionSearchFilter& NewFilter);

	/**
	 * Set quick filter by match type.
	 * @param MatchType Match type string to filter by (empty = all)
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void SetMatchTypeFilter(const FString& MatchType);

	/**
	 * Check if currently searching for sessions.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	bool IsSearching() const;

	// ========================================================================
	// SESSION SELECTION
	// ========================================================================

	/**
	 * Get the currently selected session info.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	FDLLMultiplayerSessionInfo GetSelectedSession() const { return SelectedSession; }

	/**
	 * Check if a session is selected.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	bool HasSelectedSession() const { return SelectedSession.IsValid(); }

	/**
	 * Select a session by index.
	 * @param Index Index in the session list
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void SelectSessionByIndex(int32 Index);

	/**
	 * Clear the current selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void ClearSelection();

	// ========================================================================
	// JOINING
	// ========================================================================

	/**
	 * Attempt to join the selected session.
	 * @param Password Password if required
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void JoinSelectedSession(const FString& Password = TEXT(""));

	/**
	 * Join a session by index.
	 * @param Index Index in the session list
	 * @param Password Password if required
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void JoinSessionByIndex(int32 Index, const FString& Password = TEXT(""));

	// ========================================================================
	// DATA ACCESS
	// ========================================================================

	/**
	 * Get all sessions from the last search.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	TArray<FDLLMultiplayerSessionInfo> GetSessionList() const;

	/**
	 * Get the number of sessions found.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	int32 GetSessionCount() const;

	/**
	 * Get session info by index.
	 * @param Index Index in the session list
	 * @return Session info (check IsValid())
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	FDLLMultiplayerSessionInfo GetSessionAtIndex(int32 Index) const;

	/**
	 * Get the current filter settings.
	 */
	UFUNCTION(BlueprintPure, Category = "DLL Multiplayer|Session Browser")
	FDLLMultiplayerSessionSearchFilter GetCurrentFilter() const { return CurrentFilter; }

	// ========================================================================
	// SORTING
	// ========================================================================

	/**
	 * Sort the session list.
	 * @param SortMode How to sort the sessions
	 * @param bAscending True for ascending, false for descending
	 */
	UFUNCTION(BlueprintCallable, Category = "DLL Multiplayer|Session Browser")
	void SortSessions(EDLLMultiplayerSessionSortMode SortMode, bool bAscending = true);

	// ========================================================================
	// EVENTS (Implement in Blueprint)
	// ========================================================================

	/**
	 * Called when the session list is updated.
	 * @param Sessions The updated session list
	 * @param bWasSuccessful True if search was successful
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "DLL Multiplayer|Session Browser|Events")
	void OnSessionListUpdated(const TArray<FDLLMultiplayerSessionInfo>& Sessions, bool bWasSuccessful);

	/**
	 * Called when a session is selected.
	 * @param SessionInfo The selected session info
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "DLL Multiplayer|Session Browser|Events")
	void OnSessionSelected(const FDLLMultiplayerSessionInfo& SessionInfo);

	/**
	 * Called when joining a session starts.
	 * @param SessionInfo The session being joined
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "DLL Multiplayer|Session Browser|Events")
	void OnJoiningSession(const FDLLMultiplayerSessionInfo& SessionInfo);

	/**
	 * Called when joining a session completes.
	 * @param Result The join result
	 * @param SessionInfo The session joined (if successful)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "DLL Multiplayer|Session Browser|Events")
	void OnJoinSessionComplete(EDLLMultiplayerJoinSessionResult Result, const FDLLMultiplayerSessionInfo& SessionInfo);

	/**
	 * Called when an error occurs.
	 * @param ErrorCode Machine-readable error code
	 * @param ErrorMessage Human-readable error message
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "DLL Multiplayer|Session Browser|Events")
	void OnBrowserError(const FString& ErrorCode, const FString& ErrorMessage);

protected:
	// ========================================================================
	// WIDGET BINDINGS (Optional - bind in Blueprint)
	// ========================================================================

	/** Refresh button */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	/** Join button */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinButton;

	/** Back/Cancel button */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	/** Session count text */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SessionCountText;

	/** Status/searching text */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Password input field */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> PasswordInput;

	// ========================================================================
	// INTERNAL
	// ========================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Handle sessions found from subsystem */
	UFUNCTION()
	void HandleSessionsFound(const TArray<FDLLMultiplayerSessionInfo>& Sessions, bool bWasSuccessful);

	/** Handle join result from subsystem */
	UFUNCTION()
	void HandleJoinComplete(EDLLMultiplayerJoinSessionResult Result, const FDLLMultiplayerSessionInfo& SessionInfo);

	/** Handle session error from subsystem */
	UFUNCTION()
	void HandleSessionError(const FString& ErrorCode, const FString& ErrorMessage);

	/** Button click handlers */
	UFUNCTION()
	void OnRefreshClicked();

	UFUNCTION()
	void OnJoinClicked();

	UFUNCTION()
	void OnBackClicked();

	/** Update UI state */
	void UpdateUI();

private:
	/** Reference to the sessions subsystem */
	UPROPERTY()
	TObjectPtr<UDLLMultiplayerSubsystem> SessionsSubsystem;

	/** Current search filter */
	FDLLMultiplayerSessionSearchFilter CurrentFilter;

	/** Cached session list (may be sorted differently than subsystem cache) */
	TArray<FDLLMultiplayerSessionInfo> CachedSessions;

	/** Currently selected session */
	FDLLMultiplayerSessionInfo SelectedSession;

	/** Currently selected index */
	int32 SelectedIndex = -1;

	/** Is browser initialized */
	bool bIsInitialized = false;
};
