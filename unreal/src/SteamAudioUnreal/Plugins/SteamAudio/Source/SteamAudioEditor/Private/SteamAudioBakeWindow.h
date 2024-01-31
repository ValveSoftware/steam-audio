//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "CoreMinimal.h"
#include "SteamAudioBaking.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FBakeWindow
// ---------------------------------------------------------------------------------------------------------------------

struct FBakeWindowRow
{
    EBakeTaskType Type;
    AActor* Actor;
    int Size;
};

class FBakeWindow : public TSharedFromThis<FBakeWindow>
{
public:
    FBakeWindow();
    ~FBakeWindow();

    void Invoke();

private:
    TArray<TSharedPtr<FBakeWindowRow>> BakeWindowRows;
    TSharedPtr<SListView<TSharedPtr<FBakeWindowRow>>> ListView;

    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& SpawnTabArgs);
    TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FBakeWindowRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
    bool IsBakeEnabled() const;
    FReply OnBakeSelected();
    void OnBakeComplete();
    void RefreshBakeTasks();
};

}
