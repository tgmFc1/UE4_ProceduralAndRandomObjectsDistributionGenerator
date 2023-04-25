// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProcAndRandObjDistrGeneratorEdModeToolkit.h"
#include "ProcAndRandObjDistrGeneratorEdMode.h"
#include "Engine/Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorModeManager.h"
#include "Runtime/Landscape/Classes/Landscape.h"
#include "Runtime/Landscape/Classes/LandscapeInfo.h"
#include "Runtime/Landscape/Classes/LandscapeComponent.h"
#include "Runtime/Landscape/Public/LandscapeEdit.h"
#include "PropertyCustomizationHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine.h"
//#include "EditorDialogLibrary.h"
//#include "UserInterface/PropertyEditor/SPropertyEditorAsset.h"

#include "ProcGen/ProcGenSlotObject.h"
#include "ProcGen/ProcGenManager.h"
#include "ProcGen/ProcGenActor.h"

//#include "UserInterface/PropertyEditor/SPropertyEditorArray.h"
//#include "UserInterface/PropertyEditor/SPropertyEditorArrayItem.h"

#include "Widgets/Input/SNumericEntryBox.h"

//#include "Widgets/"

#define LOCTEXT_NAMESPACE "FProcAndRandObjDistrGeneratorEdModeToolkit"

FProcAndRandObjDistrGeneratorEdModeToolkit::FProcAndRandObjDistrGeneratorEdModeToolkit()
{
}
static TArray<uint16> TerrainHData = TArray<uint16>();
static TArray<uint8> TerrainWData = TArray<uint8>();
static TArray<uint8> TerrainLCData = TArray<uint8>();
static TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
static TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool2;
static TSharedPtr<IPropertyHandle> SelectedBitmapProperty;
static FString SelectedBitmapAssetPath = "";
static UTexture2D* SelectedBitmapPtr = nullptr;

static FString SelectedProcGenSlotObjectAssetPath = "";
static UPGSObj* SelectedProcGenSlotObjectPtr = nullptr;

//static UPGSObj* CreatedProcGenSlotObjectPtr = nullptr;

static TWeakObjectPtr<UPGSObj> CreatedProcGenSlotObjectPtr = nullptr;

void FProcAndRandObjDistrGeneratorEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	SelectedActorsNamesToPaint = "";

	UPGSObj* DefUPGSObj = Cast<UPGSObj>(UPGSObj::StaticClass()->GetDefaultObject(true));
	if (DefUPGSObj)
	{
		//FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FProcAndRandObjDistrGeneratorEdModeToolkit::Init1", "DefUPGSObj created!"));
	}

	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(24));
	AssetThumbnailPool2 = MakeShareable(new FAssetThumbnailPool(24));
	//SelectedBitmapProperty
	struct Locals
	{
		static bool IsWidgetEnabled()
		{
			return true;//GEditor->GetSelectedActors()->Num() != 0;
		}

		/*static void DistributionBitmapChanged(const FAssetData& InAssetData)
		{

		}*/

		static FReply OnButtonClick(FVector InOffset)
		{
			UWorld* pCurEdWorld = GEditor->GetEditorWorldContext().World();

			USelection* SelectedActors = GEditor->GetSelectedActors();

			if (!CreatedProcGenSlotObjectPtr.IsValid() && SelectedProcGenSlotObjectPtr)
			{
				CreatedProcGenSlotObjectPtr = NewObject<UPGSObj>();
				//CreatedProcGenSlotObjectPtr = Cast<UPGSObj>(StaticConstructObject_Internal(SelectedProcGenSlotObjectPtr->GetClass()));
				if (CreatedProcGenSlotObjectPtr.IsValid())
				{
					CreatedProcGenSlotObjectPtr.Get()->AddToRoot();
					CreatedProcGenSlotObjectPtr.Get()->GenerationParamsArr = SelectedProcGenSlotObjectPtr->GenerationParamsArr;
					CreatedProcGenSlotObjectPtr.Get()->bDelayedGeneration = SelectedProcGenSlotObjectPtr->bDelayedGeneration;
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Generate button - CreatedProcGenSlotObject created!", "ProcGenSlotObject created!"));
				}
			}
			else if (CreatedProcGenSlotObjectPtr.IsValid() && SelectedProcGenSlotObjectPtr)
			{
				//TArray<AActor*> OldActorsArr = CreatedProcGenSlotObjectPtr.Get()->GenerationParamsArr;
				//CreatedProcGenSlotObjectPtr.Get()->GenerationParamsArr = SelectedProcGenSlotObjectPtr->GenerationParamsArr;
			}

			if (!CreatedProcGenSlotObjectPtr.IsValid())
			{
				return FReply::Handled();
			}

			if (!SelectedProcGenSlotObjectPtr)
			{
				return FReply::Handled();
			}

			// Let editor know that we're about to do something that we want to undo/redo
			GEditor->BeginTransaction(LOCTEXT("MoveActorsTransactionName", "MoveActors"));
			//GEditor->AddActor()
			ALandscape* pSelLandscape = nullptr;
			// For each selected actor
			for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
			{
				if (AActor * LevelActor = Cast<AActor>(*Iter))
				{
					ALandscape* Proxy = Cast<ALandscape>(LevelActor);
					if (Proxy != nullptr)
					{
						pSelLandscape = Proxy;
						break;
					}
				}
			}

			if (pSelLandscape && SelectedBitmapPtr)
			{
				ULandscapeInfo* landInf = pSelLandscape->GetLandscapeInfo();
				if (landInf != nullptr)
				{

					CreatedProcGenSlotObjectPtr.Get()->PrepareToGeneration();

					TArray<FVector> BoxPoints = TArray<FVector>();
					int32 MinX, MinY, MaxX, MaxY;
					FVector LandPos = pSelLandscape->GetActorLocation();
					if (landInf->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) && SelectedBitmapPtr && pSelLandscape->GetWorld())
					{
						TArray<FLinearColor> ColorArr = TArray<FLinearColor>();
						FTexture2DMipMap* BitmapMipMap = &SelectedBitmapPtr->PlatformData->Mips[0];
						FByteBulkData* RawImageData = &BitmapMipMap->BulkData;
						FColor* FormatedImageData = reinterpret_cast<FColor*>(RawImageData->Lock(LOCK_READ_ONLY));
						//uint32 TextureWidth = BitmapMipMap->SizeX, TextureHeight = BitmapMipMap->SizeY;
						uint32 TextureWidth = SelectedBitmapPtr->GetSizeX(), TextureHeight = SelectedBitmapPtr->GetSizeY();
						FColor PixelColor = FColor();
						FVector PixelCoords = FVector(0);
						float scaleCoof = float(MaxX > 0 ? MaxX : 1) / float(TextureWidth > 0 ? TextureWidth : 1);
						if (RawImageData->GetBulkDataSize() < (TextureHeight * TextureWidth))
						{
							//TODO
							RawImageData->Unlock();
							FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Generation inerrupted!", "Generation ERROR C2"));

							GEditor->EndTransaction();

							return FReply::Handled();
						}
						bool ProcGenSlotObjectIsRemoved = false;
						for (uint32 y = 0; y < TextureHeight; ++y)
						{
							for (uint32 x = 0; x < TextureWidth; ++x)
							{
								if (!CreatedProcGenSlotObjectPtr.IsValid())
								{
									ProcGenSlotObjectIsRemoved = true;
									continue;
								}
								PixelColor = FormatedImageData[(y * TextureWidth) + x];
								FLinearColor LColor = FLinearColor::FromSRGBColor(PixelColor);
								ColorArr.Add(LColor);
								//now disabled
								continue;

								PixelCoords.X = LandPos.X + (float(x * 100) * scaleCoof);
								PixelCoords.Y = LandPos.Y + (float(y * 100) * scaleCoof);
								PixelCoords.Z = 65535.0f;

								BoxPoints.Empty();

								float BoxSizeBB = 100.0f * scaleCoof;
								FVector ChkLoc = FVector(PixelCoords.X, PixelCoords.Y, 65535.0f);
								BoxPoints.Add(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f));
								BoxPoints.Add(ChkLoc + FVector(0.0f, BoxSizeBB, 0.0f));
								BoxPoints.Add(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f) + FVector(0.0f, BoxSizeBB, 0.0f));
								BoxPoints.Add(ChkLoc + FVector(0.0f, 0.0f, BoxSizeBB));

								CreatedProcGenSlotObjectPtr.Get()->RequestGenerateInBBox(BoxPoints, pCurEdWorld, LColor.R + LColor.G + LColor.B/* + LColor.A*/);
								//UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, PixelCoords, FVector(PixelCoords.X, PixelCoords.Y, -65535.0f), FLinearColor::Red * (LColor.R + LColor.G + LColor.B), 15.3f);
								//GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Cyan, FString::Printf(TEXT("Bitmap TextureWidth - %i, TextureHeight - %i"), TextureWidth, TextureHeight));
							}
						}
						//unlock after all
						RawImageData->Unlock();

						for (uint32 y = 0; y < TextureHeight; ++y)
						{
							for (uint32 x = 0; x < TextureWidth; ++x)
							{
								FLinearColor LColor = ColorArr[(y * TextureWidth) + x];

								PixelCoords.X = LandPos.X + (float(x * 100) * scaleCoof);
								PixelCoords.Y = LandPos.Y + (float(y * 100) * scaleCoof);
								PixelCoords.Z = 65535.0f;

								BoxPoints.Empty();

								float BoxSizeBB = 100.0f * scaleCoof;
								FVector ChkLoc = FVector(PixelCoords.X, PixelCoords.Y, 65535.0f);
								BoxPoints.Add(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f));
								BoxPoints.Add(ChkLoc + FVector(0.0f, BoxSizeBB, 0.0f));
								BoxPoints.Add(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f) + FVector(0.0f, BoxSizeBB, 0.0f));
								BoxPoints.Add(ChkLoc + FVector(0.0f, 0.0f, BoxSizeBB));

								CreatedProcGenSlotObjectPtr.Get()->RequestGenerateInBBox(BoxPoints, pCurEdWorld, LColor.R + LColor.G + LColor.B/* + LColor.A*/);
								UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, PixelCoords, FVector(PixelCoords.X, PixelCoords.Y, -65535.0f), FLinearColor::Red * (LColor.R + LColor.G + LColor.B), 15.3f);
							}
						}

						GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Cyan, FString::Printf(TEXT("Landscape MinX - %i, MinY - %i, MaxX - %i, MaxY - %i"), MinX, MinY, MaxX, MaxY));
						GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Cyan, FString::Printf(TEXT("Bitmap TextureWidth - %i, TextureHeight - %i"), TextureWidth, TextureHeight));
						if (!ProcGenSlotObjectIsRemoved)
						{
							FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("Message", "{0}"), FText::FromName(*FString::Printf(TEXT("Generation comlete! Bitmap TextureWidth - %i, TextureHeight - %i, BulkDataSize - %i"), TextureWidth, TextureHeight, RawImageData->GetBulkDataSize()))));
						}
						else
						{
							FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Generation inerrupted!", "Generation ERROR C1"));
						}
					}
				}
			}


			// We're done moving actors so close transaction
			GEditor->EndTransaction();

			return FReply::Handled();
		}

		static FReply OnButtonClick2(FVector InOffset)
		{
			if (CreatedProcGenSlotObjectPtr.IsValid())
			{
				CreatedProcGenSlotObjectPtr.Get()->PrepareToDestroy();
				CreatedProcGenSlotObjectPtr.Get()->RemoveFromRoot();
				if (CreatedProcGenSlotObjectPtr.Get()->IsDestructionThreadSafe())
				{
					CreatedProcGenSlotObjectPtr.Get()->ConditionalBeginDestroy();
				}
				CreatedProcGenSlotObjectPtr = nullptr;
			}
			return FReply::Handled();
		}

		static FReply OnButtonClickRemoveAllProcActors(FVector InOffset)
		{
			if (UProcGenManager::GetCurManager())
			{
				UProcGenManager::GetCurManager()->RemoveAllProcGeneratedActors();
			}
			return FReply::Handled();
		}

		static FReply OnButtonClick3(FVector InOffset)
		{
			if (CreatedProcGenSlotObjectPtr.IsValid())
			{
				CreatedProcGenSlotObjectPtr->FreezeCreatedObjectsInEditor();
			}
			return FReply::Handled();
		}

		static FReply OnButtonTest1(FVector InOffset)
		{
			UWorld* pCurEdWorld = GEditor->GetEditorWorldContext().World();

			USelection* SelectedActors = GEditor->GetSelectedActors();

			ALandscape* pSelLandscape = nullptr;

			for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
			{
				if (AActor * LevelActor = Cast<AActor>(*Iter))
				{
					ALandscape* Proxy = Cast<ALandscape>(LevelActor);
					if (Proxy != nullptr)
					{
						pSelLandscape = Proxy;
						break;
					}
				}
			}

			if (pSelLandscape)
			{
				ULandscapeInfo* landInf = pSelLandscape->GetLandscapeInfo();
				if (landInf != nullptr)
				{
					int32 MinX, MinY, MaxX, MaxY;
					FVector LandPos = pSelLandscape->GetActorLocation();
					if (landInf->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) && pSelLandscape->GetWorld())
					{
						TSet<ULandscapeComponent*> OutComponents = TSet<ULandscapeComponent*>();
						landInf->GetComponentsInRegion(MinX, MinY, MaxX, MaxY, OutComponents);
						
						//Test------------------------------
						FLandscapeEditDataInterface LandscapeEdit(landInf);
						const uint32 LandscapeResolution[2] = { (uint32)(1 + MaxX - MinX), (uint32)(1 + MaxY - MinY) };
						if (TerrainHData.Num() > 0)
						{
							TerrainHData.Empty();
						}
						if (TerrainWData.Num() > 0)
						{
							TerrainWData.Empty();
						}
						if (TerrainLCData.Num() > 0)
						{
							TerrainLCData.Empty();
						}

						TerrainHData.AddZeroed(LandscapeResolution[0] * LandscapeResolution[1]);
						TerrainWData.AddZeroed(LandscapeResolution[0] * LandscapeResolution[1]);
						TerrainLCData.AddZeroed(LandscapeResolution[0] * LandscapeResolution[1]);

						LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, TerrainHData.GetData(), 0);
						LandscapeEdit.GetWeightData(nullptr, MinX, MinY, MaxX, MaxY, TerrainWData.GetData(), 0);
						LandscapeEdit.GetLayerContributionData(MinX, MinY, MaxX, MaxY, TerrainLCData.GetData(), 0);
						//LandscapeEdit.

						if (TerrainHData.Num() >= int32(LandscapeResolution[0] * LandscapeResolution[1]))
						{
							for (uint32 y = 0; y < LandscapeResolution[0]; ++y)
							{
								for (uint32 x = 0; x < LandscapeResolution[1]; ++x)
								{
									float heightOffset = 32000.0f;
									FVector PixelCoords;
									FVector PixelCoords2;
									uint16 TerrainHeight = TerrainHData[(y * LandscapeResolution[1]) + x];
									PixelCoords.X = LandPos.X + (float(x * 100) * 1.0f);
									PixelCoords.Y = LandPos.Y + (float(y * 100) * 1.0f);
									PixelCoords.Z = LandPos.Z - heightOffset + TerrainHeight;
									PixelCoords2.X = LandPos.X + (float(x * 100) * 1.0f);
									PixelCoords2.Y = LandPos.Y + (float(y * 100) * 1.0f);
									PixelCoords2.Z = LandPos.Z - heightOffset + TerrainHeight + 1000.0f;
									//PixelCoords = pSelLandscape->GetTransform().TransformPosition(PixelCoords);
									//PixelCoords2 = pSelLandscape->GetTransform().TransformPosition(PixelCoords2);
									if ((FMath::RandRange(0, 50) < 1))
									{
										UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, PixelCoords, PixelCoords2, FLinearColor::Green, 15.3f);
									}

								}
							}
						}
						if (TerrainWData.Num() >= int32(LandscapeResolution[0] * LandscapeResolution[1]))
						{
							for (uint32 y = 0; y < LandscapeResolution[0]; ++y)
							{
								for (uint32 x = 0; x < LandscapeResolution[1]; ++x)
								{
									FVector PixelCoords;
									FVector PixelCoords2;
									uint8 TerrainWeight = TerrainWData[(y * LandscapeResolution[1]) + x];
									PixelCoords.X = LandPos.X + (float(x * 100) * 1.0f);
									PixelCoords.Y = LandPos.Y + (float(y * 100) * 1.0f);
									PixelCoords.Z = LandPos.Z + 0.0f;
									PixelCoords2.X = LandPos.X + (float(x * 100) * 1.0f);
									PixelCoords2.Y = LandPos.Y + (float(y * 100) * 1.0f);
									PixelCoords2.Z = LandPos.Z + -1000.0f + float(TerrainWeight * 100);
									//PixelCoords = pSelLandscape->GetTransform().TransformPosition(PixelCoords);
									//PixelCoords2 = pSelLandscape->GetTransform().TransformPosition(PixelCoords2);
									FLinearColor SelectedColour = TerrainWeight == 0 ? FLinearColor::Black : FLinearColor::Yellow;
									if ((FMath::RandRange(0, 50) < 1))
									{
										//UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, PixelCoords, PixelCoords2, SelectedColour, 15.3f);
										//UKismetSystemLibrary::DrawDebugString(pCurEdWorld, PixelCoords, FString::Printf(TEXT("TerrainWeight at this point - %i"), TerrainWeight), nullptr, FLinearColor::Yellow, 15.0f);
									}
								}
							}
						}
						if (TerrainLCData.Num() >= int32(LandscapeResolution[0] * LandscapeResolution[1]))
						{
							for (uint32 y = 0; y < LandscapeResolution[0]; ++y)
							{
								for (uint32 x = 0; x < LandscapeResolution[1]; ++x)
								{
									FVector PixelCoords;
									FVector PixelCoords2;
									uint8 TerrainWeight = TerrainLCData[(y * LandscapeResolution[1]) + x];
									PixelCoords.X = LandPos.X + (float(x * 100) * 1.0f);
									PixelCoords.Y = LandPos.Y + (float(y * 100) * 1.0f);
									PixelCoords.Z = LandPos.Z + 0.0f;
									PixelCoords2.X = LandPos.X + (float(x * 100) * 1.0f);
									PixelCoords2.Y = LandPos.Y + (float(y * 100) * 1.0f);
									PixelCoords2.Z = LandPos.Z + -1000.0f + float(TerrainWeight * 100);
									//PixelCoords = pSelLandscape->GetTransform().TransformPosition(PixelCoords);
									//PixelCoords2 = pSelLandscape->GetTransform().TransformPosition(PixelCoords2);
									FLinearColor SelectedColour = TerrainWeight == 0 ? FLinearColor::Black : FLinearColor::Yellow;
									if ((FMath::RandRange(0, 50) < 1))
									{
										for (ULandscapeComponent* LandComp : OutComponents)
										{
											PixelCoords.Z = LandComp->Bounds.GetBox().GetCenter().Z;
											if (LandComp->Bounds.GetBox().IsInsideOrOn(PixelCoords))
											{
												//ULandscapeLayerInfoObject* PaintLayer = landInf ? landInf->GetLayerInfoByName(FName(TEXT("Grass"))) : nullptr;
												//float Wgt = LandComp->GetLayerWeightAtLocation(PixelCoords, PaintLayer, nullptr, true);
												//UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, PixelCoords, PixelCoords2, FLinearColor::Yellow * Wgt, 15.3f);
											}
										}
										//UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, PixelCoords, PixelCoords2, SelectedColour, 15.3f);
										//UKismetSystemLibrary::DrawDebugString(pCurEdWorld, PixelCoords, FString::Printf(TEXT("TerrainWeight at this point - %i"), TerrainWeight), nullptr, FLinearColor::Yellow, 15.0f);
									}
								}
							}
						}
						GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Cyan, FString::Printf(TEXT("Terrain H - %i, Terrain W - %i, Terrain LC - %i, Terrain SZ - %i"), TerrainHData.Num(), TerrainWData.Num(), TerrainLCData.Num(), int32(LandscapeResolution[0] * LandscapeResolution[1])));
						//----------------------------------
					}
				}
			}
			return FReply::Handled();
		}

		static TSharedRef<SWidget> MakeButton(FText InLabel, const FVector InOffset)
		{
			return SNew(SButton)
				.Text(InLabel)
				.OnClicked_Static(&Locals::OnButtonClick, InOffset);
		}

		static TSharedRef<SWidget> MakeButtonClear(FText InLabel, const FVector InOffset)
		{
			return SNew(SButton)
				.Text(InLabel)
				.OnClicked_Static(&Locals::OnButtonClick2, InOffset);
		}

		static TSharedRef<SWidget> MakeButtonApply(FText InLabel, const FVector InOffset)
		{
			return SNew(SButton)
				.Text(InLabel)
				.OnClicked_Static(&Locals::OnButtonClick3, InOffset);
		}

		static TSharedRef<SWidget> MakeButtonTest1(FText InLabel, const FVector InOffset)
		{
			return SNew(SButton)
				.Text(InLabel)
				.OnClicked_Static(&Locals::OnButtonTest1, InOffset);
		}

		static TSharedRef<SWidget> MakeButtonRemoveAllActors(FText InLabel, const FVector InOffset)
		{
			return SNew(SButton)
				.Text(InLabel)
				.OnClicked_Static(&Locals::OnButtonClickRemoveAllProcActors, InOffset);
		}
	};

	const float Factor = 256.0f;

	SAssignNew(ToolkitWidget, SBorder)
		.HAlign(HAlign_Center)
		.Padding(25)
		.IsEnabled_Static(&Locals::IsWidgetEnabled)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(50)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
		.Text(LOCTEXT("HelperLabel", "Select Landscape"))
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			Locals::MakeButton(LOCTEXT("UpButtonLabel", "Generate selected Actors on landscape with mask"), FVector(0, 0, Factor))
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			Locals::MakeButtonApply(LOCTEXT("LeftButtonLabel", "ApplyGenerationChanges"), FVector(0, -Factor, 0))
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			Locals::MakeButtonTest1(LOCTEXT("RightButtonLabel", "Test1"), FVector(0, Factor, 0))
		]
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			Locals::MakeButtonClear(LOCTEXT("DownButtonLabel", "ClearCurrentGeneration"), FVector(0, 0, -Factor))
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(10)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
		.Text(LOCTEXT("HelperLabel2", "Bitmap for generation"))
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			Locals::MakeButtonRemoveAllActors(LOCTEXT("RemoveActorsButtonLabel", "RemoveAllProcCreatedActors"), FVector(0, 0, -Factor))
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SObjectPropertyEntryBox)
			.ThumbnailPool(AssetThumbnailPool)
		.AllowedClass(UTexture2D::StaticClass())
		//.PropertyHandle(SelectedBitmapProperty)
		.DisplayBrowse(true)
		.DisplayThumbnail(true)
		.EnableContentPicker(true)
		.OnObjectChanged(this, &FProcAndRandObjDistrGeneratorEdModeToolkit::DistributionBitmapChanged)
		.ObjectPath(this, &FProcAndRandObjDistrGeneratorEdModeToolkit::GetDistributionBitmapPath)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(10)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
		.Text(LOCTEXT("HelperLabel3", "ProcGenSlotObject for generation"))
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SObjectPropertyEntryBox)
			.ThumbnailPool(AssetThumbnailPool2)
		.AllowedClass(UObject::StaticClass())
		//.AllowedClass(UPGSObj::StaticClass())
		//.PropertyHandle(SelectedBitmapProperty)
		.DisplayBrowse(true)
		.DisplayThumbnail(true)
		.EnableContentPicker(true)
		.OnObjectChanged(this, &FProcAndRandObjDistrGeneratorEdModeToolkit::SelectedProcGenSlotObjectChanged)
		.ObjectPath(this, &FProcAndRandObjDistrGeneratorEdModeToolkit::GetSelectedProcGenSlotObjectPath)
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()
			{
					if (UProcGenManager::GetCurManager())
					{
						return UProcGenManager::GetCurManager()->bPaintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
					if (UProcGenManager::GetCurManager())
					{
						UProcGenManager::GetCurManager()->bPaintMode = !UProcGenManager::GetCurManager()->bPaintMode;
						InCheckBoxState = UProcGenManager::GetCurManager()->bPaintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						SelectedActorsNamesToPaint = "Procgen Actors selected to paint: ";
						UProcGenManager::GetCurManager()->SelectedToPaintProcGenActorsPtrs.Empty();
						if (GEditor && UProcGenManager::GetCurManager()->bPaintMode)
						{
							UProcGenManager::GetCurManager()->bPaintMode = false;
							USelection* SelectedActors = GEditor->GetSelectedActors();
							for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
							{
								if (AActor* LevelActor = Cast<AActor>(*Iter))
								{
									if (UProcGenManager::GetCurManager()->IsActorAreGenerator(LevelActor))
									{
										UProcGenManager::GetCurManager()->SelectedToPaintProcGenActorsPtrs.AddUnique(Cast<AProcGenActor>(LevelActor));
										SelectedActorsNamesToPaint.Append(LevelActor->GetName());
										SelectedActorsNamesToPaint.Append(", ");
									}
								}
							}
							GEditor->SelectNone(false, false, false);
							GEditor->SelectActor(nullptr, true, false);
							GEditor->SelectNone(false, true);
							UProcGenManager::GetCurManager()->bPaintMode = true;
						}
						else
						{
							SelectedActorsNamesToPaint = "PaintMode off";
						}
					}
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Procedural Paint"))
				.HighlightText(FText::FromString("Enable Paint mode"))
			]
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(SNumericEntryBox<int32>)
			.MinValue(50)
			.MaxValue(50000)
			.MinSliderValue(50)
			.MaxSliderValue(50000)
			.AllowSpin(true)
			.Value(this, &FProcAndRandObjDistrGeneratorEdModeToolkit::GetPaintBrushSize2)
			.OnValueChanged_Lambda([=](int32 NewValue)
			{
					SetPaintBrushSize(float(NewValue));
			})
			.OnValueCommitted_Lambda([=](int32 NewValue, ETextCommit::Type)
			{
					SetPaintBrushSize(float(NewValue));
			})
			.OnBeginSliderMovement_Lambda([=]()
					{
						
					})
			.OnEndSliderMovement_Lambda([=](double)
					{
						
					})
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(this, &FProcAndRandObjDistrGeneratorEdModeToolkit::GetSelectedActorsNamesTPText)
		]
	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()
				{
					if (UProcGenManager::GetCurManager())
					{
						return UProcGenManager::GetCurManager()->bUseCameraDirToGenerationInPaintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
				})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
				{
					if (UProcGenManager::GetCurManager())
					{
						UProcGenManager::GetCurManager()->bUseCameraDirToGenerationInPaintMode = !UProcGenManager::GetCurManager()->bUseCameraDirToGenerationInPaintMode;
						InCheckBoxState = UProcGenManager::GetCurManager()->bUseCameraDirToGenerationInPaintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
				})
			.Content()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(FText::FromString("Use Camera Dir To Generation In Paint Mode"))
					.HighlightText(FText::FromString("Paint mode option"))
				]
		]

	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()
				{
					if (UProcGenManager::GetCurManager())
					{
						return UProcGenManager::GetCurManager()->bUseCameraDirToRotationAlignGenObjectsInPaintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
				})
		.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				if (UProcGenManager::GetCurManager())
				{
					UProcGenManager::GetCurManager()->bUseCameraDirToRotationAlignGenObjectsInPaintMode = !UProcGenManager::GetCurManager()->bUseCameraDirToGenerationInPaintMode;
					InCheckBoxState = UProcGenManager::GetCurManager()->bUseCameraDirToRotationAlignGenObjectsInPaintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}
			})
					.Content()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
				.Text(FText::FromString("Use Camera Dir To Rotation Align GenObjects In Paint Mode"))
				.HighlightText(FText::FromString("Paint mode option"))
				]
		]

	+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()
				{
					if (UProcGenManager::GetCurManager())
					{
						return UProcGenManager::GetCurManager()->bUseCollisionChecksInPaintRemoveMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
				})
		.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				if (UProcGenManager::GetCurManager())
				{
					UProcGenManager::GetCurManager()->bUseCollisionChecksInPaintRemoveMode = !UProcGenManager::GetCurManager()->bUseCollisionChecksInPaintRemoveMode;
					InCheckBoxState = UProcGenManager::GetCurManager()->bUseCollisionChecksInPaintRemoveMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}
			})
					.Content()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
				.Text(FText::FromString("Use Collision Checks In Paint Removing Mode"))
				.HighlightText(FText::FromString("Paint mode option"))
				]
		]

		];
		
	FModeToolkit::Init(InitToolkitHost);
}

FName FProcAndRandObjDistrGeneratorEdModeToolkit::GetToolkitFName() const
{
	return FName("ProcAndRandObjDistrGeneratorEdMode");
}

FText FProcAndRandObjDistrGeneratorEdModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("ProcAndRandObjDistrGeneratorEdModeToolkit", "DisplayName", "ProcAndRandObjDistrGeneratorEdMode Tool");
}

class FEdMode* FProcAndRandObjDistrGeneratorEdModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FProcAndRandObjDistrGeneratorEdMode::EM_ProcAndRandObjDistrGeneratorEdModeId);
}

void FProcAndRandObjDistrGeneratorEdModeToolkit::DistributionBitmapChanged(const FAssetData& InAssetData)
{
	UTexture2D* SelectedTextureAsset = Cast<UTexture2D>(InAssetData.GetAsset());
	if (SelectedTextureAsset)
	{
		SelectedBitmapPtr = SelectedTextureAsset;
		SelectedBitmapAssetPath = InAssetData.ObjectPath.ToString();
		//FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FProcAndRandObjDistrGeneratorEdModeToolkit::DistributionBitmapChanged", "SelectedTextureAsset is now selected!"));
	}
	else
	{
		SelectedBitmapAssetPath = "";
		SelectedBitmapPtr = nullptr;
	}
}

FString FProcAndRandObjDistrGeneratorEdModeToolkit::GetDistributionBitmapPath() const
{
	return SelectedBitmapAssetPath;
}

void FProcAndRandObjDistrGeneratorEdModeToolkit::SelectedProcGenSlotObjectChanged(const FAssetData& InAssetData)
{
	UPGSObj* SelectedProcGenSlotObjectAsset = nullptr;
	GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Cyan, FString::Printf(TEXT("FProcAndRandObjDistrGeneratorEdModeToolkit::SelectedProcGenSlotObjectChanged AssetPath - %s"), *InAssetData.ObjectPath.ToString()));
	SelectedProcGenSlotObjectAssetPath = InAssetData.ObjectPath.ToString();
	if (!SelectedProcGenSlotObjectAssetPath.IsEmpty() && SelectedProcGenSlotObjectAssetPath != FString("None"))
	{
		FString ObjClassStr = SelectedProcGenSlotObjectAssetPath + FString("_C");
		GEngine->AddOnScreenDebugMessage((uint64)-1, 20.0f, FColor::Magenta, FString::Printf(TEXT("ObjClassStr find request - %s"),
			*ObjClassStr));

		UClass* pRequestedClass = FindObject<UClass>(ANY_PACKAGE, *ObjClassStr, false);
		if (!pRequestedClass)
		{
			pRequestedClass = LoadClass<UObject>(nullptr, *ObjClassStr);
			//log----------------------------
			GEngine->AddOnScreenDebugMessage((uint64)-1, 20.0f, FColor::Red, FString::Printf(TEXT("CUsefulTools::GetClassPointerByName LoadClass request - %s, bad behavior, class must be loaded before"),
				*ObjClassStr));
			//-------------------------------
		}

		if (pRequestedClass)
		{
			UPGSObj* DefUPGSObj = Cast<UPGSObj>(pRequestedClass->GetDefaultObject(true));
			if (DefUPGSObj)
			{
				SelectedProcGenSlotObjectAsset = DefUPGSObj;
				if (SelectedProcGenSlotObjectAsset)
				{
					SelectedProcGenSlotObjectPtr = SelectedProcGenSlotObjectAsset;
					SelectedProcGenSlotObjectAssetPath = InAssetData.ObjectPath.ToString();
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage((uint64)-1, 20.0f, FColor::Red, FString::Printf(TEXT("!DefUPGSObj - %s"),
					*SelectedProcGenSlotObjectAssetPath));

				GEngine->AddOnScreenDebugMessage((uint64)-1, 20.0f, FColor::Red, FString::Printf(TEXT("pRequestedClass name - %s"),
					*pRequestedClass->GetFullName()));

				UObject* DefObj = Cast<UObject>(pRequestedClass->GetDefaultObject(true));
				if (DefObj)
				{
					GEngine->AddOnScreenDebugMessage((uint64)-1, 20.0f, FColor::Red, FString::Printf(TEXT("but DefObj is exist, name - %s"),
						*DefObj->GetFullName()));
				}
			}		
		}
		else
		{
			GEngine->AddOnScreenDebugMessage((uint64)-1, 20.0f, FColor::Red, FString::Printf(TEXT("!pRequestedClass - %s"),
				*SelectedProcGenSlotObjectAssetPath));
		}
	}

	if (!SelectedProcGenSlotObjectAsset)
	{
		SelectedProcGenSlotObjectAssetPath = "";
		SelectedProcGenSlotObjectPtr = nullptr;

		if (!InAssetData.ObjectPath.ToString().IsEmpty())
		{
			SelectedProcGenSlotObjectAssetPath = InAssetData.ObjectPath.ToString();
		}
	}
	GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Cyan, FString::Printf(TEXT("FProcAndRandObjDistrGeneratorEdModeToolkit::SelectedProcGenSlotObjectChanged SelectedProcGenSlotObjectAsset ptr avaliable - %s"), SelectedProcGenSlotObjectAsset ? *FString("true") : *FString("false")));
}

float FProcAndRandObjDistrGeneratorEdModeToolkit::GetPaintBrushSize() const
{
	UProcGenManager* pProcMan = UProcGenManager::GetCurManager();
	if (pProcMan)
	{
		return pProcMan->PaintSphereSize;
	}
	return 1000.0f;
}

TOptional<int32> FProcAndRandObjDistrGeneratorEdModeToolkit::GetPaintBrushSize2() const
{
	UProcGenManager* pProcMan = UProcGenManager::GetCurManager();
	if (pProcMan)
	{
		return int32(pProcMan->PaintSphereSize);
	}
	return 1000;
}

void FProcAndRandObjDistrGeneratorEdModeToolkit::SetPaintBrushSize(float NewSize)
{
	UProcGenManager* pProcMan = UProcGenManager::GetCurManager();
	if (pProcMan)
	{
		pProcMan->PaintSphereSize = NewSize;
	}
}

FText FProcAndRandObjDistrGeneratorEdModeToolkit::GetSelectedActorsNamesTPText() const
{
	return FText::FromString(SelectedActorsNamesToPaint);
}

FString FProcAndRandObjDistrGeneratorEdModeToolkit::GetSelectedProcGenSlotObjectPath() const
{
	return SelectedProcGenSlotObjectAssetPath;
}

#undef LOCTEXT_NAMESPACE
