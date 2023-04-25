// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProcAndRandObjDistrGenerator.h"
#include "ProcAndRandObjDistrGeneratorEdMode.h"
#include "ProcGen/ProcGenManager.h"

#define LOCTEXT_NAMESPACE "FProcAndRandObjDistrGeneratorModule"

FProcAndRandObjDistrGeneratorModule::FProcAndRandObjDistrGeneratorModule()
{
	ProcGenManagerPtr = nullptr;
}

//static UProcGenManager* ProcGenManagerPtr = nullptr;

void FProcAndRandObjDistrGeneratorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorModeRegistry::Get().RegisterMode<FProcAndRandObjDistrGeneratorEdMode>(FProcAndRandObjDistrGeneratorEdMode::EM_ProcAndRandObjDistrGeneratorEdModeId, LOCTEXT("ProcAndRandObjDistrGeneratorEdModeName", "ProcAndRandObjDistrGeneratorEdMode"), FSlateIcon(), true);
	UProcGenManager* DefProcGenManager = Cast<UProcGenManager>(UProcGenManager::StaticClass()->GetDefaultObject(true));
	if (!DefProcGenManager)
	{
		ProcGenManagerPtr = NewObject<UProcGenManager>();
		ProcGenManagerPtr->SetFlags(RF_MarkAsRootSet);
		ProcGenManagerPtr->AddToRoot();
		ProcGenManagerPtr->SetCurManagerAsThisObject();
	}
	else
	{
		DefProcGenManager->SetCurManagerAsThisObject();
	}
	//ProcGenManagerPtr = nullptr
}

void FProcAndRandObjDistrGeneratorModule::ShutdownModule()
{
	if (UProcGenManager::GetCurManager())
	{
		UProcGenManager::GetCurManager()->bPaintMode = false;
	}
	/*ProcGenManagerPtr->ClearFlags(RF_MarkAsRootSet);
	if (ProcGenManagerPtr->IsDestructionThreadSafe())
	{
		ProcGenManagerPtr->ConditionalBeginDestroy();
	}
	ProcGenManagerPtr = nullptr;*/
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FEditorModeRegistry::Get().UnregisterMode(FProcAndRandObjDistrGeneratorEdMode::EM_ProcAndRandObjDistrGeneratorEdModeId);
}

UProcGenManager* FProcAndRandObjDistrGeneratorModule::GetProcGenManager()
{
	IModuleInterface* genModule = FModuleManager::Get().GetModule(TEXT("ProcAndRandObjDistrGenerator"));
	if (genModule)
	{
		FProcAndRandObjDistrGeneratorModule* moduleRt = static_cast<FProcAndRandObjDistrGeneratorModule*>(genModule);
		if (moduleRt)
		{
			return moduleRt->ProcGenManagerPtr;
		}
	}
	return nullptr;
	//return ProcGenManagerPtr;
}

void FProcAndRandObjDistrGeneratorModule::SetProcGenManager(UProcGenManager* ManPtr)
{
	//ProcGenManagerPtr = ManPtr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FProcAndRandObjDistrGeneratorModule, ProcAndRandObjDistrGenerator)