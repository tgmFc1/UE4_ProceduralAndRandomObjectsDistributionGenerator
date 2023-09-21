// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class FProcAndRandObjDistrGeneratorEdModeToolkit : public FModeToolkit
{
public:

	FProcAndRandObjDistrGeneratorEdModeToolkit();
	
	/** FModeToolkit interface */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override { return ToolkitWidget; }

	void DistributionBitmapChanged(const FAssetData& InAssetData);
	FString GetDistributionBitmapPath() const;

	void SelectedProcGenSlotObjectChanged(const FAssetData& InAssetData);
	float GetPaintBrushSize() const;
	TOptional<int32> GetPaintBrushSize2() const;
	void SetPaintBrushSize(float NewSize);
	FText GetSelectedActorsNamesTPText() const;
	FString GetSelectedProcGenSlotObjectPath() const;

	FString SelectedActorsNamesToPaint;

private:

	TSharedPtr<SWidget> ToolkitWidget;
};
