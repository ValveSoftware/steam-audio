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

#include "SOFAFileFactory.h"
#include "SOFAFile.h"
#include "Misc/FeedbackContext.h"


// ---------------------------------------------------------------------------------------------------------------------
// USOFAFileFactory
// ---------------------------------------------------------------------------------------------------------------------

USOFAFileFactory::USOFAFileFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = USOFAFile::StaticClass();

    bCreateNew = false;
    bEditorImport = true;

    Formats.Add(TEXT("sofa;SOFA file with HRTF data"));
}

UObject* USOFAFileFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
    UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
    GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

    USOFAFile* SOFAFile = NewObject<USOFAFile>(InParent, InName, Flags);
    if (SOFAFile)
    {
        SOFAFile->Name = InName.ToString();

        SOFAFile->Data.Empty(BufferEnd - Buffer);
        SOFAFile->Data.AddUninitialized(BufferEnd - Buffer);
        FMemory::Memcpy(SOFAFile->Data.GetData(), Buffer, SOFAFile->Data.Num());

        SOFAFile->Volume = 0.0f;
        SOFAFile->NormalizationType = EHRTFNormType::NONE;
    }
    else
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("Unable to load SOFA file %s"), *InName.ToString());
    }

    GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, SOFAFile);

    return SOFAFile;
}
