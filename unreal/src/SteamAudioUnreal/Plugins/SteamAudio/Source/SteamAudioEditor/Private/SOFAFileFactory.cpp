//
// Copyright (C) Valve Corporation. All rights reserved.
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
