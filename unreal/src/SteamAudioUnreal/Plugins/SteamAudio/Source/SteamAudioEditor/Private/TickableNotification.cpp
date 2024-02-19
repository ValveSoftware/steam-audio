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

#include "TickableNotification.h"
#include "LevelEditor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopeLock.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FWorkItem
// ---------------------------------------------------------------------------------------------------------------------

FWorkItem::FWorkItem()
    : Task(nullptr)
    , FinalState(SNotificationItem::CS_Success)
    , bIsFinalItem(false)
{}

FWorkItem::FWorkItem(const TFunction<void(FText&)>& Task, const SNotificationItem::ECompletionState InFinalState, const bool bIsFinalItem)
    : Task(Task)
    , FinalState(InFinalState)
    , bIsFinalItem(bIsFinalItem)
{}


// ---------------------------------------------------------------------------------------------------------------------
// FTickableNotification
// ---------------------------------------------------------------------------------------------------------------------

FTickableNotification::FTickableNotification()
    : bIsTicking(false)
{}

void FTickableNotification::CreateNotification()
{
    FNotificationInfo Info(DisplayText);
    Info.bFireAndForget = false;
    Info.FadeOutDuration = 4.0f;
    Info.ExpireDuration = 0.0f;

    NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
    NotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

    bIsTicking = true;
}

void FTickableNotification::CreateNotificationWithCancel(const FSimpleDelegate& CancelDelegate)
{
    FNotificationInfo Info(DisplayText);
    Info.bFireAndForget = false;
    Info.FadeOutDuration = 4.0f;
    Info.ExpireDuration = 0.0f;
    Info.ButtonDetails.Add(FNotificationButtonInfo(NSLOCTEXT("SteamAudio", "Cancel", "Cancel"), FText::GetEmpty(), CancelDelegate));

    NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
    NotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

    bIsTicking = true;
}

void FTickableNotification::DestroyNotification(const SNotificationItem::ECompletionState InFinalState)
{
    bIsTicking = false;
    this->FinalState = InFinalState;
}

void FTickableNotification::SetDisplayText(const FText& InDisplayText)
{
    FScopeLock Lock(&CriticalSection);

    this->DisplayText = InDisplayText;
}

void FTickableNotification::QueueWorkItem(const FWorkItem& WorkItem)
{
    FScopeLock Lock(&CriticalSection);

    WorkQueue.Enqueue(WorkItem);
}

void FTickableNotification::NotifyDestruction()
{
    if (!NotificationPtr.Pin().IsValid())
    {
        return;
    }

    NotificationPtr.Pin()->SetText(DisplayText);
    NotificationPtr.Pin()->SetCompletionState(FinalState);
    NotificationPtr.Pin()->ExpireAndFadeout();
    NotificationPtr.Reset();
}

void FTickableNotification::Tick(float DeltaTime)
{
    FScopeLock Lock(&CriticalSection);

    if (bIsTicking && NotificationPtr.Pin().IsValid())
    {
        if (!WorkQueue.IsEmpty())
        {
            FWorkItem WorkItem;
            WorkQueue.Dequeue(WorkItem);
            WorkItem.Task(DisplayText);
            FinalState = WorkItem.FinalState;
            bIsTicking = !WorkItem.bIsFinalItem;
        }

        NotificationPtr.Pin()->SetText(DisplayText);
    }
    else
    {
        NotifyDestruction();
    }
}

TStatId FTickableNotification::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(FTickableNotification, STATGROUP_Tickables);
}

}
