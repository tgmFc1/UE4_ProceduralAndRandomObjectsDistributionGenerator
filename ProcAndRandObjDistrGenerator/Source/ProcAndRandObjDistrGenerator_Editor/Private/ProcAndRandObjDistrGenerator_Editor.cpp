// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProcAndRandObjDistrGenerator_Editor.h"
#include "ProcAndRandObjDistrGeneratorEdMode.h"
#include "ProcGen/ProcGenManager.h"

#define LOCTEXT_NAMESPACE "FProcAndRandObjDistrGenerator_Editor"

FProcAndRandObjDistrGenerator_Editor::FProcAndRandObjDistrGenerator_Editor()
{
	
}

void FProcAndRandObjDistrGenerator_Editor::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorModeRegistry::Get().RegisterMode<FProcAndRandObjDistrGeneratorEdMode>(FProcAndRandObjDistrGeneratorEdMode::EM_ProcAndRandObjDistrGeneratorEdModeId, LOCTEXT("ProcAndRandObjDistrGeneratorEdModeName", "ProcAndRandObjDistrGeneratorEdMode"), FSlateIcon(), true);
}

void FProcAndRandObjDistrGenerator_Editor::ShutdownModule()
{
	if (UProcGenManager::GetCurManager())
	{
		UProcGenManager::GetCurManager()->bPaintMode = false;
	}
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FEditorModeRegistry::Get().UnregisterMode(FProcAndRandObjDistrGeneratorEdMode::EM_ProcAndRandObjDistrGeneratorEdModeId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FProcAndRandObjDistrGenerator_Editor, ProcAndRandObjDistrGenerator_Editor)