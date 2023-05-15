//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SOFAFileFactory.generated.h"


// ---------------------------------------------------------------------------------------------------------------------
// USOFAFileFactory
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Instantiates a USOFAFile asset.
 */
UCLASS()
class USOFAFileFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Called when importing a .sofa file.
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, 
		UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, 
		FFeedbackContext* Warn) override;
};
