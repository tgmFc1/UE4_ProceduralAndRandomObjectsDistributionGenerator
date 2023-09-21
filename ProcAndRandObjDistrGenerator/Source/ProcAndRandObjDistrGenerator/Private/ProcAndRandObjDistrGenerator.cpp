// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProcAndRandObjDistrGenerator.h"
#include "ProcGen/ProcGenManager.h"

#define LOCTEXT_NAMESPACE "FProcAndRandObjDistrGeneratorModule"

FProcAndRandObjDistrGeneratorModule::FProcAndRandObjDistrGeneratorModule()
{
	
}

void FProcAndRandObjDistrGeneratorModule::StartupModule()
{
	UProcGenManager* DefProcGenManager = Cast<UProcGenManager>(UProcGenManager::StaticClass()->GetDefaultObject(true));
	if (!DefProcGenManager)
	{
		UProcGenManager* ProcGenManagerPtr = nullptr;
		ProcGenManagerPtr = NewObject<UProcGenManager>();
		ProcGenManagerPtr->SetFlags(RF_MarkAsRootSet);
		ProcGenManagerPtr->AddToRoot();
		ProcGenManagerPtr->SetCurManagerAsThisObject();
	}
	else
	{
		DefProcGenManager->SetCurManagerAsThisObject();
		DefProcGenManager->SetFlags(RF_MarkAsRootSet);
		DefProcGenManager->AddToRoot();
	}
}

void FProcAndRandObjDistrGeneratorModule::ShutdownModule()
{
	if (UProcGenManager::GetCurManager())
	{
		UProcGenManager::GetCurManager()->bPaintMode = false;
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FProcAndRandObjDistrGeneratorModule, ProcAndRandObjDistrGenerator)