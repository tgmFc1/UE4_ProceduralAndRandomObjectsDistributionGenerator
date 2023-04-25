// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FProcAndRandObjDistrGeneratorModule : public IModuleInterface
{
public:

	FProcAndRandObjDistrGeneratorModule();
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static class UProcGenManager* GetProcGenManager();
	static void SetProcGenManager(class UProcGenManager* ManPtr);
protected:
	class UProcGenManager* ProcGenManagerPtr = nullptr;
};
