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
