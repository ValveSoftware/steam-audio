//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include "SteamAudioEditorModule.h"
#include "TickableEditorObject.h"
#include "Containers/Queue.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FWorkItem
// ---------------------------------------------------------------------------------------------------------------------

struct FWorkItem
{
    FWorkItem();

    FWorkItem(const TFunction<void(FText&)>& Task, const SNotificationItem::ECompletionState InFinalState, const bool bIsFinalItem);

    TFunction<void(FText&)> Task;
    SNotificationItem::ECompletionState FinalState;
    bool bIsFinalItem;
};


// ---------------------------------------------------------------------------------------------------------------------
// FTickableNotification
// ---------------------------------------------------------------------------------------------------------------------

class FTickableNotification : public FTickableEditorObject
{
public:
    FTickableNotification();

    void CreateNotification();

    void CreateNotificationWithCancel(const FSimpleDelegate& CancelDelegate);

    void DestroyNotification(const SNotificationItem::ECompletionState InFinalState = SNotificationItem::CS_Success);

    void SetDisplayText(const FText& InDisplayText);

    void QueueWorkItem(const FWorkItem& WorkItem);

protected:
    virtual void Tick(float DeltaTime) override;

    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

    virtual TStatId GetStatId() const override;

private:
    void NotifyDestruction();

    TWeakPtr<SNotificationItem> NotificationPtr;
    FCriticalSection CriticalSection;
    FText DisplayText;
    bool bIsTicking;
    TQueue<FWorkItem> WorkQueue;
    SNotificationItem::ECompletionState FinalState;
};

}
