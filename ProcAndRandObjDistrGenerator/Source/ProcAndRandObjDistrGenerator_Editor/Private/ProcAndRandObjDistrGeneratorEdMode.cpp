// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProcAndRandObjDistrGeneratorEdMode.h"
#include "ProcAndRandObjDistrGeneratorEdModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"
#include "ProcGen/ProcGenManager.h"
#include "InputCoreTypes.h"

const FEditorModeID FProcAndRandObjDistrGeneratorEdMode::EM_ProcAndRandObjDistrGeneratorEdModeId = TEXT("EM_ProcAndRandObjDistrGeneratorEdMode");

//static FName LeftAltButtonName = TEXT("LeftAlt");
//static FName LeftControlButtonName = TEXT("LeftControl");
//static FName LeftShiftButtonName = TEXT("LeftShift");

FProcAndRandObjDistrGeneratorEdMode::FProcAndRandObjDistrGeneratorEdMode()
{
	bCtrlKeyIsActive = false;
	bMouseBtnLKeyIsActive = false;
	bMouseBtnRKeyIsActive = false;
}

FProcAndRandObjDistrGeneratorEdMode::~FProcAndRandObjDistrGeneratorEdMode()
{

}

void FProcAndRandObjDistrGeneratorEdMode::Enter()
{
	FEdMode::Enter();

	if (!Toolkit.IsValid() && UsesToolkits())
	{
		Toolkit = MakeShareable(new FProcAndRandObjDistrGeneratorEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}
}

void FProcAndRandObjDistrGeneratorEdMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	bCtrlKeyIsActive = false;
	bMouseBtnLKeyIsActive = false;
	bMouseBtnRKeyIsActive = false;

	UProcGenManager* pProcMan = UProcGenManager::GetCurManager();
	if (pProcMan)
	{
		pProcMan->bPaintMode = false;
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

bool FProcAndRandObjDistrGeneratorEdMode::UsesToolkits() const
{
	return true;
}

bool FProcAndRandObjDistrGeneratorEdMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return FEdMode::StartTracking(InViewportClient, InViewport);
}

bool FProcAndRandObjDistrGeneratorEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return FEdMode::EndTracking(InViewportClient, InViewport);
}

void FProcAndRandObjDistrGeneratorEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	UProcGenManager* pProcMan = UProcGenManager::GetCurManager();
	if (pProcMan)
	{
		if (bCtrlKeyIsActive)
		{
			if (bMouseBtnLKeyIsActive)
			{
				pProcMan->RequestPaint(true);
			}
			else if (bMouseBtnRKeyIsActive)
			{
				pProcMan->RequestPaint(false);
			}
		}
	}
}

bool FProcAndRandObjDistrGeneratorEdMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	if(IsSelectionAllowed(nullptr, true))
		return FEdMode::InputKey(InViewportClient, InViewport, InKey, InEvent);

	//if (!InKey.IsMouseButton())
	{
		//just test
		//FString MsgTextStr = FString::Printf(TEXT("FProcAndRandObjDistrGeneratorEdMode::InputKey key name %s"), *InKey.GetFName().ToString());
		//FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));

		if (InKey == EKeys::LeftMouseButton)
		{
			if (InEvent == IE_Pressed)
			{
				bMouseBtnLKeyIsActive = true;
			}
			else if (InEvent == IE_Released)
			{
				bMouseBtnLKeyIsActive = false;
			}
		}

		if (InKey == EKeys::LeftControl)
		{
			if (InEvent == IE_Pressed)
			{
				bCtrlKeyIsActive = true;
			}
			else if (InEvent == IE_Released)
			{
				bCtrlKeyIsActive = false;
			}
		}

		if (InKey == EKeys::RightMouseButton)
		{
			if (InEvent == IE_Pressed)
			{
				bMouseBtnRKeyIsActive = true;
			}
			else if (InEvent == IE_Released)
			{
				bMouseBtnRKeyIsActive = false;
			}
		}
	}
	return FEdMode::InputKey(InViewportClient, InViewport, InKey, InEvent);
}

bool FProcAndRandObjDistrGeneratorEdMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

bool FProcAndRandObjDistrGeneratorEdMode::Select(AActor* InActor, bool bInSelected)
{
	return FEdMode::Select(InActor, bInSelected);
}

bool FProcAndRandObjDistrGeneratorEdMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	if (UProcGenManager::GetCurManager())
	{
		if (UProcGenManager::GetCurManager()->bPaintMode)
		{
			return false;
		}
	}
	return true;
}




