// Free to copy, edit and use in any work/projects, commercial or free

#include "ProcGen/ProcGenSlotObject.h"
#include "..\..\Public\ProcGen\ProcGenSlotObject.h"
#include "Engine.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "Engine/StaticMeshActor.h"

#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"

#include "ProcAndRandObjDistrGenerator.h"
#include "Engine/Polys.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
//#include "..\..\Public\ProcGen\ProcGenSlotObject.h"
#include "..\..\Public\ProcGen\ProcGenManager.h"
#include "ProcGen/ProcGenActor.h"
#include "ProcGen/ProcGenParamsModifierActor.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopedSlowTask.h"

UPGSObj::UPGSObj() : Super(), GenerationParamsArr()
{
	bDelayedGeneration = false;
	bPlaceActorsInEditor = false;
	bIsPreparedToGeneration = false;
	bEventGenOnly = false;
}

void UPGSObj::BeginDestroy()
{
	PrepareToDestroy();

	Super::BeginDestroy();
}

UWorld* UPGSObj::GetWorld() const
{
	if (GetOuter() && !HasAnyFlags(RF_ClassDefaultObject) && !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		AActor* Outer = GetTypedOuter<AActor>();
		if (Outer)
		{
			return Outer->GetWorld();
		}
	}

	return nullptr;
}

void UPGSObj::PrepareToGeneration()
{
	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		ProcGenSlot.CurrentGenerationStream = UKismetMathLibrary::MakeRandomStream(ProcGenSlot.RandomSeed);
		ProcGenSlot.TempTransformsForObjects.Empty();
	}

	bIsPreparedToGeneration = true;
}

void UPGSObj::RequestGenerateInBBox(const TArray<FVector>& GenerationBBoxPoints, /*const */UWorld* pGenerationWorld, float GenerationPower, AActor* OptProcGenActor)
{
	if (GenerationPower <= 0.0f)
		return;

	if (GenerationBBoxPoints.Num() < 2)
		return;

	if(!pGenerationWorld)
		return;

	FBox GenBox = FBox(GenerationBBoxPoints);

	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (!ProcGenSlot.bIsActive)
			continue;

		if (ProcGenSlot.ActorsTypesToGenerate.Num() <= 0)
		{
			if(ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() <= 0)
				continue;
		}

		ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ProcGenSlot.GenerationTraceChannel);

		float GenPowerSelected = ProcGenSlot.RandomGenerationPowerMinMax.GetRandomValue(&ProcGenSlot);
		float GenCoofSelected = ProcGenSlot.RandomGenerationCoofMinMax.GetRandomValue(&ProcGenSlot);
		GenPowerSelected *= GenerationPower;
		GenCoofSelected *= GenerationPower;
		uint32 NumGenChecks = uint32(GenPowerSelected);
		bool bGenerate = false;
		FVector TempPoint;
		FTransform TempTransform = FTransform::Identity;
		for (uint32 i = 0; i < NumGenChecks; ++i)
		{
			if (IsPendingKillOrUnreachable())
				return;

			bGenerate = RandomBoolByGenSlotParams(ProcGenSlot, GenCoofSelected);
			if (bGenerate)
			{
				TempPoint = RandomPointInBoxByGenSlotParams(ProcGenSlot, GenBox);
				FCollisionQueryParams TraceParams(FName(TEXT("CheckTrace")), true);
				TraceParams.bReturnPhysicalMaterial = true;
				FHitResult CheckHit = FHitResult(ForceInit);
				bool bIsHit = false;
				if (!ProcGenSlot.FloatingInAirGeneration)
				{
					bIsHit = pGenerationWorld->LineTraceSingleByChannel(CheckHit, TempPoint, FVector(TempPoint.X, TempPoint.Y, TempPoint.Z + -6553500.0f), CollisionChannel, TraceParams);
					if (!bIsHit && ProcGenSlot.UnderTerrainFixGeneration)
					{
						pGenerationWorld->LineTraceSingleByChannel(CheckHit, TempPoint, FVector(TempPoint.X, TempPoint.Y, TempPoint.Z + 6553500.0f), CollisionChannel, TraceParams);
						if (bIsHit)
						{
							//invert hit normal(backhit)
							CheckHit.Normal = -CheckHit.Normal;
						}
					}
				}

				int32 SelectedActorClassId = 0;
				if (ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() <= 0)
				{
					SelectedActorClassId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, ProcGenSlot.ActorsTypesToGenerate.Num() - 1, ProcGenSlot.CurrentGenerationStream);
					if (SelectedActorClassId < 0 || SelectedActorClassId >= ProcGenSlot.ActorsTypesToGenerate.Num())
					{
						SelectedActorClassId = 0;
					}
				}
				else
				{
					SelectedActorClassId = -1;
					int32 icf = 0;
					uint32 WhileCounterRt = 0;
					while (SelectedActorClassId == -1)
					{
						for (FActorTypeWithChanceTG& ActorTypeWC : ProcGenSlot.ActorsTypesWithChanceToGenerate)
						{
							if (RandomBoolByGenSlotParams(ProcGenSlot, ActorTypeWC.GenerationChance))
							{
								SelectedActorClassId = icf;
								break;
							}
							++icf;
						}
						icf = 0;
						++WhileCounterRt;
						if (WhileCounterRt >= 1000)
						{
							//safety
							break;
						}
					}

					if (SelectedActorClassId == -1)
					{
						SelectedActorClassId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() - 1, ProcGenSlot.CurrentGenerationStream);
					}
				}

				UClass* pSelectedActorClass = nullptr;
				if (ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() <= 0)
				{
					if (ProcGenSlot.ActorsTypesToGenerate.Num() > SelectedActorClassId)
						pSelectedActorClass = ProcGenSlot.ActorsTypesToGenerate[SelectedActorClassId];
				}
				else
				{
					if (SelectedActorClassId >= 0 && SelectedActorClassId < ProcGenSlot.ActorsTypesWithChanceToGenerate.Num())
					{
						pSelectedActorClass = ProcGenSlot.ActorsTypesWithChanceToGenerate[SelectedActorClassId].ActorClass;
					}
				}

				if (pSelectedActorClass == nullptr)
				{
					continue;
				}

				if (!ProcGenSlot.DistanceBetweenMinMax.IsIgnored())
				{
					float NewMinDistanceCalc = ProcGenSlot.DistanceBetweenMinMax.GetRandomValue(&ProcGenSlot);
					if (!bDelayedGeneration)
					{
						if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
						{
							bool bNeedContinue = false;
							for (AActor* pActor : ProcGenSlot.GeneratedActorsPtrs)
							{
								if (FVector::Dist(pActor->GetActorLocation(), bIsHit ? CheckHit.Location : TempPoint) < NewMinDistanceCalc)
								{
									bNeedContinue = true;
									break;
								}
							}

							if (bNeedContinue)
							{
								continue;
							}
						}

						if (ProcGenSlot.TakenIntoAccSlotsIds.Num() > 0)
						{
							bool bNeedContinue = false;
							TArray<FProcGenSlotParams*> TakenSlots = GetProcGenSlotsByUIds(ProcGenSlot.TakenIntoAccSlotsIds);
							if (TakenSlots.Num() > 0)
							{
								for (FProcGenSlotParams* pSlotParams : TakenSlots)
								{
									if (pSlotParams->GeneratedActorsPtrs.Num() > 0)
									{
										for (AActor* pActor : pSlotParams->GeneratedActorsPtrs)
										{
											if (FVector::Dist(pActor->GetActorLocation(), bIsHit ? CheckHit.Location : TempPoint) < pSlotParams->DefCollisionSize)
											{
												bNeedContinue = true;
												break;
											}
										}
									}

									if (bNeedContinue)
									{
										break;
									}
								}

								if (bNeedContinue)
								{
									continue;
								}
							}
						}
					}
					else
					{
						if (ProcGenSlot.TempTransformsForObjects.Num() > 0)
						{
							bool bNeedContinue = false;
							for (FTransform OldTrasf : ProcGenSlot.TempTransformsForObjects)
							{
								if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < NewMinDistanceCalc)
								{
									bNeedContinue = true;
									break;
								}
							}

							if (bNeedContinue)
							{
								continue;
							}
						}

						if (ProcGenSlot.TakenIntoAccSlotsIds.Num() > 0)
						{
							bool bNeedContinue = false;
							TArray<FProcGenSlotParams*> TakenSlots = GetProcGenSlotsByUIds(ProcGenSlot.TakenIntoAccSlotsIds);
							if (TakenSlots.Num() > 0)
							{
								for (FProcGenSlotParams* pSlotParams : TakenSlots)
								{
									if (pSlotParams->TempTransformsForObjects.Num() > 0)
									{
										for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
										{
											if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < pSlotParams->DefCollisionSize)
											{
												bNeedContinue = true;
												break;
											}
										}
									}

									if (bNeedContinue)
									{
										break;
									}
								}

								if (bNeedContinue)
								{
									continue;
								}
							}
						}
					}
				}

				if (!ProcGenSlot.HeightMinMax.IsIgnored())
				{
					float SelectedZ = bIsHit ? CheckHit.Location.Z : TempPoint.Z;
					if (SelectedZ > ProcGenSlot.HeightMinMax.MaxValue || SelectedZ < ProcGenSlot.HeightMinMax.MinValue)
					{
						continue;
					}
				}

				FRotator InitialRotation = FRotator::ZeroRotator;

				if (bIsHit)
				{
					if (CheckHit.GetActor())
					{
						bool bIsLandscape = CheckHit.GetActor()->IsA(ALandscape::StaticClass());
						bool bIsStaticMesh = CheckHit.GetActor()->IsA(AStaticMeshActor::StaticClass());

						if (!ProcGenSlot.bGenerateOnLandscape && bIsLandscape)
						{
							continue;
						}

						if (!ProcGenSlot.bGenerateOnStaticMeshes && bIsStaticMesh)
						{
							continue;
						}
					}

					float GroundAngleValue = FMath::RadiansToDegrees(FMath::Acos(CheckHit.Normal.Z));
					if (!ProcGenSlot.SlopeMinMax.IsIgnored())
					{
						if (GroundAngleValue > ProcGenSlot.SlopeMinMax.MaxValue || GroundAngleValue < ProcGenSlot.SlopeMinMax.MinValue)
						{
							continue;
						}
					}

					if (ProcGenSlot.AddZPosFromGroundNormal != 0.0f)
					{
						CheckHit.Location += (CheckHit.Normal * ProcGenSlot.AddZPosFromGroundNormal);
					}

					if (!ProcGenSlot.AddZPosFGNMinMax.IsIgnored())
					{
						CheckHit.Location += (CheckHit.Normal * ProcGenSlot.AddZPosFGNMinMax.GetRandomValue(&ProcGenSlot));
					}

					if (!ProcGenSlot.PressInGroundMinMax.IsIgnored())
					{
						CheckHit.Location.Z -= ProcGenSlot.PressInGroundMinMax.GetRandomValue(&ProcGenSlot);
					}

					if (ProcGenSlot.bPressInGroundByNormalZ)
					{
						float SlopePercent = GroundAngleValue / 90.0f;
						if (!ProcGenSlot.PressInGroundNZMinMax.IsIgnored())
						{
							CheckHit.Location.Z -= (ProcGenSlot.PressInGroundNZMinMax.GetRandomValue(&ProcGenSlot) * SlopePercent);
						}
					}

					TempTransform.SetLocation(CheckHit.Location + ProcGenSlot.AdditionalLocationVector);

					if (ProcGenSlot.RotateToGroundNormal)
					{
						FRotator AlignRotation = CheckHit.Normal.Rotation();
						// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
						AlignRotation.Pitch -= 90.f;
						// Clamp its value inside +/- one rotation
						AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
						//TempTransform.SetRotation(AlignRotation);
						InitialRotation = AlignRotation;
					}
				}
				else
				{
					if (ProcGenSlot.FloatingInAirGeneration)
					{
						TempTransform.SetLocation(TempPoint + ProcGenSlot.AdditionalLocationVector);
					}
					else
					{
						continue;
					}
				}

				FVector SelectedScale = FVector(1);
				if (!ProcGenSlot.ScaleMinMax.IsIgnored())
				{
					float SelectedSize = ProcGenSlot.ScaleMinMax.GetRandomValue(&ProcGenSlot);
					SelectedScale *= SelectedSize;
				}

				if (!ProcGenSlot.RandomNUScaleXMinMax.IsIgnored())
				{
					SelectedScale.X += ProcGenSlot.RandomNUScaleXMinMax.GetRandomValue(&ProcGenSlot);
				}

				if (!ProcGenSlot.RandomNUScaleYMinMax.IsIgnored())
				{
					SelectedScale.Y += ProcGenSlot.RandomNUScaleYMinMax.GetRandomValue(&ProcGenSlot);
				}

				if (!ProcGenSlot.RandomNUScaleZMinMax.IsIgnored())
				{
					SelectedScale.Z += ProcGenSlot.RandomNUScaleZMinMax.GetRandomValue(&ProcGenSlot);
				}

				FRotator AdditionalRotation = FRotator::ZeroRotator;
				if (!ProcGenSlot.RandomRotationRollMinMax.IsIgnored())
				{
					AdditionalRotation.Roll = ProcGenSlot.RandomRotationRollMinMax.GetRandomValue(&ProcGenSlot);
				}
				if (!ProcGenSlot.RandomRotationPitchMinMax.IsIgnored())
				{
					AdditionalRotation.Pitch = ProcGenSlot.RandomRotationPitchMinMax.GetRandomValue(&ProcGenSlot);
				}
				if (!ProcGenSlot.RandomRotationYawMinMax.IsIgnored())
				{
					AdditionalRotation.Yaw = ProcGenSlot.RandomRotationYawMinMax.GetRandomValue(&ProcGenSlot);
				}
				
				TempTransform.SetRotation((FQuat(InitialRotation) * FQuat(AdditionalRotation)) * FQuat(ProcGenSlot.AdditionalRotation));
				TempTransform.SetScale3D(SelectedScale);
				//temp check
				//continue;
				ProcGenSlot.TempTransformsForObjects.Add(TempTransform);
				if (bDelayedGeneration)
				{
					UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
					if (pProcGenManager)
					{
						FActorToDelayedCreateParams ActorToDelayedCreateParams = FActorToDelayedCreateParams();
						ActorToDelayedCreateParams.ActorClassPtr = pSelectedActorClass;
						ActorToDelayedCreateParams.ActorTransform = TempTransform;
						ActorToDelayedCreateParams.bSpawnInEditor = bPlaceActorsInEditor;
						ActorToDelayedCreateParams.ParentProcGenSlotObjPtr = this;
						ActorToDelayedCreateParams.ParentProcGenSlotParamsPtr = &ProcGenSlot;
						if (OptProcGenActor)
						{
							ActorToDelayedCreateParams.LinkedProcGenActorPtr = Cast<AProcGenActor>(OptProcGenActor);
						}

						pProcGenManager->DelayedCreateActorsQueue.Enqueue(ActorToDelayedCreateParams);
					}
					continue;
				}

				if (!bPlaceActorsInEditor)
				{
					FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
					SpawnInfo.OverrideLevel = pGenerationWorld->GetCurrentLevel();
					SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnInfo.ObjectFlags = RF_Transient;
					//SpawnInfo.bCreateActorPackage = true;
					//SpawnInfo.ObjectFlags = InObjectFlags;

					AActor* pGeneratedActor = pGenerationWorld->SpawnActor(pSelectedActorClass, &TempTransform, SpawnInfo);
					if (pGeneratedActor)
					{
						//pGeneratedActor->SetActorScale3D(SelectedScale);
						ProcGenSlot.GeneratedActorsPtrs.Add(pGeneratedActor);
					}
				}
#if WITH_EDITOR
				else if(GEditor)
				{
					AActor* pGeneratedActor = GEditor->AddActor(pGenerationWorld->GetCurrentLevel(), pSelectedActorClass, TempTransform, true);
					if (pGeneratedActor)
					{
						pGeneratedActor->SetActorScale3D(SelectedScale);

						ProcGenSlot.GeneratedActorsPtrs.Add(pGeneratedActor);

						GEditor->SelectActor(pGeneratedActor, 1, 0);
						pGeneratedActor->InvalidateLightingCache();
						pGeneratedActor->PostEditMove(true);
						pGeneratedActor->MarkPackageDirty();
						//UKismetSystemLibrary::DrawDebugLine(pGenerationWorld, TempTransform.GetTranslation(), FVector(TempTransform.GetTranslation().X, TempTransform.GetTranslation().Y, 65535.0f), FLinearColor::Red, 15.3f);
					}
				}
#endif
			}
		}
	}
}

static AActor* pCurOptProcGenActor = nullptr;

struct GenerationHelper
{
	static void ResizeFByScale3D(const FVector& Scale3DVec, float& SizeToScale)
	{
		float ScaleCoof = (Scale3DVec.X + Scale3DVec.Y + Scale3DVec.Z) / 3.0f;
		SizeToScale *= ScaleCoof;
	}

	static bool CheckCollision(UWorld * pGenWorld, FTransform & ObjectTransf, float ObjectSize, float ObjectHeight, bool bOnlyForGenerated = false, bool bOnlyForOthersGenerated = false)
	{
		if (!pGenWorld)
			return false;

		if (ObjectSize <= 0.0f && ObjectHeight <= 0.0f)
			return false;

		if (ObjectSize > 0.0f)
		{
			ResizeFByScale3D(ObjectTransf.GetScale3D(), ObjectSize);

			ECollisionChannel CollisionChannel = ECC_Visibility;

			//FHitResult CheckHit = FHitResult(ForceInit);
			bool bIsHit = false;

			FCollisionShape sphereColl = FCollisionShape::MakeSphere(ObjectSize);

			FCollisionQueryParams FCQP = FCollisionQueryParams(FName(TEXT("Paint_Trace")), true);
			FCQP.bTraceComplex = true;
			FCQP.bReturnPhysicalMaterial = true;

			FCollisionResponseParams FCRP = FCollisionResponseParams(ECR_Overlap/*ECR_Block*/);
			TArray<FHitResult> arrHits = TArray<FHitResult>();
			bIsHit = pGenWorld->SweepMultiByChannel(arrHits, ObjectTransf.GetLocation(), ObjectTransf.GetLocation() + FVector(0, 0, 1), FQuat::Identity, CollisionChannel, sphereColl, FCQP, FCRP);
			bIsHit = arrHits.Num() > 0;
			if (bIsHit)
			{
				TArray<AActor*> ArrHitedActors = TArray<AActor*>();
				TArray<UPrimitiveComponent*> ArrHitedComponents = TArray<UPrimitiveComponent*>();
				for (FHitResult& CurHit : arrHits)
				{
					if (CurHit.GetActor())
					{
						ArrHitedActors.AddUnique(CurHit.GetActor());
					}
					if (CurHit.GetComponent())
					{
						ArrHitedComponents.AddUnique(CurHit.GetComponent());
					}
				}
				UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();

				for (AActor* pHitedActor : ArrHitedActors)
				{
					if (bOnlyForGenerated)
					{
						if (pProcGenManager)
						{
							if (pProcGenManager->IsActorCreatedByGenerator(pHitedActor))
							{
								return true;
							}

							if ((pCurOptProcGenActor != pHitedActor) && bOnlyForOthersGenerated)
							{
								if (pProcGenManager->IsActorAreGenerator(pHitedActor))
								{
									return true;
								}
							}
							if ((pCurOptProcGenActor == pHitedActor) && bOnlyForOthersGenerated)
							{
								//TODO
							}
							else
							{
								if (pProcGenManager->IsActorAreGenerator(pHitedActor))
								{
									return true;
								}
							}
						}
					}
					else
					{
						ALandscape* pLandscape = Cast<ALandscape>(pHitedActor);
						if (!pLandscape)
						{
							return true;
						}
					}
				}

				for (UPrimitiveComponent* pHitedPrimitiveComponent : ArrHitedComponents)
				{
					if (bOnlyForGenerated)
					{
						AProcGenActor* OwnerProcGen = Cast<AProcGenActor>(pHitedPrimitiveComponent->GetOwner());
						if (OwnerProcGen && bOnlyForOthersGenerated)
						{
							if (pHitedPrimitiveComponent->GetOwner() != pCurOptProcGenActor)
							{
								return true;
							}
						}
						else if (OwnerProcGen)
						{
							return true;
						}
					}
					else
					{
						ALandscape* pOwLandscape = Cast<ALandscape>(pHitedPrimitiveComponent->GetOwner());
						if (!pOwLandscape)
						{
							return true;
						}
					}
				}
			}
		}

		if (ObjectHeight > 0.0f)
		{
			ResizeFByScale3D(ObjectTransf.GetScale3D(), ObjectHeight);

			FCollisionQueryParams TraceParams(FName(TEXT("CheckTrace")), true);
			TraceParams.bReturnPhysicalMaterial = true;
			FHitResult CheckHit = FHitResult(ForceInit);
			bool bIsHit = false;
			FVector PointEnd = ObjectTransf.GetLocation() + ((ObjectTransf.GetRotation() * FVector(0, 0, 1)) * ObjectHeight);
			FVector PointEnd2 = ObjectTransf.GetLocation() + FVector(0, 0, 1);
			UKismetSystemLibrary::DrawDebugLine(pGenWorld, PointEnd, PointEnd2, FLinearColor::Blue, 5.3f);
			bIsHit = pGenWorld->LineTraceSingleByChannel(CheckHit, PointEnd2, PointEnd, ECC_Visibility, TraceParams);
			if (bIsHit)
			{
				//if(bOnlyForGenerated)
				return true;
			}
		}

		return false;
	}
};

void UPGSObj::RequestGenerateInBBoxWithShapeBorder(const TArray<FVector>& GenerationBBoxPoints, /*const */UWorld* pGenerationWorld, float GenerationPower, FPoly& ShapeBorder,
	TArray<FPoly>& ExclusionBorders, AActor* OptProcGenActor, FVector OptGenerationDir, FVector OptAlignDir, float OptGenDirTraceMaxDist, bool bOptGenDirNoOutOfBounds, float OptAlignYaw, bool bShowProgress)
{
	/*{
		FString MsgTextStr = FString::Printf(TEXT("UPGSObj::RequestGenerateInBBoxWithShapeBorder ExclusionBorders num - %i"), ExclusionBorders.Num());
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));
	}*/

	if (GenerationPower <= 0.0f)
		return;

	if (GenerationBBoxPoints.Num() < 2)
		return;

	if (!pGenerationWorld)
		return;

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return;

	if (bEventGenOnly)
	{
		FString STaskMainMsgTextStr = "Generation started by blueprintable event";
		FScopedSlowTask GSTMainBP(100.0f, FText::FromString(STaskMainMsgTextStr));
		if(bShowProgress)
			GSTMainBP.MakeDialog();

		FGenerationInitialData InitialData = FGenerationInitialData();
		InitialData.bOptGenDirNoOutOfBounds = bOptGenDirNoOutOfBounds;
		InitialData.ExclusionBorders = ExclusionBorders;
		InitialData.GenerationBBoxPoints = GenerationBBoxPoints;
		InitialData.GenerationPower = GenerationPower;
		InitialData.OptAlignDir = OptAlignDir;
		InitialData.OptAlignYaw = OptAlignYaw;
		InitialData.OptGenDirTraceMaxDist = OptGenDirTraceMaxDist;
		InitialData.OptGenerationDir = OptGenerationDir;
		InitialData.OptProcGenActor = OptProcGenActor;
		InitialData.pGenerationWorld = pGenerationWorld;
		InitialData.ShapeBorder = ShapeBorder;
		InitialData.bShowProgress = bShowProgress;
		BPImplStartGenerateEvent(InitialData);

		if (bShowProgress)
			GSTMainBP.EnterProgressFrame(100.0f);

		return;
	}

	FString STaskMainMsgTextStr = FString::Printf(TEXT("Procedural Generation started, slots to generate - %i, prepare additional generation data"), GenerationParamsArr.Num());
	FScopedSlowTask GSTMain(100.0f, FText::FromString(STaskMainMsgTextStr));
	if (bShowProgress)
		GSTMain.MakeDialog();

	pCurOptProcGenActor = OptProcGenActor;

	

	FBox GenBox = FBox(GenerationBBoxPoints);

	TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
	ShapeBorder.Triangulate(nullptr, TrianglesPolyArr);

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(OptProcGenActor);
	if (pParentProcGenActor)
	{
		pParentProcGenActor->LinkedGenSlotsUIDs.Empty();
	}

	bool bUseOptGenDir = !OptGenerationDir.IsZero();
	bool bUseOptAlignDir = !OptAlignDir.IsZero();

	if (bUseOptGenDir)
	{
		if (!OptGenerationDir.IsNormalized())
			OptGenerationDir.Normalize();
	}

	if (bUseOptAlignDir)
	{
		if (!OptAlignDir.IsNormalized())
			OptAlignDir.Normalize();
	}

	TArray<AProcGenParamsModifierActor*> ArrCollidedGenParamsModifierActors = pProcGenManager->GetAllCollidedGenParamsModifierActors(GenBox);
	TArray<AProcGenParamsModifierActor*> ArrCollidedAndEqualGenParamsModifierActors = TArray<AProcGenParamsModifierActor*>();
	//not needed at now
	//TArray<FGenerationGridCell*> ArrGridCells = pProcGenManager->GetGridCellsArrByBounds(GenBox);

	TArray<FGenerationGridCell*> ArrGridCellsNearest = TArray<FGenerationGridCell*>();

	FString STaskMainMsgTextStr2 = FString::Printf(TEXT("Slots to generate - %i, prepare additional generation data completed!"), GenerationParamsArr.Num());
	GSTMain.CurrentFrameScope = 0.0f;
	GSTMain.CompletedWork = 10.0f;
	GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr2));

	int32 slotsProcessed = 0;
	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (!ProcGenSlot.bIsActive)
			continue;

		if (slotsProcessed > 0)
		{
			STaskMainMsgTextStr2 = FString::Printf(TEXT("Left slots to generate - %i"), GenerationParamsArr.Num() - slotsProcessed);
			GSTMain.CurrentFrameScope = 0.0f;
			GSTMain.CompletedWork = ((float(slotsProcessed) / float(GenerationParamsArr.Num())) * 90.0f) + 10.0f;
			GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr2));
		}

		ArrCollidedAndEqualGenParamsModifierActors.Empty();
		if (ProcGenSlot.ParamsModifierActorsSetup.EnableParamsModifierActorsInfluence)
		{
			for (AProcGenParamsModifierActor* pPGPModifierActor : ArrCollidedGenParamsModifierActors)
			{
				if (pPGPModifierActor->ModifierSlotsMap.Find(ProcGenSlot.SlotUniqueId))
				{
					ArrCollidedAndEqualGenParamsModifierActors.Add(pPGPModifierActor);
				}
			}
		}

		if (ProcGenSlot.ActorsTypesToGenerate.Num() <= 0)
		{
			if (ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() <= 0)
			{
				if (ProcGenSlot.StaticMeshesTypesToGenerateWithChance.Num() <= 0)
				{
					if (ProcGenSlot.DecalTypesToGenerateWithChance.Num() <= 0)
					{
						continue;
					}
				}
			}
		}

		ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ProcGenSlot.GenerationTraceChannel);

		float GenPowerSelected = ProcGenSlot.RandomGenerationPowerMinMax.GetRandomValue(&ProcGenSlot);
		float GenCoofSelected = ProcGenSlot.RandomGenerationCoofMinMax.GetRandomValue(&ProcGenSlot);
		GenPowerSelected *= GenerationPower;
		//no scale by GenerationPower
		//GenCoofSelected *= GenerationPower;
		uint32 NumGenChecks = uint32(GenPowerSelected);
		bool bGenerate = false;
		FVector TempPoint;
		FTransform TempTransform = FTransform::Identity;
		TempTransform.SetLocation(FVector::ZeroVector);
		//FVector* ShapeIntersectPtr = nullptr;
		float ModifierInfluencePercents = 1.0f;
		float DistanceFromEdgesInfluencePercents = 1.0f;
		float DistanceFromExcludeShapesInfluencePercents = 1.0f;
		float GenCoofSelectedNv = GenCoofSelected;
		FString STaskMsgTextStr = FString::Printf(TEXT("Generation of slot started, slot Id - %i, num of next generation attempts - %i"), ProcGenSlot.SlotUniqueId, NumGenChecks);
		FScopedSlowTask GenSlowTask(100.0f, FText::FromString(STaskMsgTextStr));
		//FScopedSlowTask GenSlowTask(NumGenChecks, FText::FromString(STaskMsgTextStr));
		//GenSlowTask.Initialize();
		//GenSlowTask.MakeDialogDelayed(0.5f, false, false);
		if (bShowProgress)
			GenSlowTask.MakeDialog();
		
		uint32 NextProgressFrame = 0;
		for (uint32 i = 0; i < NumGenChecks; ++i)
		{
			//update
			GenCoofSelected = ProcGenSlot.RandomGenerationCoofMinMax.GetRandomValue(&ProcGenSlot);
			GenCoofSelectedNv = GenCoofSelected;
			//update
			if (i >= NextProgressFrame)
			{
				NextProgressFrame += 500;
				if (NextProgressFrame >= NumGenChecks)
				{
					NextProgressFrame = NumGenChecks - 1;
				}
				FString STaskMsgTextStr2 = FString::Printf(TEXT("Generation of slot process, slot Id - %i, num of next generation attempts - %i"), ProcGenSlot.SlotUniqueId, NumGenChecks - i);
				GenSlowTask.CurrentFrameScope = 0.0f;
				GenSlowTask.CompletedWork = (float(i) / float(NumGenChecks)) * 100.0f;
				if (bShowProgress)
					GenSlowTask.EnterProgressFrame(0.0f, FText::FromString(STaskMsgTextStr2));
			}

			if (IsPendingKillOrUnreachable())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation interrupted! Generation ERROR C7"));
				return;
			}

			AProcGenParamsModifierActor* pSelectedPGPModifierActor = nullptr;

			TempPoint = RandomPointInBoxByGenSlotParams(ProcGenSlot, GenBox);

			if (!LinePolyIntersection(TrianglesPolyArr, TempPoint, TempPoint - FVector(0, 0, 6553500.0f)))
			{
				//no intersection
				continue;
			}

			if (ArrCollidedAndEqualGenParamsModifierActors.Num() > 0)
			{
				for (AProcGenParamsModifierActor* pPGPModifierActor : ArrCollidedAndEqualGenParamsModifierActors)
				{
					if (pPGPModifierActor->IsPointInsideShape2D(TempPoint))
					{
						pSelectedPGPModifierActor = pPGPModifierActor;
						break;
					}
				}
			}

			GenCoofSelectedNv = GenCoofSelected;
			FGenModifierSlotParams* GenModifierSlotParams = nullptr;
			if (pSelectedPGPModifierActor)
			{
				GenModifierSlotParams = pSelectedPGPModifierActor->ModifierSlotsMap.Find(ProcGenSlot.SlotUniqueId);
				if (GenModifierSlotParams)
				{
					ModifierInfluencePercents = 1.0f;
					if (GenModifierSlotParams->bUseLinearModifierPowerFromShapeEdgesToShapeCenter)
					{
						ModifierInfluencePercents = pSelectedPGPModifierActor->GetPercentDistanceToShapeEdgeFromPointToShapeCenter(TempPoint, ProcGenSlot.ParamsModifierActorsSetup.b2DCalculations);
						ModifierInfluencePercents = FMath::Clamp(ModifierInfluencePercents, 0.0f, 1.0f);
						if (ProcGenSlot.ParamsModifierActorsSetup.InvertInfluencePercents)
						{
							ModifierInfluencePercents = 1.0f - ModifierInfluencePercents;
						}

						if (GenModifierSlotParams->ShapeEdgesToCenterPowerGlobalCurve)
						{
							ModifierInfluencePercents = GenModifierSlotParams->ShapeEdgesToCenterPowerGlobalCurve->GetFloatValue(ModifierInfluencePercents);
						}

						ModifierInfluencePercents *= ProcGenSlot.ParamsModifierActorsSetup.InfluenceMinMaxMult.GetRandomValue(&ProcGenSlot);
						ModifierInfluencePercents = FMath::Clamp(ModifierInfluencePercents, ProcGenSlot.ParamsModifierActorsSetup.InfluenceMinMax.MinValue, ProcGenSlot.ParamsModifierActorsSetup.InfluenceMinMax.MaxValue);
					}

					float AdditionalGenCoof = UKismetMathLibrary::RandomFloatInRangeFromStream(GenModifierSlotParams->GenerationChanceMinMaxModifier.MinValue,
						GenModifierSlotParams->GenerationChanceMinMaxModifier.MaxValue, ProcGenSlot.CurrentGenerationStream);

					if (ProcGenSlot.ParamsModifierActorsSetup.EnableLinearParamsModifierActorsInfluence)
					{
						AdditionalGenCoof *= ModifierInfluencePercents;
					}

					if (GenModifierSlotParams->bAdditiveModifier)
					{
						GenCoofSelectedNv += AdditionalGenCoof;
					}
					else
					{
						GenCoofSelectedNv = AdditionalGenCoof;
					}
				}
			}

			if (ProcGenSlot.DistanceFromEdgesGenerationParams.EnableDistantBasedGeneration)
			{
				if (pParentProcGenActor)
				{
					float AdditionalGenCoof = ProcGenSlot.DistanceFromEdgesGenerationParams.DistancePercentsModifyGenCoofMinMax.GetRandomValue(&ProcGenSlot);
					if (!pParentProcGenActor->bGenerationOnSpline)
					{
						DistanceFromEdgesInfluencePercents = pParentProcGenActor->GetPercentDistanceToShapeEdgeFromPointToShapeCenter(TempPoint, true);
					}
					else
					{
						DistanceFromEdgesInfluencePercents = pParentProcGenActor->GetPercentDistanceToSplineNearestCenterFromPoint(TempPoint, true);
					}

					DistanceFromEdgesInfluencePercents = FMath::Clamp(DistanceFromEdgesInfluencePercents, 0.0f, 1.0f);
					if (ProcGenSlot.DistanceFromEdgesGenerationParams.InvertDistancePercents)
					{
						DistanceFromEdgesInfluencePercents = 1.0f - DistanceFromEdgesInfluencePercents;
					}
					DistanceFromEdgesInfluencePercents *= ProcGenSlot.DistanceFromEdgesGenerationParams.DistancePercentsMult;
					DistanceFromEdgesInfluencePercents = FMath::Clamp(DistanceFromEdgesInfluencePercents, ProcGenSlot.DistanceFromEdgesGenerationParams.DistancePercentsMinMaxToGeneration.MinValue,
						ProcGenSlot.DistanceFromEdgesGenerationParams.DistancePercentsMinMaxToGeneration.MaxValue);
				
					if (ProcGenSlot.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenCoofCurve)
					{
						AdditionalGenCoof *= ProcGenSlot.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenCoofCurve->GetFloatValue(DistanceFromEdgesInfluencePercents);
					}
					else
					{
						AdditionalGenCoof *= DistanceFromEdgesInfluencePercents;
					}

					GenCoofSelectedNv += AdditionalGenCoof;
				}
			}

			if (ProcGenSlot.DistanceFromExcludeShapesGenerationParams.EnableExcludeShapesDistanceBasedGeneration)
			{
				if (pParentProcGenActor)
				{
					DistanceFromExcludeShapesInfluencePercents = pParentProcGenActor->GetDistanceToExcludedShapes(TempPoint) / ProcGenSlot.DistanceFromExcludeShapesGenerationParams.MaxExcludeShapesDistance;
					DistanceFromExcludeShapesInfluencePercents = FMath::Clamp(DistanceFromExcludeShapesInfluencePercents, 0.0f, 1.0f);
					if (ProcGenSlot.DistanceFromExcludeShapesGenerationParams.InvertDistancePercents)
					{
						DistanceFromExcludeShapesInfluencePercents = 1.0f - DistanceFromExcludeShapesInfluencePercents;
					}
					DistanceFromExcludeShapesInfluencePercents *= ProcGenSlot.DistanceFromExcludeShapesGenerationParams.DistancePercentsMult;
					DistanceFromExcludeShapesInfluencePercents = FMath::Clamp(DistanceFromExcludeShapesInfluencePercents, ProcGenSlot.DistanceFromExcludeShapesGenerationParams.DistancePercentsMinMaxToGeneration.MinValue,
						ProcGenSlot.DistanceFromExcludeShapesGenerationParams.DistancePercentsMinMaxToGeneration.MaxValue);

					float AdditionalGenCoof = ProcGenSlot.DistanceFromExcludeShapesGenerationParams.DistancePercentsModifyGenCoofMinMax.GetRandomValue(&ProcGenSlot);
					if (ProcGenSlot.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenCoofCurve)
					{
						AdditionalGenCoof *= ProcGenSlot.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenCoofCurve->GetFloatValue(DistanceFromExcludeShapesInfluencePercents);
					}
					else
					{
						AdditionalGenCoof *= DistanceFromExcludeShapesInfluencePercents;
					}
					GenCoofSelectedNv += AdditionalGenCoof;
				}
			}

			bGenerate = RandomBoolByGenSlotParams(ProcGenSlot, GenCoofSelectedNv);
			if (bGenerate)
			{
				FBox BoundsOfNewObject = FBox(ForceInit);
				
				
				//else
				{
					//process excluded shapes

					bool needContinue = false;

					if (LinePolyIntersection(ExclusionBorders, TempPoint, TempPoint - FVector(0, 0, 6553500.0f)))
					{
						needContinue = true;
					}

					if (needContinue)
					{
						continue;
					}

					
				}

				//UKismetSystemLibrary::DrawDebugLine(pGenerationWorld, TempPoint, FVector(TempPoint.X, TempPoint.Y, -65535.0f), FLinearColor::Red, 15.3f);

				FCollisionQueryParams TraceParams(FName(TEXT("CheckTrace")), true);
				TraceParams.bReturnPhysicalMaterial = true;
				if (pParentProcGenActor && !pParentProcGenActor->bSelfCollideWhenGeneration)
				{
					TraceParams.AddIgnoredActor(pParentProcGenActor);
				}
				FHitResult CheckHit = FHitResult(ForceInit);
				bool bIsHit = false;
				FVector PointEnd = FVector(TempPoint.X, TempPoint.Y, TempPoint.Z + -6553500.0f);
				FVector PointEnd2 = FVector(TempPoint.X, TempPoint.Y, TempPoint.Z + 6553500.0f);
				FVector OriginalHitLoc = TempPoint;
				if (bUseOptGenDir)
				{
					//if (!OptGenerationDir.IsNormalized())
					//	OptGenerationDir.Normalize();

					PointEnd = TempPoint + (OptGenerationDir * ((OptGenDirTraceMaxDist > 0.0f) ? OptGenDirTraceMaxDist : 6553500.0f));
					PointEnd2 = TempPoint - (OptGenerationDir * ((OptGenDirTraceMaxDist > 0.0f) ? OptGenDirTraceMaxDist : 6553500.0f));
				}

				if (!ProcGenSlot.FloatingInAirGeneration)
				{
					if (!ProcGenSlot.bUseMultiLT)
					{
						bIsHit = pGenerationWorld->LineTraceSingleByChannel(CheckHit, TempPoint, PointEnd, CollisionChannel, TraceParams);
						if (!bIsHit && ProcGenSlot.UnderTerrainFixGeneration)
						{
							bIsHit = pGenerationWorld->LineTraceSingleByChannel(CheckHit, TempPoint, PointEnd2, CollisionChannel, TraceParams);
							if (bIsHit)
							{
								//invert hit normal(backhit)
								CheckHit.Normal = -CheckHit.Normal;
							}
						}

						if (bIsHit)
						{
							if (pParentProcGenActor)
							{
								if (!pParentProcGenActor->IsPointInsideConnectedShapeLimitActors(CheckHit.Location))
								{
									continue;
								}

								if (pParentProcGenActor->ShapeLimitActors.Contains(CheckHit.Actor.Get()))
								{
									continue;
								}
							}

							if (pProcGenManager->IsActorAreGenerator(CheckHit.Actor.Get()))
							{
								if (!ProcGenSlot.bAllowGenerationOnOtherGenActors)
								{
									continue;
								}
							}
							if (pProcGenManager->IsActorCreatedByGenerator(CheckHit.Actor.Get()))
							{
								if (!ProcGenSlot.bAllowGenerationOnOtherGenActors)
								{
									continue;
								}
							}

							if (ProcGenSlot.SurfaceActorsWithTagsAllowed.Num() > 0)
							{
								if (CheckHit.Actor.Get())
								{
									bool bHaveTag = false;
									for (const FName& TagCur : CheckHit.Actor.Get()->Tags)
									{
										if (ProcGenSlot.SurfaceActorsWithTagsAllowed.Contains(TagCur))
										{
											bHaveTag = true;
											break;
										}
									}

									if (!bHaveTag)
									{
										continue;
									}
								}
							}
						}
					}
					else
					{
						FCollisionResponseParams FCRPX = FCollisionResponseParams(ECR_Overlap/*ECR_Block*/);
						TArray<FHitResult> HitsRes = TArray<FHitResult>();
						bIsHit = pGenerationWorld->LineTraceMultiByChannel(HitsRes, TempPoint, PointEnd, CollisionChannel, TraceParams, FCRPX);
						bIsHit = HitsRes.Num() > 0;
						if (!bIsHit && ProcGenSlot.UnderTerrainFixGeneration)
						{
							bIsHit = pGenerationWorld->LineTraceMultiByChannel(HitsRes, TempPoint, PointEnd2, CollisionChannel, TraceParams, FCRPX);
							bIsHit = HitsRes.Num() > 0;
						}
						if (bIsHit)
						{
							bool bRequestHitFailure = false;
							AActor* HitActor = nullptr;
							for (FHitResult& CurHitRes : HitsRes)
							{
								HitActor = CurHitRes.Actor.Get();
								if (HitActor)
								{
									bool bIsThisHitFiltered = false;
									if (pProcGenManager->IsActorAreGenerator(HitActor))
									{
										if (!ProcGenSlot.bAllowGenerationOnOtherGenActors)
										{
											//TODO skip generation
											//bIsHit = false;
											//break;
											bRequestHitFailure = true;
											bIsThisHitFiltered = true;
										}
									}
									if (pProcGenManager->IsActorCreatedByGenerator(HitActor))
									{
										if (!ProcGenSlot.bAllowGenerationOnOtherGenActors)
										{
											//TODO skip generation
											//bIsHit = false;
											//break;
											bRequestHitFailure = true;
											bIsThisHitFiltered = true;
										}
									}
									if (pParentProcGenActor)
									{
										if (!pParentProcGenActor->IsPointInsideConnectedShapeLimitActors(CurHitRes.Location))
										{
											bIsThisHitFiltered = true;
											continue;
										}

										if (pParentProcGenActor->ShapeLimitActors.Contains(CurHitRes.Actor.Get()))
										{
											bIsThisHitFiltered = true;
											continue;
										}
									}
									bool bIsLandscape = HitActor->IsA(ALandscape::StaticClass());
									bool bIsStaticMesh = HitActor->IsA(AStaticMeshActor::StaticClass());
									bool bIsStaticMeshComponent = HitActor->IsA(AStaticMeshActor::StaticClass());
									if (Cast<UStaticMeshComponent>(CurHitRes.GetComponent()))
									{
										bIsStaticMeshComponent = true;
									}
									//prefer generation on actors with selected tag
									if (ProcGenSlot.SurfaceActorsWithTagsAllowed.Num() > 0)
									{
										if (HitActor)
										{
											bool bHaveTag = false;
											for (const FName& TagCur : HitActor->Tags)
											{
												if (ProcGenSlot.SurfaceActorsWithTagsAllowed.Contains(TagCur))
												{
													bHaveTag = true;
													break;
												}
											}

											if (!bHaveTag)
											{
												continue;
											}
											else
											{
												bRequestHitFailure = false;
												bIsThisHitFiltered = false;
												CheckHit = CurHitRes;
												break;
											}
										}
									}

									if (ProcGenSlot.bGenerateOnLandscape && bIsLandscape)
									{
										//prefer generation on landscape
										CheckHit = CurHitRes;
										if (!bIsThisHitFiltered && (ProcGenSlot.bPrefFirstHit || ProcGenSlot.bPrefLandscape))
										{
											bRequestHitFailure = false;
											break;
										}
									}

									if (ProcGenSlot.bGenerateOnStaticMeshes && (bIsStaticMesh || bIsStaticMeshComponent))
									{
										CheckHit = CurHitRes;
										if (!bIsThisHitFiltered && (ProcGenSlot.bPrefFirstHit && !ProcGenSlot.bPrefLandscape))
										{
											bRequestHitFailure = false;
											break;
										}
									}
								}
							}

							if (bRequestHitFailure)
							{
								bIsHit = false;
								continue;
							}
						}
					}
				}

				if (!ProcGenSlot.FloatingInAirGeneration && bIsHit && (ProcGenSlot.SurfaceTypesAllowed.Num() > 0))
				{
					uint8 HitedSurfaceType = 255;
					if (CheckHit.PhysMaterial.IsValid())
					{
						EPhysicalSurface HitSurfaceType = UPhysicalMaterial::DetermineSurfaceType(CheckHit.PhysMaterial.Get());
						HitedSurfaceType = uint8(HitSurfaceType);
						if (!ProcGenSlot.SurfaceTypesAllowed.Contains(HitedSurfaceType))
						{
							continue;
						}
					}
				}

				if (!ProcGenSlot.FloatingInAirGeneration && bIsHit && (ProcGenSlot.TerrainLayersAllowed.Num() > 0))
				{
					ULandscapeHeightfieldCollisionComponent* pHitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(CheckHit.GetComponent());
					if (pHitLandscapeCollision)
					{
						ULandscapeComponent* pLComp = Cast<ULandscapeComponent>(pHitLandscapeCollision->RenderComponent.Get());
						if (pLComp)
						{
							float fWeightComplete = 0.0f;
							for (FString& LayersAllowedStr : ProcGenSlot.TerrainLayersAllowed)
							{
								fWeightComplete += pLComp->EditorGetPaintLayerWeightByNameAtLocation(CheckHit.ImpactPoint, *LayersAllowedStr);
							}

							if (fWeightComplete < ProcGenSlot.MinimalTerrainLayerWeightToGenerate)
							{
								continue;
							}
						}
					}
				}

				if (bUseOptGenDir && bOptGenDirNoOutOfBounds)
				{
					if (bIsHit)
					{
						if (!LinePolyIntersection(TrianglesPolyArr, CheckHit.Location + FVector(0, 0, 6553500), CheckHit.Location - FVector(0, 0, 6553500)))
						{
							//no intersection
							continue;
						}
					}
				}

				UClass* pSelectedActorClass = nullptr;
				//UStaticMesh* pSelectedSM = nullptr;
				//FStaticMeshCollisionSetup* pSMCollisionSetup = nullptr;
				FStaticMeshTypeWithChanceTG* pSelectedSMSetupTG = nullptr;
				FDecalTypeWithChanceTG* pDecalTypeSetup = nullptr;
				if (ProcGenSlot.StaticMeshesTypesToGenerateWithChance.Num() > 0)
				{
					int32 icf = 0;
					uint32 WhileCounterRt = 0;
					while (pSelectedSMSetupTG == nullptr)
					{
						for (FStaticMeshTypeWithChanceTG& MeshTypeWC : ProcGenSlot.StaticMeshesTypesToGenerateWithChance)
						{
							if (RandomBoolByGenSlotParams(ProcGenSlot, MeshTypeWC.GenerationChance))
							{
								pSelectedSMSetupTG = &MeshTypeWC;
							}
							++icf;
						}
						icf = 0;
						++WhileCounterRt;
						if (WhileCounterRt >= 1000)
						{
							//safety
							break;
						}
					}

					if (!pSelectedSMSetupTG)
					{
						int32 SelectedId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, ProcGenSlot.StaticMeshesTypesToGenerateWithChance.Num() - 1, ProcGenSlot.CurrentGenerationStream);
						if (SelectedId < 0 || SelectedId >= ProcGenSlot.StaticMeshesTypesToGenerateWithChance.Num())
						{
							SelectedId = 0;
						}
						pSelectedSMSetupTG = &ProcGenSlot.StaticMeshesTypesToGenerateWithChance[SelectedId];
					}
				}
				else
				{
					int32 SelectedActorClassId = 0;
					if (ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() <= 0)
					{
						SelectedActorClassId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, ProcGenSlot.ActorsTypesToGenerate.Num() - 1, ProcGenSlot.CurrentGenerationStream);
						if (SelectedActorClassId < 0 || SelectedActorClassId >= ProcGenSlot.ActorsTypesToGenerate.Num())
						{
							SelectedActorClassId = 0;
						}
					}
					else
					{
						SelectedActorClassId = -1;
						int32 icf = 0;
						uint32 WhileCounterRt = 0;
						while (SelectedActorClassId == -1)
						{
							for (FActorTypeWithChanceTG& ActorTypeWC : ProcGenSlot.ActorsTypesWithChanceToGenerate)
							{
								if (RandomBoolByGenSlotParams(ProcGenSlot, ActorTypeWC.GenerationChance))
								{
									SelectedActorClassId = icf;
									break;
								}
								++icf;
							}
							icf = 0;
							++WhileCounterRt;
							if (WhileCounterRt >= 1000)
							{
								//safety
								break;
							}
						}

						if (SelectedActorClassId == -1)
						{
							SelectedActorClassId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() - 1, ProcGenSlot.CurrentGenerationStream);
						}
					}

					if (ProcGenSlot.ActorsTypesWithChanceToGenerate.Num() <= 0)
					{
						if (ProcGenSlot.ActorsTypesToGenerate.Num() > SelectedActorClassId)
							pSelectedActorClass = ProcGenSlot.ActorsTypesToGenerate[SelectedActorClassId];
					}
					else
					{
						if (SelectedActorClassId >= 0 && SelectedActorClassId < ProcGenSlot.ActorsTypesWithChanceToGenerate.Num())
						{
							pSelectedActorClass = ProcGenSlot.ActorsTypesWithChanceToGenerate[SelectedActorClassId].ActorClass;
						}
					}

					if (pSelectedActorClass == nullptr)
					{
						//continue;
					}
				}

				if (ProcGenSlot.DecalTypesToGenerateWithChance.Num() > 0)
				{
					int32 icf = 0;
					uint32 WhileCounterRt = 0;
					while (pDecalTypeSetup == nullptr)
					{
						for (FDecalTypeWithChanceTG& DecalTypeWC : ProcGenSlot.DecalTypesToGenerateWithChance)
						{
							if (RandomBoolByGenSlotParams(ProcGenSlot, DecalTypeWC.GenerationChance))
							{
								pDecalTypeSetup = &DecalTypeWC;
							}
							++icf;
						}
						icf = 0;
						++WhileCounterRt;
						if (WhileCounterRt >= 1000)
						{
							//safety
							break;
						}
					}

					if (!pDecalTypeSetup)
					{
						int32 SelectedId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, ProcGenSlot.DecalTypesToGenerateWithChance.Num() - 1, ProcGenSlot.CurrentGenerationStream);
						if (SelectedId < 0 || SelectedId >= ProcGenSlot.DecalTypesToGenerateWithChance.Num())
						{
							SelectedId = 0;
						}
						pDecalTypeSetup = &ProcGenSlot.DecalTypesToGenerateWithChance[SelectedId];
						
					}
				}
				//if (/*!ProcGenSlot.DistanceBetweenMinMax.IsIgnored()*/)
				{
					float NewMinDistanceCalc = ProcGenSlot.DistanceBetweenMinMax.GetRandomValue(&ProcGenSlot);
					float BoxExt = NewMinDistanceCalc / 2.0f;
					BoundsOfNewObject = FBox::BuildAABB(bIsHit ? CheckHit.Location : TempPoint, FVector(BoxExt, BoxExt, BoxExt));
					//if (!bDelayedGeneration)
					//{
						/*if (!ProcGenSlot.DistanceBetweenMinMax.IsIgnored())
						{
							if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
							{
								bool bNeedContinue = false;
								for (AActor* pActor : ProcGenSlot.GeneratedActorsPtrs)
								{
									if (BoundsOfNewObject.IsInsideXY(pActor->GetActorLocation()))
									{
										if (FVector::Dist(pActor->GetActorLocation(), bIsHit ? CheckHit.Location : TempPoint) < NewMinDistanceCalc)
										{
											bNeedContinue = true;
											break;
										}
									}
								}

								if (bNeedContinue)
								{
									continue;
								}
							}
						}
						BoxExt = ProcGenSlot.DefCollisionSize * 2.0f;
						BoundsOfNewObject = FBox::BuildAABB(bIsHit ? CheckHit.Location : TempPoint, FVector(BoxExt, BoxExt, BoxExt));
						if (ProcGenSlot.TakenIntoAccSlotsIds.Num() > 0)
						{
							bool bNeedContinue = false;
							TArray<FProcGenSlotParams*> TakenSlots = GetProcGenSlotsByUIds(ProcGenSlot.TakenIntoAccSlotsIds);
							if (TakenSlots.Num() > 0)
							{
								for (FProcGenSlotParams* pSlotParams : TakenSlots)
								{
									if (pSlotParams->GeneratedActorsPtrs.Num() > 0)
									{
										for (AActor* pActor : pSlotParams->GeneratedActorsPtrs)
										{
											if (BoundsOfNewObject.IsInsideXY(pActor->GetActorLocation()))
											{
												if (FVector::Dist(pActor->GetActorLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
												{
													bNeedContinue = true;
													break;
												}
											}
										}
									}

									if (bNeedContinue)
									{
										break;
									}
								}

								if (bNeedContinue)
								{
									continue;
								}
							}
						}*/
					//}
					//else
					{
						if (!ProcGenSlot.DistanceBetweenMinMax.IsIgnored())
						{
							if (!ProcGenSlot.bEnableGridBasedDistanceCheckOptimization)
							{
								if (ProcGenSlot.TempTransformsForObjects.Num() > 0)
								{
									bool bNeedContinue = false;
									for (FTransform OldTrasf : ProcGenSlot.TempTransformsForObjects)
									{
										if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
										{
											if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < NewMinDistanceCalc)
											{
												bNeedContinue = true;
												break;
											}
										}
									}

									if (bNeedContinue)
									{
										continue;
									}
								}
							}
							else
							{
								bool bNeedContinue = false;
								ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(bIsHit ? CheckHit.Location : TempPoint);
								if (ArrGridCellsNearest.Num() > 0)
								{
									for (FGenerationGridCell* pCell : ArrGridCellsNearest)
									{
										if (!pCell)
											continue;

										FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(ProcGenSlot.SlotUniqueId);
										if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
										{
											for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
											{
												if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
												{
													if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < NewMinDistanceCalc)
													{
														bNeedContinue = true;
														break;
													}
												}
											}
										}

										if (bNeedContinue)
										{
											break;
										}
									}
								}

								if (bNeedContinue)
								{
									continue;
								}
							}
						}
						//
						BoxExt = ProcGenSlot.DefCollisionSize * 2.0f;
						BoundsOfNewObject = FBox::BuildAABB(bIsHit ? CheckHit.Location : TempPoint, FVector(BoxExt, BoxExt, BoxExt));

						if (ProcGenSlot.TakenIntoAccSlotsIds.Num() > 0)
						{
							bool bNeedContinue = false;
							TArray<FProcGenSlotParams*> TakenSlots = GetProcGenSlotsByUIds(ProcGenSlot.TakenIntoAccSlotsIds);
							if (TakenSlots.Num() > 0)
							{
								for (FProcGenSlotParams* pSlotParams : TakenSlots)
								{
									if (!pSlotParams->bEnableGridBasedDistanceCheckOptimization)
									{
										if (pSlotParams->TempTransformsForObjects.Num() > 0)
										{
											for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
											{
												if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
												{
													if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
													{
														bNeedContinue = true;
														break;
													}
												}
											}
										}
									}
									else
									{
										ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(bIsHit ? CheckHit.Location : TempPoint);
										if (ArrGridCellsNearest.Num() > 0)
										{
											for (FGenerationGridCell* pCell : ArrGridCellsNearest)
											{
												if (!pCell)
													continue;

												FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(pSlotParams->SlotUniqueId);
												if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
												{
													for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
													{
														if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
														{
															if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
															{
																bNeedContinue = true;
																break;
															}
														}
													}
												}

												if (bNeedContinue)
												{
													break;
												}
											}
										}
									}

									if (bNeedContinue)
									{
										break;
									}
								}

								if (bNeedContinue)
								{
									continue;
								}
							}
						}
					}
				}

				if (pParentProcGenActor)
				{
					if (pParentProcGenActor->OthersPGAsUsedAsTIA.Num() > 0)
					{
						float BoxExt = ProcGenSlot.DefCollisionSize * 2.0f;
						BoundsOfNewObject = FBox::BuildAABB(bIsHit ? CheckHit.Location : TempPoint, FVector(BoxExt, BoxExt, BoxExt));

						bool bNeedContinue = false;
						for (AProcGenActor* OthersPGA_Ptr : pParentProcGenActor->OthersPGAsUsedAsTIA)
						{
							if (!OthersPGA_Ptr)
								continue;

							if (OthersPGA_Ptr->CurCreatedProcGenSlotObject.Get())
							{
								TArray<FProcGenSlotParams*> TakenSlots = OthersPGA_Ptr->CurCreatedProcGenSlotObject.Get()->GetProcGenSlotsByUIds(ProcGenSlot.TakenIntoAccSlotsIds);
								if (TakenSlots.Num() > 0)
								{
									for (FProcGenSlotParams* pSlotParams : TakenSlots)
									{
										if (!pSlotParams->bEnableGridBasedDistanceCheckOptimization)
										{
											if (pSlotParams->TempTransformsForObjects.Num() > 0)
											{
												for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
												{
													if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
													{
														if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
														{
															bNeedContinue = true;
															break;
														}
													}
												}
											}
										}
										else
										{
											ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(bIsHit ? CheckHit.Location : TempPoint);
											if (ArrGridCellsNearest.Num() > 0)
											{
												for (FGenerationGridCell* pCell : ArrGridCellsNearest)
												{
													if (!pCell)
														continue;

													FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(pSlotParams->SlotUniqueId);
													if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
													{
														for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
														{
															if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
															{
																if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
																{
																	bNeedContinue = true;
																	break;
																}
															}
														}
													}

													if (bNeedContinue)
													{
														break;
													}
												}
											}
										}

										if (bNeedContinue)
										{
											break;
										}
									}

									if (bNeedContinue)
									{
										break;
									}
								}
							}
						}

						if (bNeedContinue)
						{
							continue;
						}
					}
				}

				if (!ProcGenSlot.HeightMinMax.IsIgnored())
				{
					float SelectedZ = bIsHit ? CheckHit.Location.Z : TempPoint.Z;
					if (SelectedZ > ProcGenSlot.HeightMinMax.MaxValue || SelectedZ < ProcGenSlot.HeightMinMax.MinValue)
					{
						continue;
					}
				}

				FRotator InitialRotation = FRotator::ZeroRotator;

				FVector SelectedScale = FVector(1);
				if (!ProcGenSlot.ScaleMinMax.IsIgnored())
				{
					float SelectedSize = ProcGenSlot.ScaleMinMax.GetRandomValue(&ProcGenSlot);
					SelectedScale *= SelectedSize;
				}

				if (!ProcGenSlot.RandomNUScaleXMinMax.IsIgnored())
				{
					SelectedScale.X += ProcGenSlot.RandomNUScaleXMinMax.GetRandomValue(&ProcGenSlot);
				}

				if (!ProcGenSlot.RandomNUScaleYMinMax.IsIgnored())
				{
					SelectedScale.Y += ProcGenSlot.RandomNUScaleYMinMax.GetRandomValue(&ProcGenSlot);
				}

				if (!ProcGenSlot.RandomNUScaleZMinMax.IsIgnored())
				{
					SelectedScale.Z += ProcGenSlot.RandomNUScaleZMinMax.GetRandomValue(&ProcGenSlot);
				}

				if (GenModifierSlotParams)
				{
					float AdditionalScaleCoof = UKismetMathLibrary::RandomFloatInRangeFromStream(GenModifierSlotParams->ScaleMinMaxModifier.MinValue,
						GenModifierSlotParams->ScaleMinMaxModifier.MaxValue, ProcGenSlot.CurrentGenerationStream);

					if (ProcGenSlot.ParamsModifierActorsSetup.EnableLinearParamsModifierActorsInfluence)
					{
						AdditionalScaleCoof *= ModifierInfluencePercents;
					}

					if (GenModifierSlotParams->bAdditiveModifier)
					{
						SelectedScale *= (1.0f + AdditionalScaleCoof);
					}
					else
					{
						SelectedScale *= AdditionalScaleCoof;
					}
				}

				if (ProcGenSlot.DistanceFromEdgesGenerationParams.EnableDistantBasedGeneration)
				{
					if (pParentProcGenActor)
					{
						float AdditionalScaleCoof = ProcGenSlot.DistanceFromEdgesGenerationParams.DistancePercentsModifyScaleMinMax.GetRandomValue(&ProcGenSlot);

						if (ProcGenSlot.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenScaleCurve)
						{
							AdditionalScaleCoof *= ProcGenSlot.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenScaleCurve->GetFloatValue(DistanceFromEdgesInfluencePercents);
						}
						else
						{
							AdditionalScaleCoof *= DistanceFromEdgesInfluencePercents;
						}

						SelectedScale *= (1.0f + AdditionalScaleCoof);
					}
				}

				if (ProcGenSlot.DistanceFromExcludeShapesGenerationParams.EnableExcludeShapesDistanceBasedGeneration)
				{
					if (pParentProcGenActor)
					{
						float AdditionalScaleCoof = ProcGenSlot.DistanceFromExcludeShapesGenerationParams.DistancePercentsModifyScaleMinMax.GetRandomValue(&ProcGenSlot);

						if (ProcGenSlot.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenScaleCurve)
						{
							AdditionalScaleCoof *= ProcGenSlot.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenScaleCurve->GetFloatValue(DistanceFromExcludeShapesInfluencePercents);
						}
						else
						{
							AdditionalScaleCoof *= DistanceFromExcludeShapesInfluencePercents;
						}

						SelectedScale *= (1.0f + AdditionalScaleCoof);
					}
				}

				if (bIsHit)
				{
					OriginalHitLoc = CheckHit.Location;
					if (CheckHit.GetActor())
					{
						bool bIsLandscape = CheckHit.GetActor()->IsA(ALandscape::StaticClass());
						bool bIsStaticMesh = CheckHit.GetActor()->IsA(AStaticMeshActor::StaticClass());

						if (!ProcGenSlot.bGenerateOnLandscape && bIsLandscape)
						{
							continue;
						}

						if (!ProcGenSlot.bGenerateOnStaticMeshes && bIsStaticMesh)
						{
							continue;
						}
					}

					float GroundAngleValue = FMath::RadiansToDegrees(FMath::Acos(CheckHit.Normal.Z));
					if (ProcGenSlot.ExtendedSlopeCheckSetupParams.EnableExtendedSlopeCheck)
					{
						FExtendedSlopeCheckFunctionInParams ExtendedSlopeCheckFunctionInParams = FExtendedSlopeCheckFunctionInParams();
						ExtendedSlopeCheckFunctionInParams.CheckRadius = ProcGenSlot.ExtendedSlopeCheckSetupParams.SlopeCheckRadius;
						ExtendedSlopeCheckFunctionInParams.CurrentSlopeAngle = GroundAngleValue;
						ExtendedSlopeCheckFunctionInParams.ParentProcGenActorPtr = pParentProcGenActor;
						ExtendedSlopeCheckFunctionInParams.MultiLt = ProcGenSlot.bUseMultiLT;
						ExtendedSlopeCheckFunctionInParams.StartCheckPoint = TempPoint;
						ExtendedSlopeCheckFunctionInParams.EndCheckPoint = OriginalHitLoc;
						ExtendedSlopeCheckFunctionInParams.HeightDifferenceToDetectEdges = ProcGenSlot.ExtendedSlopeCheckSetupParams.HeightDifferenceToDetectEdges;
						FExtendedSlopeCheckFunctionOutParams ExtendedSlopeCheckFunctionOutParams = DoExtendedSlopeCheck(ExtendedSlopeCheckFunctionInParams, ProcGenSlot);
						if (ProcGenSlot.ExtendedSlopeCheckSetupParams.bUseRecalculatedSlope)
						{
							GroundAngleValue = ExtendedSlopeCheckFunctionOutParams.RecalculatedSlopeAngle;
						}

						if (ProcGenSlot.ExtendedSlopeCheckSetupParams.bGenerationOnEdgesOnly && !ExtendedSlopeCheckFunctionOutParams.EdgeIsDetected)
						{
							continue;
						}

						if (ProcGenSlot.ExtendedSlopeCheckSetupParams.bGenerationOnNotEdgesOnly && ExtendedSlopeCheckFunctionOutParams.EdgeIsDetected)
						{
							continue;
						}
					}
					if (!ProcGenSlot.SlopeMinMax.IsIgnored())
					{
						if (GroundAngleValue > ProcGenSlot.SlopeMinMax.MaxValue || GroundAngleValue < ProcGenSlot.SlopeMinMax.MinValue)
						{
							bool bNeedContinue = true;
							if (GroundAngleValue > ProcGenSlot.SlopeMinMax.MaxValue)
							{
								bool bGenOverslope = RandomBoolByGenSlotParams(ProcGenSlot, ProcGenSlot.SlopeOverMaxGenChance);
								if (bGenOverslope)
								{
									if (GroundAngleValue <= ProcGenSlot.SlopeMaxVariation.MaxValue)
									{
										bNeedContinue = false;
									}
								}
							}
							//TODO do same check for "GroundAngleValue < ProcGenSlot.SlopeMinMax.MinValue"

							if (bNeedContinue)
							{
								continue;
							}
						}
					}

					if (ProcGenSlot.AddZPosFromGroundNormal != 0.0f)
					{
						float GroundNormalZLgnScaled = ProcGenSlot.AddZPosFromGroundNormal;
						if(ProcGenSlot.bScaleAllParamsByInstScale)
						{
							GenerationHelper::ResizeFByScale3D(SelectedScale, GroundNormalZLgnScaled);
						}
						CheckHit.Location += (CheckHit.Normal * GroundNormalZLgnScaled);
					}

					if (!ProcGenSlot.AddZPosFGNMinMax.IsIgnored())
					{
						float ZPosFGNScaled = ProcGenSlot.AddZPosFGNMinMax.GetRandomValue(&ProcGenSlot);
						if (ProcGenSlot.bScaleAllParamsByInstScale)
						{
							GenerationHelper::ResizeFByScale3D(SelectedScale, ZPosFGNScaled);
						}
						CheckHit.Location += (CheckHit.Normal * ZPosFGNScaled);
					}

					if (!ProcGenSlot.PressInGroundMinMax.IsIgnored())
					{
						float ZPressInGround = ProcGenSlot.PressInGroundMinMax.GetRandomValue(&ProcGenSlot);
						if (ProcGenSlot.bScaleAllParamsByInstScale)
						{
							GenerationHelper::ResizeFByScale3D(SelectedScale, ZPressInGround);
						}
						CheckHit.Location.Z -= ZPressInGround;
					}

					if (ProcGenSlot.PressInGroundBySlopeForce != 0.0f)
					{
						float SlopePerc = GroundAngleValue / 90.0f;
						SlopePerc = FMath::Clamp(SlopePerc, 0.0f, 1.0f);
						float SlopeForceScaled = ProcGenSlot.PressInGroundBySlopeForce;
						if (ProcGenSlot.bScaleAllParamsByInstScale)
						{
							GenerationHelper::ResizeFByScale3D(SelectedScale, SlopeForceScaled);
						}
						CheckHit.Location.Z -= SlopeForceScaled * SlopePerc;
					}

					if (ProcGenSlot.bPressInGroundByNormalZ)
					{
						float SlopePercent = GroundAngleValue / 90.0f;
						if (!ProcGenSlot.PressInGroundNZMinMax.IsIgnored())
						{
							float PressZVal = ProcGenSlot.PressInGroundNZMinMax.GetRandomValue(&ProcGenSlot);
							if (ProcGenSlot.bScaleAllParamsByInstScale)
							{
								GenerationHelper::ResizeFByScale3D(SelectedScale, PressZVal);
							}
							CheckHit.Location.Z -= (PressZVal * SlopePercent);
						}
					}

					TempTransform.SetLocation(CheckHit.Location + ProcGenSlot.AdditionalLocationVector);

					if (ProcGenSlot.RotateToGroundNormal)
					{
						FRotator AlignRotation = CheckHit.Normal.Rotation();
						// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
						AlignRotation.Pitch -= 90.f;
						// Clamp its value inside +/- one rotation
						AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
						//TempTransform.SetRotation(AlignRotation);
						InitialRotation = AlignRotation;
					}

					if (bUseOptAlignDir)
					{
						//if (!OptAlignDir.IsNormalized())
						//	OptAlignDir.Normalize();

						FRotator AlignRotation = OptAlignDir.Rotation();
						// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
						AlignRotation.Pitch -= 90.f;
						// Clamp its value inside +/- one rotation
						AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
						//TempTransform.SetRotation(AlignRotation);
						InitialRotation = AlignRotation;

						if (OptAlignYaw != 0.0f)
						{
							InitialRotation.Yaw = 0.0f;
						}
					}
				}
				else
				{
					if (ProcGenSlot.FloatingInAirGeneration)
					{
						TempTransform.SetLocation(TempPoint + ProcGenSlot.AdditionalLocationVector);
						OriginalHitLoc = TempTransform.GetLocation();
					}
					else
					{
						continue;
					}
				}

				FRotator AdditionalRotation = FRotator::ZeroRotator;
				if (!ProcGenSlot.RandomRotationRollMinMax.IsIgnored())
				{
					AdditionalRotation.Roll += ProcGenSlot.RandomRotationRollMinMax.GetRandomValue(&ProcGenSlot);
				}
				if (!ProcGenSlot.RandomRotationPitchMinMax.IsIgnored())
				{
					AdditionalRotation.Pitch += ProcGenSlot.RandomRotationPitchMinMax.GetRandomValue(&ProcGenSlot);
				}
				if (!ProcGenSlot.RandomRotationYawMinMax.IsIgnored())
				{
					AdditionalRotation.Yaw += ProcGenSlot.RandomRotationYawMinMax.GetRandomValue(&ProcGenSlot);
				}

				if (OptAlignYaw != 0.0f)
				{
					AdditionalRotation.Yaw += OptAlignYaw;
					if (AdditionalRotation.Yaw > 360.0f)
					{
						AdditionalRotation.Yaw -= 360.0f;
					}
					else if (AdditionalRotation.Yaw < -360.0f)
					{
						AdditionalRotation.Yaw += 360.0f;
					}
				}

				AdditionalRotation.Normalize();

				FQuat NormalizedQRot = (FQuat(InitialRotation) * FQuat(AdditionalRotation)).GetNormalized() * FQuat(ProcGenSlot.AdditionalRotation);
				NormalizedQRot.Normalize();
				TempTransform.SetScale3D(SelectedScale);
				TempTransform.SetRotation(NormalizedQRot);
				FTransform TempTransformWithoutFlip = TempTransform;

				if (ProcGenSlot.bCollisionCheckBeforeGenerate)
				{
					float BoxExt = ProcGenSlot.DefCollisionSize / 2.0f;
					BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));

					FTransform TempTransf2 = TempTransform;
					TempTransf2.SetLocation(OriginalHitLoc);
					if (GenerationHelper::CheckCollision(pGenerationWorld, TempTransf2, ProcGenSlot.DefCollisionSize, ProcGenSlot.DefCollisionHeightSize,
						ProcGenSlot.bCollisionCheckExcludeOnlyAllPGMeshes, ProcGenSlot.bCollisionCheckExcludeOnlyOtherPGMeshes))
					{
						continue;
					}
					bool bNeedContinueBCCF = false;

					if (!ProcGenSlot.bEnableGridBasedDistanceCheckOptimization)
					{
						if (ProcGenSlot.DefCollisionHeightSize > 0.0f)
						{
							float HgtSz = ProcGenSlot.DefCollisionHeightSize;
							float HgtSz2 = ProcGenSlot.DefCollisionHeightSize;
							GenerationHelper::ResizeFByScale3D(TempTransform.GetScale3D(), HgtSz);
							FVector SegmentAPos = FVector::ZeroVector;
							FVector SegmentBPos = FVector::ZeroVector;
							for (FTransform& oldObjTrasf : ProcGenSlot.TempTransformsForObjects)
							{
								GenerationHelper::ResizeFByScale3D(oldObjTrasf.GetScale3D(), HgtSz2);
								FVector PointSegA = oldObjTrasf.GetLocation() + ((oldObjTrasf.GetRotation() * FVector(0, 0, 1)) * HgtSz2);
								FVector PointSegA2 = oldObjTrasf.GetLocation() + FVector(0, 0, 1);
								FVector PointSegB = TempTransform.GetLocation() + ((TempTransform.GetRotation() * FVector(0, 0, 1)) * HgtSz);
								FVector PointSegB2 = TempTransform.GetLocation() + FVector(0, 0, 1);

								FMath::SegmentDistToSegmentSafe(PointSegA, PointSegA2, PointSegB, PointSegB2, SegmentAPos, SegmentBPos);
								BoundsOfNewObject.MoveTo(SegmentAPos);
								if (BoundsOfNewObject.IsInsideXY(SegmentBPos))
								{
									if (FVector::Dist(SegmentAPos, SegmentBPos) < ProcGenSlot.DefCollisionSize)
									{
										bNeedContinueBCCF = true;
										break;
									}
								}
							}
						}
						else
						{
							BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));
							for (FTransform& oldObjTrasf : ProcGenSlot.TempTransformsForObjects)
							{
								if (BoundsOfNewObject.IsInsideXY(oldObjTrasf.GetLocation()))
								{
									if (FVector::Dist(oldObjTrasf.GetLocation(), TempTransform.GetLocation()) < ProcGenSlot.DefCollisionSize)
									{
										bNeedContinueBCCF = true;
										break;
									}
								}
							}
						}
					}
					else
					{
						ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(TempTransform.GetLocation());
						if (ArrGridCellsNearest.Num() > 0)
						{
							for (FGenerationGridCell* pCell : ArrGridCellsNearest)
							{
								if (!pCell)
									continue;

								FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(ProcGenSlot.SlotUniqueId);
								//CellGenSlot.TempTransformsForDistanceChecks.Add(TempTransform);
								if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
								{
									if (ProcGenSlot.DefCollisionHeightSize > 0.0f)
									{
										float HgtSz = ProcGenSlot.DefCollisionHeightSize;
										float HgtSz2 = ProcGenSlot.DefCollisionHeightSize;
										GenerationHelper::ResizeFByScale3D(TempTransform.GetScale3D(), HgtSz);
										FVector SegmentAPos = FVector::ZeroVector;
										FVector SegmentBPos = FVector::ZeroVector;
										for (FTransform& oldObjTrasf : CellGenSlot.TempTransformsForDistanceChecks)
										{
											GenerationHelper::ResizeFByScale3D(oldObjTrasf.GetScale3D(), HgtSz2);
											FVector PointSegA = oldObjTrasf.GetLocation() + ((oldObjTrasf.GetRotation() * FVector(0, 0, 1)) * HgtSz2);
											FVector PointSegA2 = oldObjTrasf.GetLocation() + FVector(0, 0, 1);
											FVector PointSegB = TempTransform.GetLocation() + ((TempTransform.GetRotation() * FVector(0, 0, 1)) * HgtSz);
											FVector PointSegB2 = TempTransform.GetLocation() + FVector(0, 0, 1);

											FMath::SegmentDistToSegmentSafe(PointSegA, PointSegA2, PointSegB, PointSegB2, SegmentAPos, SegmentBPos);
											BoundsOfNewObject.MoveTo(SegmentAPos);
											if (BoundsOfNewObject.IsInsideXY(SegmentBPos))
											{
												if (FVector::Dist(SegmentAPos, SegmentBPos) < ProcGenSlot.DefCollisionSize)
												{
													bNeedContinueBCCF = true;
													break;
												}
											}
										}
									}
									else
									{
										BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));
										for (FTransform& oldObjTrasf : CellGenSlot.TempTransformsForDistanceChecks)
										{
											if (BoundsOfNewObject.IsInsideXY(oldObjTrasf.GetLocation()))
											{
												if (FVector::Dist(oldObjTrasf.GetLocation(), TempTransform.GetLocation()) < ProcGenSlot.DefCollisionSize)
												{
													bNeedContinueBCCF = true;
													break;
												}
											}
										}
									}
								}

								if (bNeedContinueBCCF)
								{
									break;
								}
							}
						}
					}

					if (bNeedContinueBCCF)
					{
						continue;
					}

					//FMath::SegmentDistToSegmentSafe()
				}

				if (ProcGenSlot.bRandomPitch180)
				{
					bool Add180ToPitch = UKismetMathLibrary::RandomBoolWithWeightFromStream(0.5f, ProcGenSlot.CurrentGenerationStream);
					if (Add180ToPitch)
					{
						FRotator AlignRotation = FVector(0, 0, -1).Rotation();
						AlignRotation.Pitch -= 90.f;
						AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
						NormalizedQRot = NormalizedQRot * FQuat(AlignRotation);
						NormalizedQRot.Normalize();
						TempTransform.SetRotation(NormalizedQRot);
					}
				}

				if (ProcGenSlot.bIsGridGenEnabled)
				{
					FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(TempTransform.GetLocation());
					//FGenerationGridCell* pGridCell = pProcGenManager->GetGenerationGridCellInCustomArrByLocation(TempTransform.GetLocation(), ArrGridCells);
					//FGenerationGridCell* pGridCell = pProcGenManager->GetGenerationGridCellByLocation(TempTransform.GetLocation());
					if (pGridCell)
					{
						FGenerationInCellSlotData NewSlotData = FGenerationInCellSlotData();
						FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(ProcGenSlot.SlotUniqueId);
						if (pSelectedSMSetupTG)
						{
							//FGenerationInCellSlotData NewSlotData = FGenerationInCellSlotData();
							NewSlotData.ParentActorPtr = pParentProcGenActor;
							NewSlotData.SlotTransf = TempTransform;
							NewSlotData.CellSlotDataType = EGenCellSlotDataType::SMeshToGen;
							NewSlotData.StaticMeshPtr = pSelectedSMSetupTG->StaticMeshPtr;
							NewSlotData.ParentCellId = pGridCell->CellId;
							CellGenSlot.SlotData.Add(NewSlotData);
							
						}
						else if (pDecalTypeSetup)
						{
							NewSlotData.ParentActorPtr = pParentProcGenActor;
							NewSlotData.SlotTransf = TempTransform;
							NewSlotData.CellSlotDataType = EGenCellSlotDataType::DecalToGen;
							NewSlotData.DecalMaterial = pDecalTypeSetup->DecalMaterial;
							NewSlotData.DecalSize = pDecalTypeSetup->DecalInitialScale;
							NewSlotData.ParentCellId = pGridCell->CellId;
							CellGenSlot.SlotData.Add(NewSlotData);
						}
						else if (pSelectedActorClass)
						{
							NewSlotData.ParentActorPtr = pParentProcGenActor;
							NewSlotData.SlotTransf = TempTransform;
							NewSlotData.CellSlotDataType = EGenCellSlotDataType::ActorToGen;
							NewSlotData.ActorToCreateClassPtr = pSelectedActorClass;
							NewSlotData.ParentCellId = pGridCell->CellId;
							CellGenSlot.SlotData.Add(NewSlotData);
						}
					}
				}

				if (!ProcGenSlot.bDebugIsObjectsCreatingDisabled && !ProcGenSlot.bIsGridGenEnabled)
				{
					if (pSelectedSMSetupTG)
					{
						//AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(OptProcGenActor);
						if (pParentProcGenActor)
						{
							pParentProcGenActor->CreateNewSMComponent(pSelectedSMSetupTG->StaticMeshPtr, TempTransform, &pSelectedSMSetupTG->StaticMeshCollisionSetup, &pSelectedSMSetupTG->StaticMeshRenderingOverrideSetup);
						}
					}

					if (pDecalTypeSetup)
					{
						if (pParentProcGenActor)
						{
							pParentProcGenActor->CreateNewDecalComponent(pDecalTypeSetup->DecalMaterial, TempTransform, pDecalTypeSetup->DecalInitialScale, &pDecalTypeSetup->DecalRenderingOverrideSetup);
						}
					}
				}
				//temp check
				//continue;
				//UKismetSystemLibrary::DrawDebugLine(pGenerationWorld, TempTransform.GetLocation(), FVector(FMath::RandRange(-100.0f, 100.0f), FMath::RandRange(-100.0f, 100.0f), FMath::RandRange(-100.0f, 100.0f)), FLinearColor::Red, 15.3f);

				ProcGenSlot.TempTransformsForObjects.Add(TempTransformWithoutFlip);
				if (ProcGenSlot.bEnableGridBasedDistanceCheckOptimization)
				{
					FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(TempTransform.GetLocation());
					if (pGridCell)
					{
						FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(ProcGenSlot.SlotUniqueId);
						CellGenSlot.TempTransformsForDistanceChecks.Add(TempTransformWithoutFlip);
					}
				}

				if (ProcGenSlot.bDebugIsObjectsCreatingDisabled || ProcGenSlot.bIsGridGenEnabled)
				{
					continue;
				}

				if (!pSelectedActorClass)
					continue;

				if (bDelayedGeneration)
				{
					//UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();//Ftest_extendModule::GetProcGenManager();
					if (pProcGenManager)
					{
						FActorToDelayedCreateParams ActorToDelayedCreateParams = FActorToDelayedCreateParams();
						ActorToDelayedCreateParams.ActorClassPtr = pSelectedActorClass;
						ActorToDelayedCreateParams.ActorTransform = TempTransform;
						ActorToDelayedCreateParams.bSpawnInEditor = bPlaceActorsInEditor;
						ActorToDelayedCreateParams.ParentProcGenSlotObjPtr = this;
						ActorToDelayedCreateParams.ParentProcGenSlotParamsPtr = &ProcGenSlot;
						if (pParentProcGenActor)
						{
							ActorToDelayedCreateParams.LinkedProcGenActorPtr = pParentProcGenActor;
						}

						if (!ProcGenSlot.bDebugIsObjectsCreatingDisabled)
						{
							pProcGenManager->DelayedCreateActorsQueue.Enqueue(ActorToDelayedCreateParams);
						}
					}
					else
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation interrupted! !UProcGenManager, Generation ERROR C8"));
					}
					continue;
				}

				if (ProcGenSlot.bDebugIsObjectsCreatingDisabled)
				{
					continue;
				}

				if (!bPlaceActorsInEditor)
				{
					FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
					SpawnInfo.OverrideLevel = pGenerationWorld->GetCurrentLevel();
					SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
					SpawnInfo.bHideFromSceneOutliner = true;
					SpawnInfo.bCreateActorPackage = true;
#endif
					SpawnInfo.ObjectFlags = RF_Transactional;
					//SpawnInfo.bCreateActorPackage = true;
					//SpawnInfo.ObjectFlags = InObjectFlags;

					AActor* pGeneratedActor = pGenerationWorld->SpawnActor(pSelectedActorClass, &TempTransform, SpawnInfo);
					if (pGeneratedActor)
					{
						pGeneratedActor->SetActorScale3D(TempTransform.GetScale3D());

						ProcGenSlot.GeneratedActorsPtrs.Add(pGeneratedActor);

						pProcGenManager->AddProceduralTagToActor(pGeneratedActor);
						if (pParentProcGenActor)
						{
							pGeneratedActor->OnDestroyed.AddDynamic(pParentProcGenActor, &AProcGenActor::OnLinkedActorDestroyed);
						}
					}
				}
#if WITH_EDITOR
				else if(GEditor)
				{
					AActor* pGeneratedActor = GEditor->AddActor(pGenerationWorld->GetCurrentLevel(), pSelectedActorClass, TempTransform, true);
					if (pGeneratedActor)
					{
						pGeneratedActor->SetActorScale3D(SelectedScale);

						ProcGenSlot.GeneratedActorsPtrs.Add(pGeneratedActor);

						GEditor->SelectActor(pGeneratedActor, 1, 0);
						pGeneratedActor->InvalidateLightingCache();
						pGeneratedActor->PostEditMove(true);
						pGeneratedActor->MarkPackageDirty();

						pProcGenManager->AddProceduralTagToActor(pGeneratedActor);

						if (pParentProcGenActor)
						{
							pGeneratedActor->OnDestroyed.AddDynamic(pParentProcGenActor, &AProcGenActor::OnLinkedActorDestroyed);
						}
						//UKismetSystemLibrary::DrawDebugLine(pGenerationWorld, TempTransform.GetTranslation(), FVector(TempTransform.GetTranslation().X, TempTransform.GetTranslation().Y, 65535.0f), FLinearColor::Red, 15.3f);
					}
				}
#endif
				//------Generate End
			}
		}
		//SlowTask.Destroy();
		//End Generation of slot
		/*
		FString MsgTextStr = FString::Printf(TEXT("Generation of slot ended, slot Id - %i, Genrated objects num - %i, num of generation attempts - %i"), ProcGenSlot.SlotUniqueId, ProcGenSlot.TempTransformsForObjects.Num(), NumGenChecks);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));
		*/

		if (pParentProcGenActor && ProcGenSlot.bIsGridGenEnabled)
		{
			pParentProcGenActor->LinkedGenSlotsUIDs.AddUnique(ProcGenSlot.SlotUniqueId);
		}

		++slotsProcessed;
	}

	STaskMainMsgTextStr2 = FString::Printf(TEXT("Slots generation completed! Processed Slots num - %i"), slotsProcessed);
	GSTMain.CurrentFrameScope = 0.0f;
	GSTMain.CompletedWork = 100.0f;
	GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr2));
	//FPlatformProcess::Sleep(0.550f);
}

void UPGSObj::PrepareToDestroy()
{
	//check this first
	RemoveTempTrasfsFromGrid();

	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
		{
			/*if (bPlaceActorsInEditor)
			{
				/ *for (AActor* pActor : ProcGenSlot.GeneratedActorsPtrs)
				{
					GEditor->SelectNone(false, false, false);
					GEditor->SelectActor(pActor, true, false);
					GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("DELETE"));
				}* /
				
				for (AActor* pActor : ProcGenSlot.GeneratedActorsPtrs)
				{
					pActor->OnDestroyed.Clear();
					pActor->Destroy();
				}
			}
			else
			{
				for (AActor* pActor : ProcGenSlot.GeneratedActorsPtrs)
				{
					pActor->OnDestroyed.Clear();
					pActor->Destroy();
				}
			}*/
			for (int32 i = ProcGenSlot.GeneratedActorsPtrs.Num() - 1; i >= 0; --i)
			{
				AActor* pActor = ProcGenSlot.GeneratedActorsPtrs[i];
				if (pActor->IsValidLowLevelFast(false))
				{
					pActor->OnDestroyed.Clear();
					pActor->Destroy();
				}
			}
			ProcGenSlot.GeneratedActorsPtrs.Empty();
		}
		ProcGenSlot.TempTransformsForObjects.Empty();
	}
}

void UPGSObj::FreezeCreatedObjectsInEditor()
{
	RemoveTempTrasfsFromGrid();

	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
		{
			ProcGenSlot.GeneratedActorsPtrs.Empty();
		}
		ProcGenSlot.TempTransformsForObjects.Empty();
	}
}

FExtendedSlopeCheckFunctionOutParams UPGSObj::DoExtendedSlopeCheck(const FExtendedSlopeCheckFunctionInParams& InParams, const FProcGenSlotParams& PGSParams)
{
	FExtendedSlopeCheckFunctionOutParams OutParams = FExtendedSlopeCheckFunctionOutParams();
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();//Ftest_extendModule::GetProcGenManager();
	if (!pProcGenManager)
		return OutParams;

	UWorld* pGenWorld = pProcGenManager->GetWorldPRFEditor();
	if(!pGenWorld)
		return OutParams;

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(PGSParams.GenerationTraceChannel);

	FVector CheckPoints[4];
	CheckPoints[0] = InParams.StartCheckPoint + FVector(InParams.CheckRadius, InParams.CheckRadius, 0);
	CheckPoints[1] = InParams.StartCheckPoint + FVector(-InParams.CheckRadius, -InParams.CheckRadius, 0);
	CheckPoints[2] = InParams.StartCheckPoint + FVector(-InParams.CheckRadius, InParams.CheckRadius, 0);
	CheckPoints[3] = InParams.StartCheckPoint + FVector(InParams.CheckRadius, -InParams.CheckRadius, 0);

	float SlopeAngles[4] = { 0.0f,0.0f,0.0f,0.0f };

	FVector HitLocations[4] = { FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector };

	FCollisionQueryParams TraceParams(FName(TEXT("CheckTrace")), true);
	TraceParams.bReturnPhysicalMaterial = true;
	if (InParams.ParentProcGenActorPtr && !InParams.ParentProcGenActorPtr->bSelfCollideWhenGeneration)
	{
		TraceParams.AddIgnoredActor(InParams.ParentProcGenActorPtr);
	}

	for (int32 ie = 0; ie < 4; ++ie)
	{
		const FVector& PointStart = CheckPoints[ie];
		FHitResult CheckHit = FHitResult(ForceInit);
		bool bIsHit = false;
		FVector PointEnd = FVector(PointStart.X, PointStart.Y, PointStart.Z + -6553500.0f);
		FVector PointEnd2 = FVector(PointStart.X, PointStart.Y, PointStart.Z + 6553500.0f);

		if (!InParams.MultiLt)
		{
			bIsHit = pGenWorld->LineTraceSingleByChannel(CheckHit, PointStart, PointEnd, CollisionChannel, TraceParams);
			if (!bIsHit && PGSParams.UnderTerrainFixGeneration)
			{
				bIsHit = pGenWorld->LineTraceSingleByChannel(CheckHit, PointStart, PointEnd2, CollisionChannel, TraceParams);
				if (bIsHit)
				{
					//invert hit normal(backhit)
					CheckHit.Normal = -CheckHit.Normal;
				}
			}

			if (bIsHit)
			{
				if (InParams.ParentProcGenActorPtr)
				{
					if (!InParams.ParentProcGenActorPtr->IsPointInsideConnectedShapeLimitActors(CheckHit.Location))
					{
						continue;
					}

					if (InParams.ParentProcGenActorPtr->ShapeLimitActors.Contains(CheckHit.Actor.Get()))
					{
						continue;
					}
				}

				if (pProcGenManager->IsActorAreGenerator(CheckHit.Actor.Get()))
				{
					if (!PGSParams.bAllowGenerationOnOtherGenActors)
					{
						continue;
					}
				}
				if (pProcGenManager->IsActorCreatedByGenerator(CheckHit.Actor.Get()))
				{
					if (!PGSParams.bAllowGenerationOnOtherGenActors)
					{
						continue;
					}
				}

				if (PGSParams.SurfaceActorsWithTagsAllowed.Num() > 0)
				{
					if (CheckHit.Actor.Get())
					{
						bool bHaveTag = false;
						for (const FName& TagCur : CheckHit.Actor.Get()->Tags)
						{
							if (PGSParams.SurfaceActorsWithTagsAllowed.Contains(TagCur))
							{
								bHaveTag = true;
								break;
							}
						}

						if (!bHaveTag)
						{
							continue;
						}
					}
				}
				FVector RtDir = InParams.EndCheckPoint - CheckHit.Location;
				RtDir.Normalize();
				if (RtDir.Z < 0.0f)
				{
					RtDir.Z *= -1.0f;
				}
				//float ZCalc = 
				//float DifferenceOf = InParams.EndCheckPoint.Z - CheckHit.Location.Z;
				//DifferenceOf = DifferenceOf / 25.0f;
				float GroundAngleValue = FMath::RadiansToDegrees(FMath::Acos(RtDir.Z));
				SlopeAngles[ie] = GroundAngleValue;
				HitLocations[ie] = CheckHit.Location;
			}
		}
		else
		{
			FCollisionResponseParams FCRPX = FCollisionResponseParams(ECR_Overlap/*ECR_Block*/);
			TArray<FHitResult> HitsRes = TArray<FHitResult>();
			bIsHit = pGenWorld->LineTraceMultiByChannel(HitsRes, PointStart, PointEnd, CollisionChannel, TraceParams, FCRPX);
			bIsHit = HitsRes.Num() > 0;
			if (!bIsHit && PGSParams.UnderTerrainFixGeneration)
			{
				bIsHit = pGenWorld->LineTraceMultiByChannel(HitsRes, PointStart, PointEnd2, CollisionChannel, TraceParams, FCRPX);
				bIsHit = HitsRes.Num() > 0;
			}
			if (bIsHit)
			{
				bool bRequestHitFailure = false;
				AActor* HitActor = nullptr;
				for (FHitResult& CurHitRes : HitsRes)
				{
					HitActor = CurHitRes.Actor.Get();
					if (HitActor)
					{
						bool bIsThisHitFiltered = false;
						if (pProcGenManager->IsActorAreGenerator(HitActor))
						{
							if (!PGSParams.bAllowGenerationOnOtherGenActors)
							{
								//TODO skip generation
								//bIsHit = false;
								//break;
								bRequestHitFailure = true;
								bIsThisHitFiltered = true;
							}
						}
						if (pProcGenManager->IsActorCreatedByGenerator(HitActor))
						{
							if (!PGSParams.bAllowGenerationOnOtherGenActors)
							{
								//TODO skip generation
								//bIsHit = false;
								//break;
								bRequestHitFailure = true;
								bIsThisHitFiltered = true;
							}
						}
						if (InParams.ParentProcGenActorPtr)
						{
							if (!InParams.ParentProcGenActorPtr->IsPointInsideConnectedShapeLimitActors(CurHitRes.Location))
							{
								bIsThisHitFiltered = true;
								continue;
							}

							if (InParams.ParentProcGenActorPtr->ShapeLimitActors.Contains(CurHitRes.Actor.Get()))
							{
								bIsThisHitFiltered = true;
								continue;
							}
						}
						bool bIsLandscape = HitActor->IsA(ALandscape::StaticClass());
						bool bIsStaticMesh = HitActor->IsA(AStaticMeshActor::StaticClass());
						bool bIsStaticMeshComponent = HitActor->IsA(AStaticMeshActor::StaticClass());
						if (Cast<UStaticMeshComponent>(CurHitRes.GetComponent()))
						{
							bIsStaticMeshComponent = true;
						}
						//prefer generation on actors with selected tag
						if (PGSParams.SurfaceActorsWithTagsAllowed.Num() > 0)
						{
							if (HitActor)
							{
								bool bHaveTag = false;
								for (const FName& TagCur : HitActor->Tags)
								{
									if (PGSParams.SurfaceActorsWithTagsAllowed.Contains(TagCur))
									{
										bHaveTag = true;
										break;
									}
								}

								if (!bHaveTag)
								{
									continue;
								}
								else
								{
									bRequestHitFailure = false;
									bIsThisHitFiltered = false;
									CheckHit = CurHitRes;
									break;
								}
							}
						}

						if (PGSParams.bGenerateOnLandscape && bIsLandscape)
						{
							//prefer generation on landscape
							CheckHit = CurHitRes;
							if (!bIsThisHitFiltered && (PGSParams.bPrefFirstHit || PGSParams.bPrefLandscape))
							{
								bRequestHitFailure = false;
								break;
							}
						}

						if (PGSParams.bGenerateOnStaticMeshes && (bIsStaticMesh || bIsStaticMeshComponent))
						{
							CheckHit = CurHitRes;
							if (!bIsThisHitFiltered && (PGSParams.bPrefFirstHit && !PGSParams.bPrefLandscape))
							{
								bRequestHitFailure = false;
								break;
							}
						}
					}
				}

				if (bRequestHitFailure)
				{
					bIsHit = false;
					HitLocations[ie] = PointStart;
					continue;
				}
				else
				{
					HitLocations[ie] = CheckHit.Location;
				}
			}
		}
	}
	float CalcAngle = 10000.0f;
	for (int32 ie = 0; ie < 4; ++ie)
	{
		int32 ieNext = (ie == 3) ? 0 : ie + 1;
		FVector NormalUC = HitLocations[ie] - HitLocations[ieNext];
		NormalUC.Normalize();
		NormalUC = FRotationMatrix::MakeFromY(NormalUC).Rotator().Vector();
		if (NormalUC.Z < CalcAngle)
		{
			CalcAngle = NormalUC.Z;
		}

		if (ie == 3)
		{
			CalcAngle = FMath::RadiansToDegrees(FMath::Acos(CalcAngle));
		}

		float diff = InParams.EndCheckPoint.Z - HitLocations[ie].Z;
		if (diff < 0.0f)
		{
			diff *= -1.0f;
		}
		if (diff >= InParams.HeightDifferenceToDetectEdges)
		{
			OutParams.EdgeIsDetected = true;
			if (InParams.EndCheckPoint.Z < HitLocations[ie].Z)
			{
				OutParams.IsLowerEdge = true;
			}
		}
	}
	OutParams.RecalculatedSlopeAngle = CalcAngle;
	return OutParams;
}

TArray<FGenerationObjectPointData> UPGSObj::PrepareGenerationPoints(const FGenerationInitialData& InitialData, FProcGenSlotParams& PGSParams)
{
	TArray<FGenerationObjectPointData> GeneratedPoints = TArray<FGenerationObjectPointData>();

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return GeneratedPoints;

	PGSParams.CurrentGenerationStream = UKismetMathLibrary::MakeRandomStream(PGSParams.RandomSeed);

	FBox GenBox = FBox(InitialData.GenerationBBoxPoints);

	FPoly ShapeBorder = InitialData.ShapeBorder;
	TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
	ShapeBorder.Triangulate(nullptr, TrianglesPolyArr);

	TArray<FPoly> ExclusionBorders = InitialData.ExclusionBorders;

	FVector OptGenerationDir = InitialData.OptGenerationDir;
	FVector OptAlignDir = InitialData.OptAlignDir;

	bool bUseOptGenDir = !OptGenerationDir.IsZero();
	bool bUseOptAlignDir = !OptAlignDir.IsZero();

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	if (!pParentProcGenActor)
	{
		return GeneratedPoints;
	}
	else
	{

	}

	FString STaskMainMsgTextStr = "Prepare Generation Points process started";//FString::Printf(TEXT("Prepare Generation Points, slots to generate - %i, prepare additional generation data"), GenerationParamsArr.Num());
	FScopedSlowTask GSTMain(100.0f, FText::FromString(STaskMainMsgTextStr));
	if(InitialData.bShowProgress)
		GSTMain.MakeDialog();

	if (bUseOptGenDir)
	{
		if (!OptGenerationDir.IsNormalized())
			OptGenerationDir.Normalize();
	}

	if (bUseOptAlignDir)
	{
		if (!OptAlignDir.IsNormalized())
			OptAlignDir.Normalize();
	}

	TArray<AProcGenParamsModifierActor*> ArrCollidedGenParamsModifierActors = pProcGenManager->GetAllCollidedGenParamsModifierActors(GenBox);
	TArray<AProcGenParamsModifierActor*> ArrCollidedAndEqualGenParamsModifierActors = TArray<AProcGenParamsModifierActor*>();

	if (PGSParams.ParamsModifierActorsSetup.EnableParamsModifierActorsInfluence)
	{
		for (AProcGenParamsModifierActor* pPGPModifierActor : ArrCollidedGenParamsModifierActors)
		{
			if (pPGPModifierActor->ModifierSlotsMap.Find(PGSParams.SlotUniqueId))
			{
				ArrCollidedAndEqualGenParamsModifierActors.Add(pPGPModifierActor);
			}
		}
	}

	float GenPowerSelected = PGSParams.RandomGenerationPowerMinMax.GetRandomValue(&PGSParams);
	float GenCoofSelected = PGSParams.RandomGenerationCoofMinMax.GetRandomValue(&PGSParams);
	GenPowerSelected *= InitialData.GenerationPower;
	//GenCoofSelected *= InitialData.GenerationPower;
	uint32 NumGenChecks = uint32(GenPowerSelected);
	bool bGenerate = false;
	FVector TempPoint;
	float ModifierInfluencePercents = 0.0f;
	float DistanceFromEdgesInfluencePercents = 0.0f;
	float DistanceFromExcludeShapesInfluencePercents = 0.0f;
	float GenCoofSelectedNv = GenCoofSelected;

	uint32 pointsProcessed = 0;
	uint32 pointsProcessed2 = 0;

	for (uint32 i = 0; i < NumGenChecks; ++i)
	{
		if (pointsProcessed > pointsProcessed2)
		{
			STaskMainMsgTextStr = FString::Printf(TEXT("Left Points to generate - %i"), NumGenChecks - pointsProcessed);
			GSTMain.CurrentFrameScope = 0.0f;
			GSTMain.CompletedWork = (float(pointsProcessed) / float(NumGenChecks)) * 100.0f;
			if (InitialData.bShowProgress)
				GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr));

			pointsProcessed2 += 150;
		}
		++pointsProcessed;

		GenCoofSelected = PGSParams.RandomGenerationCoofMinMax.GetRandomValue(&PGSParams);
		GenCoofSelectedNv = GenCoofSelected;

		AProcGenParamsModifierActor* pSelectedPGPModifierActor = nullptr;

		TempPoint = RandomPointInBoxByGenSlotParams(PGSParams, GenBox);

		if (!LinePolyIntersection(TrianglesPolyArr, TempPoint, TempPoint - FVector(0, 0, 6553500.0f)))
		{
			//no intersection
			continue;
		}

		if (ArrCollidedAndEqualGenParamsModifierActors.Num() > 0)
		{
			for (AProcGenParamsModifierActor* pPGPModifierActor : ArrCollidedAndEqualGenParamsModifierActors)
			{
				if (pPGPModifierActor->IsPointInsideShape2D(TempPoint))
				{
					pSelectedPGPModifierActor = pPGPModifierActor;
					break;
				}
			}
		}

		GenCoofSelectedNv = GenCoofSelected;
		FGenModifierSlotParams* GenModifierSlotParams = nullptr;
		if (pSelectedPGPModifierActor)
		{
			GenModifierSlotParams = pSelectedPGPModifierActor->ModifierSlotsMap.Find(PGSParams.SlotUniqueId);
			if (GenModifierSlotParams)
			{
				ModifierInfluencePercents = 1.0f;
				if (GenModifierSlotParams->bUseLinearModifierPowerFromShapeEdgesToShapeCenter)
				{
					ModifierInfluencePercents = pSelectedPGPModifierActor->GetPercentDistanceToShapeEdgeFromPointToShapeCenter(TempPoint, PGSParams.ParamsModifierActorsSetup.b2DCalculations);
					ModifierInfluencePercents = FMath::Clamp(ModifierInfluencePercents, 0.0f, 1.0f);
					if (PGSParams.ParamsModifierActorsSetup.InvertInfluencePercents)
					{
						ModifierInfluencePercents = 1.0f - ModifierInfluencePercents;
					}

					if (GenModifierSlotParams->ShapeEdgesToCenterPowerGlobalCurve)
					{
						ModifierInfluencePercents = GenModifierSlotParams->ShapeEdgesToCenterPowerGlobalCurve->GetFloatValue(ModifierInfluencePercents);
					}

					ModifierInfluencePercents *= PGSParams.ParamsModifierActorsSetup.InfluenceMinMaxMult.GetRandomValue(&PGSParams);
					ModifierInfluencePercents = FMath::Clamp(ModifierInfluencePercents, PGSParams.ParamsModifierActorsSetup.InfluenceMinMax.MinValue, PGSParams.ParamsModifierActorsSetup.InfluenceMinMax.MaxValue);
				}

				float AdditionalGenCoof = UKismetMathLibrary::RandomFloatInRangeFromStream(GenModifierSlotParams->GenerationChanceMinMaxModifier.MinValue,
					GenModifierSlotParams->GenerationChanceMinMaxModifier.MaxValue, PGSParams.CurrentGenerationStream);

				if (PGSParams.ParamsModifierActorsSetup.EnableLinearParamsModifierActorsInfluence)
				{
					AdditionalGenCoof *= ModifierInfluencePercents;
				}

				if (GenModifierSlotParams->bAdditiveModifier)
				{
					GenCoofSelectedNv += AdditionalGenCoof;
				}
				else
				{
					GenCoofSelectedNv = AdditionalGenCoof;
				}
			}
		}

		if (PGSParams.DistanceFromEdgesGenerationParams.EnableDistantBasedGeneration)
		{
			if (pParentProcGenActor)
			{
				float AdditionalGenCoof = PGSParams.DistanceFromEdgesGenerationParams.DistancePercentsModifyGenCoofMinMax.GetRandomValue(&PGSParams);
				if (!pParentProcGenActor->bGenerationOnSpline)
				{
					DistanceFromEdgesInfluencePercents = pParentProcGenActor->GetPercentDistanceToShapeEdgeFromPointToShapeCenter(TempPoint, true);
				}
				else
				{
					DistanceFromEdgesInfluencePercents = pParentProcGenActor->GetPercentDistanceToSplineNearestCenterFromPoint(TempPoint, true);
				}

				DistanceFromEdgesInfluencePercents = FMath::Clamp(DistanceFromEdgesInfluencePercents, 0.0f, 1.0f);
				if (PGSParams.DistanceFromEdgesGenerationParams.InvertDistancePercents)
				{
					DistanceFromEdgesInfluencePercents = 1.0f - DistanceFromEdgesInfluencePercents;
				}
				DistanceFromEdgesInfluencePercents *= PGSParams.DistanceFromEdgesGenerationParams.DistancePercentsMult;
				DistanceFromEdgesInfluencePercents = FMath::Clamp(DistanceFromEdgesInfluencePercents, PGSParams.DistanceFromEdgesGenerationParams.DistancePercentsMinMaxToGeneration.MinValue,
					PGSParams.DistanceFromEdgesGenerationParams.DistancePercentsMinMaxToGeneration.MaxValue);

				if (PGSParams.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenCoofCurve)
				{
					AdditionalGenCoof *= PGSParams.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenCoofCurve->GetFloatValue(DistanceFromEdgesInfluencePercents);
				}
				else
				{
					AdditionalGenCoof *= DistanceFromEdgesInfluencePercents;
				}

				GenCoofSelectedNv += AdditionalGenCoof;
			}
		}

		if (PGSParams.DistanceFromExcludeShapesGenerationParams.EnableExcludeShapesDistanceBasedGeneration)
		{
			if (pParentProcGenActor)
			{
				DistanceFromExcludeShapesInfluencePercents = pParentProcGenActor->GetDistanceToExcludedShapes(TempPoint) / PGSParams.DistanceFromExcludeShapesGenerationParams.MaxExcludeShapesDistance;
				DistanceFromExcludeShapesInfluencePercents = FMath::Clamp(DistanceFromExcludeShapesInfluencePercents, 0.0f, 1.0f);
				if (PGSParams.DistanceFromExcludeShapesGenerationParams.InvertDistancePercents)
				{
					DistanceFromExcludeShapesInfluencePercents = 1.0f - DistanceFromExcludeShapesInfluencePercents;
				}
				DistanceFromExcludeShapesInfluencePercents *= PGSParams.DistanceFromExcludeShapesGenerationParams.DistancePercentsMult;
				DistanceFromExcludeShapesInfluencePercents = FMath::Clamp(DistanceFromExcludeShapesInfluencePercents, PGSParams.DistanceFromExcludeShapesGenerationParams.DistancePercentsMinMaxToGeneration.MinValue,
					PGSParams.DistanceFromExcludeShapesGenerationParams.DistancePercentsMinMaxToGeneration.MaxValue);

				float AdditionalGenCoof = PGSParams.DistanceFromExcludeShapesGenerationParams.DistancePercentsModifyGenCoofMinMax.GetRandomValue(&PGSParams);
				if (PGSParams.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenCoofCurve)
				{
					AdditionalGenCoof *= PGSParams.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenCoofCurve->GetFloatValue(DistanceFromExcludeShapesInfluencePercents);
				}
				else
				{
					AdditionalGenCoof *= DistanceFromExcludeShapesInfluencePercents;
				}
				GenCoofSelectedNv += AdditionalGenCoof;
			}
		}

		bGenerate = RandomBoolByGenSlotParams(PGSParams, GenCoofSelectedNv);
		if (bGenerate)
		{
			//process excluded shapes

			if (LinePolyIntersection(ExclusionBorders, TempPoint, TempPoint - FVector(0, 0, 6553500.0f)))
			{
				continue;
			}
			FGenerationObjectPointData ObjectPointData = FGenerationObjectPointData();
			ObjectPointData.PointLocation = TempPoint;
			ObjectPointData.DistanceFromEdgesInfluencePercents = DistanceFromEdgesInfluencePercents;
			ObjectPointData.DistanceFromExcludeShapesInfluencePercents = DistanceFromExcludeShapesInfluencePercents;
			ObjectPointData.ModifierInfluencePercents = ModifierInfluencePercents;
			if (pSelectedPGPModifierActor)
			{
				ObjectPointData.SelectedPGPModifierActorsPtrs.Add(pSelectedPGPModifierActor);
			}
			ObjectPointData.PointLocationSource = TempPoint;
			GeneratedPoints.Add(ObjectPointData);
		}
	}

	return GeneratedPoints;
}

TArray<FGenerationHandledPointData> UPGSObj::HandlingProcessOfGenerationPoints(const FGenerationInitialData& InitialData, FProcGenSlotParams& PGSParams, const TArray<FGenerationObjectPointData>& PointsData)
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return TArray<FGenerationHandledPointData>();

	FVector OptGenerationDir = InitialData.OptGenerationDir;
	FVector OptAlignDir = InitialData.OptAlignDir;
	float OptGenDirTraceMaxDist = InitialData.OptGenDirTraceMaxDist;
	bool bOptGenDirNoOutOfBounds = InitialData.bOptGenDirNoOutOfBounds;

	bool bUseOptGenDir = !OptGenerationDir.IsZero();
	bool bUseOptAlignDir = !OptAlignDir.IsZero();

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	if (!pParentProcGenActor)
	{
		return TArray<FGenerationHandledPointData>();
	}
	else
	{

	}

	FString STaskMainMsgTextStr = "Handling Process Of Generation Points started";
	FScopedSlowTask GSTMain(100.0f, FText::FromString(STaskMainMsgTextStr));
	if (InitialData.bShowProgress)
		GSTMain.MakeDialog();

	FPoly ShapeBorder = InitialData.ShapeBorder;
	TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
	ShapeBorder.Triangulate(nullptr, TrianglesPolyArr);

	UWorld* pGenerationWorld = InitialData.pGenerationWorld;
	if(!pGenerationWorld)
		return TArray<FGenerationHandledPointData>();

	if (bUseOptGenDir)
	{
		if (!OptGenerationDir.IsNormalized())
			OptGenerationDir.Normalize();
	}

	if (bUseOptAlignDir)
	{
		if (!OptAlignDir.IsNormalized())
			OptAlignDir.Normalize();
	}

	TArray<FGenerationHandledPointData> HandledPointsData = TArray<FGenerationHandledPointData>();
	FCollisionQueryParams TraceParams(FName(TEXT("CheckTrace")), true);
	TraceParams.bReturnPhysicalMaterial = true;
	if (pParentProcGenActor && !pParentProcGenActor->bSelfCollideWhenGeneration)
	{
		TraceParams.AddIgnoredActor(pParentProcGenActor);
	}

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(PGSParams.GenerationTraceChannel);

	uint32 pointsProcessed = 0;
	uint32 pointsProcessed2 = 0;

	for (const FGenerationObjectPointData& PointData : PointsData)
	{
		if (pointsProcessed > pointsProcessed2)
		{
			STaskMainMsgTextStr = FString::Printf(TEXT("Left Points to handle - %i"), PointsData.Num() - pointsProcessed);
			GSTMain.CurrentFrameScope = 0.0f;
			GSTMain.CompletedWork = (float(pointsProcessed) / float(PointsData.Num())) * 100.0f;
			if (InitialData.bShowProgress)
				GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr));

			pointsProcessed2 += 150;
		}
		++pointsProcessed;

		const FVector& TempPoint = PointData.PointLocation;
		FGenerationHandledPointData NewHandledData = FGenerationHandledPointData();
		
		FHitResult CheckHit = FHitResult(ForceInit);
		bool bIsHit = false;
		FVector PointEnd = FVector(TempPoint.X, TempPoint.Y, TempPoint.Z - 6553500.0f);
		FVector PointEnd2 = FVector(TempPoint.X, TempPoint.Y, TempPoint.Z + 6553500.0f);
		FVector OriginalHitLoc = TempPoint;
		if (bUseOptGenDir)
		{
			//if (!OptGenerationDir.IsNormalized())
			//	OptGenerationDir.Normalize();

			PointEnd = TempPoint + (OptGenerationDir * ((OptGenDirTraceMaxDist > 0.0f) ? OptGenDirTraceMaxDist : 6553500.0f));
			PointEnd2 = TempPoint - (OptGenerationDir * ((OptGenDirTraceMaxDist > 0.0f) ? OptGenDirTraceMaxDist : 6553500.0f));
		}

		if (!PGSParams.FloatingInAirGeneration)
		{
			if (!PGSParams.bUseMultiLT)
			{
				bIsHit = pGenerationWorld->LineTraceSingleByChannel(CheckHit, TempPoint, PointEnd, CollisionChannel, TraceParams);
				if (!bIsHit && PGSParams.UnderTerrainFixGeneration)
				{
					bIsHit = pGenerationWorld->LineTraceSingleByChannel(CheckHit, TempPoint, PointEnd2, CollisionChannel, TraceParams);
					if (bIsHit)
					{
						//invert hit normal(backhit)
						CheckHit.Normal = -CheckHit.Normal;
					}
				}

				if (bIsHit)
				{
					if (pParentProcGenActor)
					{
						if (!pParentProcGenActor->IsPointInsideConnectedShapeLimitActors(CheckHit.Location))
						{
							continue;
						}

						if (pParentProcGenActor->ShapeLimitActors.Contains(CheckHit.Actor.Get()))
						{
							continue;
						}
					}

					if (pProcGenManager->IsActorAreGenerator(CheckHit.Actor.Get()))
					{
						if (!PGSParams.bAllowGenerationOnOtherGenActors)
						{
							continue;
						}
					}
					if (pProcGenManager->IsActorCreatedByGenerator(CheckHit.Actor.Get()))
					{
						if (!PGSParams.bAllowGenerationOnOtherGenActors)
						{
							continue;
						}
					}

					if (PGSParams.SurfaceActorsWithTagsAllowed.Num() > 0)
					{
						if (CheckHit.Actor.Get())
						{
							bool bHaveTag = false;
							for (const FName& TagCur : CheckHit.Actor.Get()->Tags)
							{
								if (PGSParams.SurfaceActorsWithTagsAllowed.Contains(TagCur))
								{
									bHaveTag = true;
									break;
								}
							}

							if (!bHaveTag)
							{
								continue;
							}
						}
					}
				}
			}
			else
			{
				FCollisionResponseParams FCRPX = FCollisionResponseParams(ECR_Overlap/*ECR_Block*/);
				TArray<FHitResult> HitsRes = TArray<FHitResult>();
				bIsHit = pGenerationWorld->LineTraceMultiByChannel(HitsRes, TempPoint, PointEnd, CollisionChannel, TraceParams, FCRPX);
				bIsHit = HitsRes.Num() > 0;
				if (!bIsHit && PGSParams.UnderTerrainFixGeneration)
				{
					bIsHit = pGenerationWorld->LineTraceMultiByChannel(HitsRes, TempPoint, PointEnd2, CollisionChannel, TraceParams, FCRPX);
					bIsHit = HitsRes.Num() > 0;
				}
				if (bIsHit)
				{
					bool bRequestHitFailure = false;
					AActor* HitActor = nullptr;
					for (FHitResult& CurHitRes : HitsRes)
					{
						HitActor = CurHitRes.Actor.Get();
						if (HitActor)
						{
							bool bIsThisHitFiltered = false;
							if (pProcGenManager->IsActorAreGenerator(HitActor))
							{
								if (!PGSParams.bAllowGenerationOnOtherGenActors)
								{
									//TODO skip generation
									//bIsHit = false;
									//break;
									bRequestHitFailure = true;
									bIsThisHitFiltered = true;
								}
							}
							if (pProcGenManager->IsActorCreatedByGenerator(HitActor))
							{
								if (!PGSParams.bAllowGenerationOnOtherGenActors)
								{
									//TODO skip generation
									//bIsHit = false;
									//break;
									bRequestHitFailure = true;
									bIsThisHitFiltered = true;
								}
							}
							if (pParentProcGenActor)
							{
								if (!pParentProcGenActor->IsPointInsideConnectedShapeLimitActors(CurHitRes.Location))
								{
									bIsThisHitFiltered = true;
									continue;
								}

								if (pParentProcGenActor->ShapeLimitActors.Contains(CurHitRes.Actor.Get()))
								{
									bIsThisHitFiltered = true;
									continue;
								}
							}
							bool bIsLandscape = HitActor->IsA(ALandscape::StaticClass());
							bool bIsStaticMesh = HitActor->IsA(AStaticMeshActor::StaticClass());
							bool bIsStaticMeshComponent = HitActor->IsA(AStaticMeshActor::StaticClass());
							if (Cast<UStaticMeshComponent>(CurHitRes.GetComponent()))
							{
								bIsStaticMeshComponent = true;
							}
							//prefer generation on actors with selected tag
							if (PGSParams.SurfaceActorsWithTagsAllowed.Num() > 0)
							{
								if (HitActor)
								{
									bool bHaveTag = false;
									for (const FName& TagCur : HitActor->Tags)
									{
										if (PGSParams.SurfaceActorsWithTagsAllowed.Contains(TagCur))
										{
											bHaveTag = true;
											break;
										}
									}

									if (!bHaveTag)
									{
										continue;
									}
									else
									{
										bRequestHitFailure = false;
										bIsThisHitFiltered = false;
										CheckHit = CurHitRes;
										break;
									}
								}
							}

							if (PGSParams.bGenerateOnLandscape && bIsLandscape)
							{
								//prefer generation on landscape
								CheckHit = CurHitRes;
								if (!bIsThisHitFiltered && (PGSParams.bPrefFirstHit || PGSParams.bPrefLandscape))
								{
									bRequestHitFailure = false;
									break;
								}
							}

							if (PGSParams.bGenerateOnStaticMeshes && (bIsStaticMesh || bIsStaticMeshComponent))
							{
								CheckHit = CurHitRes;
								if (!bIsThisHitFiltered && (PGSParams.bPrefFirstHit && !PGSParams.bPrefLandscape))
								{
									bRequestHitFailure = false;
									break;
								}
							}
						}
					}

					if (bRequestHitFailure)
					{
						bIsHit = false;
						continue;
					}
				}
			}
		}

		if (!PGSParams.FloatingInAirGeneration && bIsHit && (PGSParams.SurfaceTypesAllowed.Num() > 0))
		{
			uint8 HitedSurfaceType = 255;
			if (CheckHit.PhysMaterial.IsValid())
			{
				EPhysicalSurface HitSurfaceType = UPhysicalMaterial::DetermineSurfaceType(CheckHit.PhysMaterial.Get());
				HitedSurfaceType = uint8(HitSurfaceType);
				if (!PGSParams.SurfaceTypesAllowed.Contains(HitedSurfaceType))
				{
					continue;
				}
			}
		}

		if (!PGSParams.FloatingInAirGeneration && bIsHit && (PGSParams.TerrainLayersAllowed.Num() > 0))
		{
			ULandscapeHeightfieldCollisionComponent* pHitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(CheckHit.GetComponent());
			if (pHitLandscapeCollision)
			{
				ULandscapeComponent* pLComp = Cast<ULandscapeComponent>(pHitLandscapeCollision->RenderComponent.Get());
				if (pLComp)
				{
					float fWeightComplete = 0.0f;
					for (FString& LayersAllowedStr : PGSParams.TerrainLayersAllowed)
					{
						fWeightComplete += pLComp->EditorGetPaintLayerWeightByNameAtLocation(CheckHit.ImpactPoint, *LayersAllowedStr);
					}

					if (fWeightComplete < PGSParams.MinimalTerrainLayerWeightToGenerate)
					{
						continue;
					}
				}
			}
		}

		if (bUseOptGenDir && bOptGenDirNoOutOfBounds)
		{
			if (bIsHit)
			{
				if (!LinePolyIntersection(TrianglesPolyArr, CheckHit.Location + FVector(0, 0, 6553500), CheckHit.Location - FVector(0, 0, 6553500)))
				{
					//no intersection
					continue;
				}
			}
		}

		if (bIsHit)
		{
			NewHandledData.bHitSuccessfull = true;
			NewHandledData.HitActorPtr = CheckHit.GetActor();
			NewHandledData.HitComponentPtr = CheckHit.GetComponent();
			NewHandledData.HitDistance = CheckHit.Distance;
			NewHandledData.HitNormal = CheckHit.Normal;
			NewHandledData.PointData = PointData;
			NewHandledData.PointData.PointLocation = CheckHit.Location;
			if (CheckHit.PhysMaterial.IsValid())
			{
				EPhysicalSurface HitSurfaceType = UPhysicalMaterial::DetermineSurfaceType(CheckHit.PhysMaterial.Get());
				NewHandledData.SurfaceTypeId = uint8(HitSurfaceType);
			}
		}
		else
		{
			NewHandledData.PointData = PointData;
			NewHandledData.bHitSuccessfull = false;
			NewHandledData.PointData.PointLocation = TempPoint;
		}

		HandledPointsData.Add(NewHandledData);
	}
	return HandledPointsData;
}

FGenerationObjectToSpawnData UPGSObj::PrepareObjectToGenerateOnPoint(const FGenerationInitialData& InitialData, FProcGenSlotParams& PGSParams)
{
	FGenerationObjectToSpawnData SpawnData = FGenerationObjectToSpawnData();
	SpawnData.pGenWorld = InitialData.pGenerationWorld;
	UClass* pSelectedActorClass = nullptr;
	FStaticMeshTypeWithChanceTG* pSelectedSMSetupTG = nullptr;
	FDecalTypeWithChanceTG* pDecalTypeSetup = nullptr;
	if (PGSParams.StaticMeshesTypesToGenerateWithChance.Num() > 0)
	{
		int32 icf = 0;
		uint32 WhileCounterRt = 0;
		while (pSelectedSMSetupTG == nullptr)
		{
			for (FStaticMeshTypeWithChanceTG& MeshTypeWC : PGSParams.StaticMeshesTypesToGenerateWithChance)
			{
				if (RandomBoolByGenSlotParams(PGSParams, MeshTypeWC.GenerationChance))
				{
					pSelectedSMSetupTG = &MeshTypeWC;
				}
				++icf;
			}
			icf = 0;
			++WhileCounterRt;
			if (WhileCounterRt >= 1000)
			{
				//safety
				break;
			}
		}

		if (!pSelectedSMSetupTG)
		{
			int32 SelectedId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, PGSParams.StaticMeshesTypesToGenerateWithChance.Num() - 1, PGSParams.CurrentGenerationStream);
			if (SelectedId < 0 || SelectedId >= PGSParams.StaticMeshesTypesToGenerateWithChance.Num())
			{
				SelectedId = 0;
			}
			pSelectedSMSetupTG = &PGSParams.StaticMeshesTypesToGenerateWithChance[SelectedId];
		}
	}
	else
	{
		int32 SelectedActorClassId = 0;
		if (PGSParams.ActorsTypesWithChanceToGenerate.Num() <= 0)
		{
			SelectedActorClassId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, PGSParams.ActorsTypesToGenerate.Num() - 1, PGSParams.CurrentGenerationStream);
			if (SelectedActorClassId < 0 || SelectedActorClassId >= PGSParams.ActorsTypesToGenerate.Num())
			{
				SelectedActorClassId = 0;
			}
		}
		else
		{
			SelectedActorClassId = -1;
			int32 icf = 0;
			uint32 WhileCounterRt = 0;
			while (SelectedActorClassId == -1)
			{
				for (FActorTypeWithChanceTG& ActorTypeWC : PGSParams.ActorsTypesWithChanceToGenerate)
				{
					if (RandomBoolByGenSlotParams(PGSParams, ActorTypeWC.GenerationChance))
					{
						SelectedActorClassId = icf;
						break;
					}
					++icf;
				}
				icf = 0;
				++WhileCounterRt;
				if (WhileCounterRt >= 1000)
				{
					//safety
					break;
				}
			}

			if (SelectedActorClassId == -1)
			{
				SelectedActorClassId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, PGSParams.ActorsTypesWithChanceToGenerate.Num() - 1, PGSParams.CurrentGenerationStream);
			}
		}

		if (PGSParams.ActorsTypesWithChanceToGenerate.Num() <= 0)
		{
			if (PGSParams.ActorsTypesToGenerate.Num() > SelectedActorClassId)
				pSelectedActorClass = PGSParams.ActorsTypesToGenerate[SelectedActorClassId];
		}
		else
		{
			if (SelectedActorClassId >= 0 && SelectedActorClassId < PGSParams.ActorsTypesWithChanceToGenerate.Num())
			{
				pSelectedActorClass = PGSParams.ActorsTypesWithChanceToGenerate[SelectedActorClassId].ActorClass;
			}
		}

		if (pSelectedActorClass == nullptr)
		{
			//continue;
		}
	}

	if (PGSParams.DecalTypesToGenerateWithChance.Num() > 0)
	{
		int32 icf = 0;
		uint32 WhileCounterRt = 0;
		while (pDecalTypeSetup == nullptr)
		{
			for (FDecalTypeWithChanceTG& DecalTypeWC : PGSParams.DecalTypesToGenerateWithChance)
			{
				if (RandomBoolByGenSlotParams(PGSParams, DecalTypeWC.GenerationChance))
				{
					pDecalTypeSetup = &DecalTypeWC;
				}
				++icf;
			}
			icf = 0;
			++WhileCounterRt;
			if (WhileCounterRt >= 1000)
			{
				//safety
				break;
			}
		}

		if (!pDecalTypeSetup)
		{
			int32 SelectedId = UKismetMathLibrary::RandomIntegerInRangeFromStream(0, PGSParams.DecalTypesToGenerateWithChance.Num() - 1, PGSParams.CurrentGenerationStream);
			if (SelectedId < 0 || SelectedId >= PGSParams.DecalTypesToGenerateWithChance.Num())
			{
				SelectedId = 0;
			}
			pDecalTypeSetup = &PGSParams.DecalTypesToGenerateWithChance[SelectedId];
		}
	}

	if (pSelectedActorClass)
	{
		SpawnData.ActorTS.ActorClass = pSelectedActorClass;
	}
	if (pSelectedSMSetupTG)
	{
		SpawnData.MeshTS = *pSelectedSMSetupTG;
	}
	if (pDecalTypeSetup)
	{
		SpawnData.DecalTS = *pDecalTypeSetup;
	}
	SpawnData.ProcGenActorPtr = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	return SpawnData;
}

TArray<FGenerationHandledPointData> UPGSObj::FilterOfHandledPointsByParams(const FGenerationInitialData& InitialData, FProcGenSlotParams& PGSParams, const TArray<FGenerationHandledPointData>& HandledPoints, bool bEnableDistanceChecks)
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return TArray<FGenerationHandledPointData>();

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	if (!pParentProcGenActor)
	{
		return TArray<FGenerationHandledPointData>();
	}

	FString STaskMainMsgTextStr = "Filtering Of Handled Points started";
	FScopedSlowTask GSTMain(100.0f, FText::FromString(STaskMainMsgTextStr));
	if (InitialData.bShowProgress)
		GSTMain.MakeDialog();

	TArray<FGenerationGridCell*> ArrGridCellsNearest = TArray<FGenerationGridCell*>();
	FBox BoundsOfNewObject = FBox(EForceInit::ForceInit);
	TArray<FGenerationHandledPointData> HandledPointsFiltered = HandledPoints;
	int32 NumOfPoints = HandledPointsFiltered.Num();

	uint32 pointsProcessed = 0;
	uint32 pointsProcessed2 = 0;

	for (int32 i = NumOfPoints - 1; i >= 0; --i)
	{
		if (pointsProcessed > pointsProcessed2)
		{
			STaskMainMsgTextStr = FString::Printf(TEXT("Left Points to filter - %i"), NumOfPoints - pointsProcessed);
			GSTMain.CurrentFrameScope = 0.0f;
			GSTMain.CompletedWork = (float(pointsProcessed) / float(NumOfPoints)) * 100.0f;
			if (InitialData.bShowProgress)
				GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr));

			pointsProcessed2 += 150;
		}
		++pointsProcessed;

		FGenerationHandledPointData& CurrentPoint = HandledPointsFiltered[i];

		if(bEnableDistanceChecks)
		{
			float NewMinDistanceCalc = PGSParams.DistanceBetweenMinMax.GetRandomValue(&PGSParams);
			float BoxExt = NewMinDistanceCalc / 2.0f;
			BoundsOfNewObject = FBox::BuildAABB(CurrentPoint.PointData.PointLocation, FVector(BoxExt, BoxExt, BoxExt));
			{
				if (!PGSParams.DistanceBetweenMinMax.IsIgnored())
				{
					if (!PGSParams.bEnableGridBasedDistanceCheckOptimization)
					{
						if (PGSParams.TempTransformsForObjects.Num() > 0)
						{
							bool bNeedContinue = false;
							for (FTransform OldTrasf : PGSParams.TempTransformsForObjects)
							{
								if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
								{
									if (FVector::Dist(OldTrasf.GetLocation(), CurrentPoint.PointData.PointLocation) < NewMinDistanceCalc)
									{
										bNeedContinue = true;
										break;
									}
								}
							}

							if (bNeedContinue)
							{
								HandledPointsFiltered.RemoveAtSwap(i);
								continue;
							}
						}
					}
					else
					{
						bool bNeedContinue = false;
						ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(CurrentPoint.PointData.PointLocation);
						if (ArrGridCellsNearest.Num() > 0)
						{
							for (FGenerationGridCell* pCell : ArrGridCellsNearest)
							{
								if (!pCell)
									continue;

								FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
								if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
								{
									for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
									{
										if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
										{
											if (FVector::Dist(OldTrasf.GetLocation(), CurrentPoint.PointData.PointLocation) < NewMinDistanceCalc)
											{
												bNeedContinue = true;
												break;
											}
										}
									}
								}

								if (bNeedContinue)
								{
									break;
								}
							}
						}

						if (bNeedContinue)
						{
							HandledPointsFiltered.RemoveAtSwap(i);
							continue;
						}
					}
				}
				//
				BoxExt = PGSParams.DefCollisionSize * 2.0f;
				BoundsOfNewObject = FBox::BuildAABB(CurrentPoint.PointData.PointLocation, FVector(BoxExt, BoxExt, BoxExt));

				if (PGSParams.TakenIntoAccSlotsIds.Num() > 0)
				{
					bool bNeedContinue = false;
					TArray<FProcGenSlotParams*> TakenSlots = GetProcGenSlotsByUIds(PGSParams.TakenIntoAccSlotsIds);
					if (TakenSlots.Num() > 0)
					{
						for (FProcGenSlotParams* pSlotParams : TakenSlots)
						{
							if (!pSlotParams->bEnableGridBasedDistanceCheckOptimization)
							{
								if (pSlotParams->TempTransformsForObjects.Num() > 0)
								{
									for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
									{
										if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
										{
											if (FVector::Dist(OldTrasf.GetLocation(), CurrentPoint.PointData.PointLocation) < (pSlotParams->DefCollisionSize + PGSParams.DefCollisionSize))
											{
												bNeedContinue = true;
												break;
											}
										}
									}
								}
							}
							else
							{
								ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(CurrentPoint.PointData.PointLocation);
								if (ArrGridCellsNearest.Num() > 0)
								{
									for (FGenerationGridCell* pCell : ArrGridCellsNearest)
									{
										if (!pCell)
											continue;

										FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(pSlotParams->SlotUniqueId);
										if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
										{
											for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
											{
												if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
												{
													if (FVector::Dist(OldTrasf.GetLocation(), CurrentPoint.PointData.PointLocation) < (pSlotParams->DefCollisionSize + PGSParams.DefCollisionSize))
													{
														bNeedContinue = true;
														break;
													}
												}
											}
										}

										if (bNeedContinue)
										{
											break;
										}
									}
								}
							}

							if (bNeedContinue)
							{
								break;
							}
						}

						if (bNeedContinue)
						{
							HandledPointsFiltered.RemoveAtSwap(i);
							continue;
						}
					}
				}
			}
		}

		if (pParentProcGenActor)
		{
			if (pParentProcGenActor->OthersPGAsUsedAsTIA.Num() > 0)
			{
				float BoxExt = PGSParams.DefCollisionSize * 2.0f;
				BoundsOfNewObject = FBox::BuildAABB(CurrentPoint.PointData.PointLocation, FVector(BoxExt, BoxExt, BoxExt));

				bool bNeedContinue = false;
				for (AProcGenActor* OthersPGA_Ptr : pParentProcGenActor->OthersPGAsUsedAsTIA)
				{
					if (!OthersPGA_Ptr)
						continue;

					if (OthersPGA_Ptr->CurCreatedProcGenSlotObject.Get())
					{
						TArray<FProcGenSlotParams*> TakenSlots = OthersPGA_Ptr->CurCreatedProcGenSlotObject.Get()->GetProcGenSlotsByUIds(PGSParams.TakenIntoAccSlotsIds);
						if (TakenSlots.Num() > 0)
						{
							for (FProcGenSlotParams* pSlotParams : TakenSlots)
							{
								if (!pSlotParams->bEnableGridBasedDistanceCheckOptimization)
								{
									if (pSlotParams->TempTransformsForObjects.Num() > 0)
									{
										for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
										{
											if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
											{
												if (FVector::Dist(OldTrasf.GetLocation(), CurrentPoint.PointData.PointLocation) < (pSlotParams->DefCollisionSize + PGSParams.DefCollisionSize))
												{
													bNeedContinue = true;
													break;
												}
											}
										}
									}
								}
								else
								{
									ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(CurrentPoint.PointData.PointLocation);
									if (ArrGridCellsNearest.Num() > 0)
									{
										for (FGenerationGridCell* pCell : ArrGridCellsNearest)
										{
											if (!pCell)
												continue;

											FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(pSlotParams->SlotUniqueId);
											if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
											{
												for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
												{
													if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
													{
														if (FVector::Dist(OldTrasf.GetLocation(), CurrentPoint.PointData.PointLocation) < (pSlotParams->DefCollisionSize + PGSParams.DefCollisionSize))
														{
															bNeedContinue = true;
															break;
														}
													}
												}
											}

											if (bNeedContinue)
											{
												break;
											}
										}
									}
								}

								if (bNeedContinue)
								{
									break;
								}
							}

							if (bNeedContinue)
							{
								break;
							}
						}
					}
				}

				if (bNeedContinue)
				{
					HandledPointsFiltered.RemoveAtSwap(i);
					continue;
				}
			}
		}

		if (!PGSParams.HeightMinMax.IsIgnored())
		{
			float SelectedZ = CurrentPoint.PointData.PointLocation.Z;
			if (SelectedZ > PGSParams.HeightMinMax.MaxValue || SelectedZ < PGSParams.HeightMinMax.MinValue)
			{
				HandledPointsFiltered.RemoveAtSwap(i);
				continue;
			}
		}

		if (CurrentPoint.bHitSuccessfull)
		{
			FVector OriginalHitLoc = CurrentPoint.PointData.PointLocation;
			if (CurrentPoint.HitActorPtr)
			{
				bool bIsLandscape = CurrentPoint.HitActorPtr->IsA(ALandscape::StaticClass());
				bool bIsStaticMesh = CurrentPoint.HitActorPtr->IsA(AStaticMeshActor::StaticClass());

				if (!PGSParams.bGenerateOnLandscape && bIsLandscape)
				{
					HandledPointsFiltered.RemoveAtSwap(i);
					continue;
				}

				if (!PGSParams.bGenerateOnStaticMeshes && bIsStaticMesh)
				{
					HandledPointsFiltered.RemoveAtSwap(i);
					continue;
				}
			}

			float GroundAngleValue = FMath::RadiansToDegrees(FMath::Acos(CurrentPoint.HitNormal.Z));
			if (PGSParams.ExtendedSlopeCheckSetupParams.EnableExtendedSlopeCheck)
			{
				FExtendedSlopeCheckFunctionInParams ExtendedSlopeCheckFunctionInParams = FExtendedSlopeCheckFunctionInParams();
				ExtendedSlopeCheckFunctionInParams.CheckRadius = PGSParams.ExtendedSlopeCheckSetupParams.SlopeCheckRadius;
				ExtendedSlopeCheckFunctionInParams.CurrentSlopeAngle = GroundAngleValue;
				ExtendedSlopeCheckFunctionInParams.ParentProcGenActorPtr = pParentProcGenActor;
				ExtendedSlopeCheckFunctionInParams.MultiLt = PGSParams.bUseMultiLT;
				ExtendedSlopeCheckFunctionInParams.StartCheckPoint = OriginalHitLoc + FVector(0,0,655360);
				ExtendedSlopeCheckFunctionInParams.EndCheckPoint = OriginalHitLoc;
				ExtendedSlopeCheckFunctionInParams.HeightDifferenceToDetectEdges = PGSParams.ExtendedSlopeCheckSetupParams.HeightDifferenceToDetectEdges;
				FExtendedSlopeCheckFunctionOutParams ExtendedSlopeCheckFunctionOutParams = DoExtendedSlopeCheck(ExtendedSlopeCheckFunctionInParams, PGSParams);
				if (PGSParams.ExtendedSlopeCheckSetupParams.bUseRecalculatedSlope)
				{
					GroundAngleValue = ExtendedSlopeCheckFunctionOutParams.RecalculatedSlopeAngle;
				}

				if (PGSParams.ExtendedSlopeCheckSetupParams.bGenerationOnEdgesOnly && !ExtendedSlopeCheckFunctionOutParams.EdgeIsDetected)
				{
					HandledPointsFiltered.RemoveAtSwap(i);
					continue;
				}

				if (PGSParams.ExtendedSlopeCheckSetupParams.bGenerationOnNotEdgesOnly && ExtendedSlopeCheckFunctionOutParams.EdgeIsDetected)
				{
					HandledPointsFiltered.RemoveAtSwap(i);
					continue;
				}
			}
			if (!PGSParams.SlopeMinMax.IsIgnored())
			{
				if (GroundAngleValue > PGSParams.SlopeMinMax.MaxValue || GroundAngleValue < PGSParams.SlopeMinMax.MinValue)
				{
					bool bNeedContinue = true;
					if (GroundAngleValue > PGSParams.SlopeMinMax.MaxValue)
					{
						bool bGenOverslope = RandomBoolByGenSlotParams(PGSParams, PGSParams.SlopeOverMaxGenChance);
						if (bGenOverslope)
						{
							if (GroundAngleValue <= PGSParams.SlopeMaxVariation.MaxValue)
							{
								bNeedContinue = false;
							}
						}
					}
					//TODO do same check for "GroundAngleValue < PGSParams.SlopeMinMax.MinValue"

					if (bNeedContinue)
					{
						HandledPointsFiltered.RemoveAtSwap(i);
						continue;
					}
				}
			}
			CurrentPoint.GroundAngle = GroundAngleValue;
		}
	}
	return HandledPointsFiltered;
}

TArray<FTransform> UPGSObj::GenerateObjectsTransformsFromHandledPoints(const FGenerationInitialData& InitialData, FProcGenSlotParams& PGSParams, const TArray<FGenerationHandledPointData>& HandledPoints)
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return TArray<FTransform>();

	FVector OptGenerationDir = InitialData.OptGenerationDir;
	FVector OptAlignDir = InitialData.OptAlignDir;

	bool bUseOptGenDir = !OptGenerationDir.IsZero();
	bool bUseOptAlignDir = !OptAlignDir.IsZero();

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	if (!pParentProcGenActor)
	{
		return TArray<FTransform>();
	}
	else
	{

	}

	UWorld* pGenerationWorld = InitialData.pGenerationWorld;
	if (!pGenerationWorld)
		return TArray<FTransform>();

	if (bUseOptGenDir)
	{
		if (!OptGenerationDir.IsNormalized())
			OptGenerationDir.Normalize();
	}

	if (bUseOptAlignDir)
	{
		if (!OptAlignDir.IsNormalized())
			OptAlignDir.Normalize();
	}

	float OptAlignYaw = InitialData.OptAlignYaw;

	float ModifierInfluencePercents = 0.0f;
	float DistanceFromEdgesInfluencePercents = 0.0f;
	float DistanceFromExcludeShapesInfluencePercents = 0.0f;

	FString STaskMainMsgTextStr = "Generate Objects Transforms From Handled Points started";
	FScopedSlowTask GSTMain(100.0f, FText::FromString(STaskMainMsgTextStr));
	if (InitialData.bShowProgress)
		GSTMain.MakeDialog();

	uint32 pointsProcessed = 0;
	uint32 pointsProcessed2 = 0;

	TArray<FTransform> GenTrasfsArr = TArray<FTransform>();
	for (const FGenerationHandledPointData& CurHdlPoint : HandledPoints)
	{
		if (pointsProcessed > pointsProcessed2)
		{
			STaskMainMsgTextStr = FString::Printf(TEXT("Left Transforms to generate - %i"), HandledPoints.Num() - pointsProcessed);
			GSTMain.CurrentFrameScope = 0.0f;
			GSTMain.CompletedWork = (float(pointsProcessed) / float(HandledPoints.Num())) * 100.0f;
			if (InitialData.bShowProgress)
				GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr));

			pointsProcessed2 += 150;
		}
		++pointsProcessed;

		AProcGenParamsModifierActor* pSelectedPGPModifierActor = nullptr;

		if (CurHdlPoint.PointData.SelectedPGPModifierActorsPtrs.Num() > 0)
		{
			pSelectedPGPModifierActor = CurHdlPoint.PointData.SelectedPGPModifierActorsPtrs[0];
		}

		FGenModifierSlotParams* GenModifierSlotParams = nullptr;
		if (pSelectedPGPModifierActor)
		{
			GenModifierSlotParams = pSelectedPGPModifierActor->ModifierSlotsMap.Find(PGSParams.SlotUniqueId);
		}

		ModifierInfluencePercents = CurHdlPoint.PointData.ModifierInfluencePercents;
		DistanceFromEdgesInfluencePercents = CurHdlPoint.PointData.DistanceFromEdgesInfluencePercents;
		DistanceFromExcludeShapesInfluencePercents = CurHdlPoint.PointData.DistanceFromExcludeShapesInfluencePercents;

		FTransform TempTransform = FTransform::Identity;
		FRotator InitialRotation = FRotator::ZeroRotator;
		FVector PointLoc = CurHdlPoint.PointData.PointLocation;

		FVector SelectedScale = FVector(1);
		if (!PGSParams.ScaleMinMax.IsIgnored())
		{
			float SelectedSize = PGSParams.ScaleMinMax.GetRandomValue(&PGSParams);
			SelectedScale *= SelectedSize;
		}

		if (!PGSParams.RandomNUScaleXMinMax.IsIgnored())
		{
			SelectedScale.X += PGSParams.RandomNUScaleXMinMax.GetRandomValue(&PGSParams);
		}

		if (!PGSParams.RandomNUScaleYMinMax.IsIgnored())
		{
			SelectedScale.Y += PGSParams.RandomNUScaleYMinMax.GetRandomValue(&PGSParams);
		}

		if (!PGSParams.RandomNUScaleZMinMax.IsIgnored())
		{
			SelectedScale.Z += PGSParams.RandomNUScaleZMinMax.GetRandomValue(&PGSParams);
		}

		if (GenModifierSlotParams)
		{
			float AdditionalScaleCoof = UKismetMathLibrary::RandomFloatInRangeFromStream(GenModifierSlotParams->ScaleMinMaxModifier.MinValue,
				GenModifierSlotParams->ScaleMinMaxModifier.MaxValue, PGSParams.CurrentGenerationStream);

			if (PGSParams.ParamsModifierActorsSetup.EnableLinearParamsModifierActorsInfluence)
			{
				AdditionalScaleCoof *= ModifierInfluencePercents;
			}

			if (GenModifierSlotParams->bAdditiveModifier)
			{
				SelectedScale *= (1.0f + AdditionalScaleCoof);
			}
			else
			{
				SelectedScale *= AdditionalScaleCoof;
			}
		}

		if (PGSParams.DistanceFromEdgesGenerationParams.EnableDistantBasedGeneration)
		{
			if (pParentProcGenActor)
			{
				float AdditionalScaleCoof = PGSParams.DistanceFromEdgesGenerationParams.DistancePercentsModifyScaleMinMax.GetRandomValue(&PGSParams);

				if (PGSParams.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenScaleCurve)
				{
					AdditionalScaleCoof *= PGSParams.DistanceFromEdgesGenerationParams.ShapeEdgesToCenterPowerGenScaleCurve->GetFloatValue(DistanceFromEdgesInfluencePercents);
				}
				else
				{
					AdditionalScaleCoof *= DistanceFromEdgesInfluencePercents;
				}

				SelectedScale *= (1.0f + AdditionalScaleCoof);
			}
		}

		if (PGSParams.DistanceFromExcludeShapesGenerationParams.EnableExcludeShapesDistanceBasedGeneration)
		{
			if (pParentProcGenActor)
			{
				float AdditionalScaleCoof = PGSParams.DistanceFromExcludeShapesGenerationParams.DistancePercentsModifyScaleMinMax.GetRandomValue(&PGSParams);

				if (PGSParams.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenScaleCurve)
				{
					AdditionalScaleCoof *= PGSParams.DistanceFromExcludeShapesGenerationParams.ExcludeShapesDistancePowerGenScaleCurve->GetFloatValue(DistanceFromExcludeShapesInfluencePercents);
				}
				else
				{
					AdditionalScaleCoof *= DistanceFromExcludeShapesInfluencePercents;
				}

				SelectedScale *= (1.0f + AdditionalScaleCoof);
			}
		}

		if (CurHdlPoint.bHitSuccessfull)
		{
			if (PGSParams.AddZPosFromGroundNormal != 0.0f)
			{
				float GroundNormalZLgnScaled = PGSParams.AddZPosFromGroundNormal;
				if (PGSParams.bScaleAllParamsByInstScale)
				{
					GenerationHelper::ResizeFByScale3D(SelectedScale, GroundNormalZLgnScaled);
				}
				PointLoc += (CurHdlPoint.HitNormal * GroundNormalZLgnScaled);
			}

			if (!PGSParams.AddZPosFGNMinMax.IsIgnored())
			{
				float ZPosFGNScaled = PGSParams.AddZPosFGNMinMax.GetRandomValue(&PGSParams);
				if (PGSParams.bScaleAllParamsByInstScale)
				{
					GenerationHelper::ResizeFByScale3D(SelectedScale, ZPosFGNScaled);
				}
				PointLoc += (CurHdlPoint.HitNormal * ZPosFGNScaled);
			}

			if (!PGSParams.PressInGroundMinMax.IsIgnored())
			{
				float ZPressInGroundScaled = PGSParams.PressInGroundMinMax.GetRandomValue(&PGSParams);
				if (PGSParams.bScaleAllParamsByInstScale)
				{
					GenerationHelper::ResizeFByScale3D(SelectedScale, ZPressInGroundScaled);
				}
				PointLoc.Z -= ZPressInGroundScaled;
			}

			if (PGSParams.PressInGroundBySlopeForce != 0.0f)
			{
				float SlopePerc = CurHdlPoint.GroundAngle / 90.0f;
				SlopePerc = FMath::Clamp(SlopePerc, 0.0f, 1.0f);
				float ZPressInGroundSFScaled = PGSParams.PressInGroundBySlopeForce;
				if (PGSParams.bScaleAllParamsByInstScale)
				{
					GenerationHelper::ResizeFByScale3D(SelectedScale, ZPressInGroundSFScaled);
				}
				PointLoc.Z -= ZPressInGroundSFScaled * SlopePerc;
			}

			if (PGSParams.bPressInGroundByNormalZ)
			{
				float SlopePercent = CurHdlPoint.GroundAngle / 90.0f;
				if (!PGSParams.PressInGroundNZMinMax.IsIgnored())
				{
					float ZPressInGroundNZScaled = PGSParams.PressInGroundNZMinMax.GetRandomValue(&PGSParams);
					if (PGSParams.bScaleAllParamsByInstScale)
					{
						GenerationHelper::ResizeFByScale3D(SelectedScale, ZPressInGroundNZScaled);
					}
					PointLoc.Z -= (ZPressInGroundNZScaled * SlopePercent);
				}
			}

			TempTransform.SetLocation(PointLoc + PGSParams.AdditionalLocationVector);

			if (PGSParams.RotateToGroundNormal)
			{
				FRotator AlignRotation = CurHdlPoint.HitNormal.Rotation();
				// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
				AlignRotation.Pitch -= 90.f;
				// Clamp its value inside +/- one rotation
				AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
				//TempTransform.SetRotation(AlignRotation);
				InitialRotation = AlignRotation;
			}

			if (bUseOptAlignDir)
			{
				//if (!OptAlignDir.IsNormalized())
				//	OptAlignDir.Normalize();

				FRotator AlignRotation = OptAlignDir.Rotation();
				// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
				AlignRotation.Pitch -= 90.f;
				// Clamp its value inside +/- one rotation
				AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
				//TempTransform.SetRotation(AlignRotation);
				InitialRotation = AlignRotation;

				if (OptAlignYaw != 0.0f)
				{
					InitialRotation.Yaw = 0.0f;
				}
			}
		}
		else
		{
			if (PGSParams.FloatingInAirGeneration)
			{
				TempTransform.SetLocation(CurHdlPoint.PointData.PointLocationSource + PGSParams.AdditionalLocationVector);
			}
			else
			{
				continue;
			}
		}

		FRotator AdditionalRotation = FRotator::ZeroRotator;
		if (!PGSParams.RandomRotationRollMinMax.IsIgnored())
		{
			AdditionalRotation.Roll += PGSParams.RandomRotationRollMinMax.GetRandomValue(&PGSParams);
		}
		if (!PGSParams.RandomRotationPitchMinMax.IsIgnored())
		{
			AdditionalRotation.Pitch += PGSParams.RandomRotationPitchMinMax.GetRandomValue(&PGSParams);
		}
		if (!PGSParams.RandomRotationYawMinMax.IsIgnored())
		{
			AdditionalRotation.Yaw += PGSParams.RandomRotationYawMinMax.GetRandomValue(&PGSParams);
		}

		if (OptAlignYaw != 0.0f)
		{
			AdditionalRotation.Yaw += OptAlignYaw;
			if (AdditionalRotation.Yaw > 360.0f)
			{
				AdditionalRotation.Yaw -= 360.0f;
			}
			else if (AdditionalRotation.Yaw < -360.0f)
			{
				AdditionalRotation.Yaw += 360.0f;
			}
		}

		AdditionalRotation.Normalize();

		TempTransform.SetRotation((FQuat(InitialRotation) * FQuat(AdditionalRotation)) * FQuat(PGSParams.AdditionalRotation));
		TempTransform.SetScale3D(SelectedScale);

		GenTrasfsArr.Add(TempTransform);
	}
	return GenTrasfsArr;
}

TArray<FTransform> UPGSObj::FilterOfGeneratedTransforms(const FGenerationInitialData& InitialData, FProcGenSlotParams& PGSParams, const TArray<FTransform>& TransformsToFilter)
{
	UWorld* pGenerationWorld = InitialData.pGenerationWorld;
	if (!pGenerationWorld)
	{
		return TArray<FTransform>();
	}
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return TArray<FTransform>();

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	if (!pParentProcGenActor)
	{
		return TArray<FTransform>();
	}

	TArray<FTransform> OutTransforms = TransformsToFilter;
	TArray<FGenerationGridCell*> ArrGridCellsNearest = TArray<FGenerationGridCell*>();
	//for(const FTransform& CurTransf : TransformsToFilter)
	FBox BoundsOfNewObject = FBox(EForceInit::ForceInit);
	int32 numOfTrasfs = OutTransforms.Num();

	FString STaskMainMsgTextStr = "Filtering Of Generated Transforms started";
	FScopedSlowTask GSTMain(100.0f, FText::FromString(STaskMainMsgTextStr));
	if (InitialData.bShowProgress)
		GSTMain.MakeDialog();

	uint32 pointsProcessed = 0;
	uint32 pointsProcessed2 = 0;

	for (int32 i = numOfTrasfs - 1; i >= 0; --i)
	{
		if (pointsProcessed > pointsProcessed2)
		{
			STaskMainMsgTextStr = FString::Printf(TEXT("Left Transforms to filter - %i"), numOfTrasfs - pointsProcessed);
			GSTMain.CurrentFrameScope = 0.0f;
			GSTMain.CompletedWork = (float(pointsProcessed) / float(numOfTrasfs)) * 100.0f;
			if (InitialData.bShowProgress)
				GSTMain.EnterProgressFrame(0.0f, FText::FromString(STaskMainMsgTextStr));

			pointsProcessed2 += 150;
		}
		++pointsProcessed;

		FTransform& TempTransform = OutTransforms[i];

		bool bDistanceApproved = GenerationDistanceCheck(InitialData, PGSParams, TempTransform);
		if (!bDistanceApproved)
		{
			OutTransforms.RemoveAtSwap(i);
			continue;
		}

		if (PGSParams.bCollisionCheckBeforeGenerate)
		{
			float BoxExt = PGSParams.DefCollisionSize / 2.0f;
			BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));

			FTransform TempTransf2 = TempTransform;
			//TempTransf2.SetLocation(OriginalHitLoc);
			if (GenerationHelper::CheckCollision(pGenerationWorld, TempTransf2, PGSParams.DefCollisionSize, PGSParams.DefCollisionHeightSize,
				PGSParams.bCollisionCheckExcludeOnlyAllPGMeshes, PGSParams.bCollisionCheckExcludeOnlyOtherPGMeshes))
			{
				continue;
			}
			bool bNeedContinueBCCF = false;

			if (!PGSParams.bEnableGridBasedDistanceCheckOptimization)
			{
				if (PGSParams.DefCollisionHeightSize > 0.0f)
				{
					float HgtSz = PGSParams.DefCollisionHeightSize;
					float HgtSz2 = PGSParams.DefCollisionHeightSize;
					GenerationHelper::ResizeFByScale3D(TempTransform.GetScale3D(), HgtSz);
					FVector SegmentAPos = FVector::ZeroVector;
					FVector SegmentBPos = FVector::ZeroVector;
					for (FTransform& oldObjTrasf : PGSParams.TempTransformsForObjects)
					{
						GenerationHelper::ResizeFByScale3D(oldObjTrasf.GetScale3D(), HgtSz2);
						FVector PointSegA = oldObjTrasf.GetLocation() + ((oldObjTrasf.GetRotation() * FVector(0, 0, 1)) * HgtSz2);
						FVector PointSegA2 = oldObjTrasf.GetLocation() + FVector(0, 0, 1);
						FVector PointSegB = TempTransform.GetLocation() + ((TempTransform.GetRotation() * FVector(0, 0, 1)) * HgtSz);
						FVector PointSegB2 = TempTransform.GetLocation() + FVector(0, 0, 1);

						FMath::SegmentDistToSegmentSafe(PointSegA, PointSegA2, PointSegB, PointSegB2, SegmentAPos, SegmentBPos);
						BoundsOfNewObject.MoveTo(SegmentAPos);
						if (BoundsOfNewObject.IsInsideXY(SegmentBPos))
						{
							if (FVector::Dist(SegmentAPos, SegmentBPos) < PGSParams.DefCollisionSize)
							{
								bNeedContinueBCCF = true;
								break;
							}
						}
					}
				}
				else
				{
					BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));
					for (FTransform& oldObjTrasf : PGSParams.TempTransformsForObjects)
					{
						if (BoundsOfNewObject.IsInsideXY(oldObjTrasf.GetLocation()))
						{
							if (FVector::Dist(oldObjTrasf.GetLocation(), TempTransform.GetLocation()) < PGSParams.DefCollisionSize)
							{
								bNeedContinueBCCF = true;
								break;
							}
						}
					}
				}
			}
			else
			{
				ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(TempTransform.GetLocation());
				if (ArrGridCellsNearest.Num() > 0)
				{
					for (FGenerationGridCell* pCell : ArrGridCellsNearest)
					{
						if (!pCell)
							continue;

						FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
						//CellGenSlot.TempTransformsForDistanceChecks.Add(TempTransform);
						if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
						{
							if (PGSParams.DefCollisionHeightSize > 0.0f)
							{
								float HgtSz = PGSParams.DefCollisionHeightSize;
								float HgtSz2 = PGSParams.DefCollisionHeightSize;
								GenerationHelper::ResizeFByScale3D(TempTransform.GetScale3D(), HgtSz);
								FVector SegmentAPos = FVector::ZeroVector;
								FVector SegmentBPos = FVector::ZeroVector;
								for (FTransform& oldObjTrasf : CellGenSlot.TempTransformsForDistanceChecks)
								{
									GenerationHelper::ResizeFByScale3D(oldObjTrasf.GetScale3D(), HgtSz2);
									FVector PointSegA = oldObjTrasf.GetLocation() + ((oldObjTrasf.GetRotation() * FVector(0, 0, 1)) * HgtSz2);
									FVector PointSegA2 = oldObjTrasf.GetLocation() + FVector(0, 0, 1);
									FVector PointSegB = TempTransform.GetLocation() + ((TempTransform.GetRotation() * FVector(0, 0, 1)) * HgtSz);
									FVector PointSegB2 = TempTransform.GetLocation() + FVector(0, 0, 1);

									FMath::SegmentDistToSegmentSafe(PointSegA, PointSegA2, PointSegB, PointSegB2, SegmentAPos, SegmentBPos);
									BoundsOfNewObject.MoveTo(SegmentAPos);
									if (BoundsOfNewObject.IsInsideXY(SegmentBPos))
									{
										if (FVector::Dist(SegmentAPos, SegmentBPos) < PGSParams.DefCollisionSize)
										{
											bNeedContinueBCCF = true;
											break;
										}
									}
								}
							}
							else
							{
								BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));
								for (FTransform& oldObjTrasf : CellGenSlot.TempTransformsForDistanceChecks)
								{
									if (BoundsOfNewObject.IsInsideXY(oldObjTrasf.GetLocation()))
									{
										if (FVector::Dist(oldObjTrasf.GetLocation(), TempTransform.GetLocation()) < PGSParams.DefCollisionSize)
										{
											bNeedContinueBCCF = true;
											break;
										}
									}
								}
							}
						}

						if (bNeedContinueBCCF)
						{
							break;
						}
					}
				}
			}

			if (bNeedContinueBCCF)
			{
				OutTransforms.RemoveAtSwap(i);
				continue;
			}
		}

		if (bDistanceApproved)
		{
			PGSParams.TempTransformsForObjects.Add(TempTransform);
			if (PGSParams.bEnableGridBasedDistanceCheckOptimization)
			{
				FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(TempTransform.GetLocation());
				if (pGridCell)
				{
					FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
					CellGenSlot.TempTransformsForDistanceChecks.Add(TempTransform);
				}
			}
		}
	}

	for (const FTransform& TransfCur : PGSParams.TempTransformsForObjects)
	{
		FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(TransfCur.GetLocation());
		if (pGridCell)
		{
			FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
			if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
			{
				CellGenSlot.TempTransformsForDistanceChecks.Empty();
			}
		}
	}
	PGSParams.TempTransformsForObjects.Empty();
	return OutTransforms;
}

bool UPGSObj::GenerateObjectOnLevel(FProcGenSlotParams& PGSParams, const FTransform& TempTransform, FGenerationObjectToSpawnData& SpawnData, FObjectCreationResults& CreationResults)
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return false;

	AProcGenActor* pParentProcGenActor = SpawnData.ProcGenActorPtr;
	if (!pParentProcGenActor)
	{
		return false;
	}

	UWorld* pGenerationWorld = SpawnData.pGenWorld;
	if (!pGenerationWorld)
	{
		return false;
	}

	UClass* pSelectedActorClass = nullptr;
	FStaticMeshTypeWithChanceTG* pSelectedSMSetupTG = nullptr;
	FDecalTypeWithChanceTG* pDecalTypeSetup = nullptr;

	pSelectedActorClass = SpawnData.ActorTS.ActorClass.Get();
	if (SpawnData.MeshTS.StaticMeshPtr)
		pSelectedSMSetupTG = &SpawnData.MeshTS;

	if(SpawnData.DecalTS.DecalMaterial != nullptr)
		pDecalTypeSetup = &SpawnData.DecalTS;

	FTransform TempTransformWithoutFlip = TempTransform;
	FTransform TempTransformNew = TempTransform;

	if (PGSParams.bRandomPitch180)
	{
		bool Add180ToPitch = UKismetMathLibrary::RandomBoolWithWeightFromStream(0.5f, PGSParams.CurrentGenerationStream);
		if (Add180ToPitch)
		{
			FRotator AlignRotation = FVector(0, 0, -1).Rotation();
			AlignRotation.Pitch -= 90.f;
			AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);
			FQuat NormalizedQRot = TempTransform.GetRotation() * FQuat(AlignRotation);
			NormalizedQRot.Normalize();
			TempTransformNew.SetRotation(NormalizedQRot);
		}
	}

	bool bObjectCreated = false;
	if (PGSParams.bIsGridGenEnabled)
	{
		FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(TempTransformNew.GetLocation());
		if (pGridCell)
		{
			FGenerationInCellSlotData NewSlotData = FGenerationInCellSlotData();
			FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
			if (pSelectedSMSetupTG)
			{
				NewSlotData.ParentActorPtr = pParentProcGenActor;
				NewSlotData.SlotTransf = TempTransformNew;
				NewSlotData.CellSlotDataType = EGenCellSlotDataType::SMeshToGen;
				NewSlotData.StaticMeshPtr = pSelectedSMSetupTG->StaticMeshPtr;
				NewSlotData.ParentCellId = pGridCell->CellId;
				CellGenSlot.SlotData.Add(NewSlotData);
				bObjectCreated = true;
			}
			else if (pDecalTypeSetup)
			{
				NewSlotData.ParentActorPtr = pParentProcGenActor;
				NewSlotData.SlotTransf = TempTransformNew;
				NewSlotData.CellSlotDataType = EGenCellSlotDataType::DecalToGen;
				NewSlotData.DecalMaterial = pDecalTypeSetup->DecalMaterial;
				NewSlotData.DecalSize = pDecalTypeSetup->DecalInitialScale;
				NewSlotData.ParentCellId = pGridCell->CellId;
				CellGenSlot.SlotData.Add(NewSlotData);
				bObjectCreated = true;
			}
			else if (pSelectedActorClass)
			{
				NewSlotData.ParentActorPtr = pParentProcGenActor;
				NewSlotData.SlotTransf = TempTransformNew;
				NewSlotData.CellSlotDataType = EGenCellSlotDataType::ActorToGen;
				NewSlotData.ActorToCreateClassPtr = pSelectedActorClass;
				NewSlotData.ParentCellId = pGridCell->CellId;
				CellGenSlot.SlotData.Add(NewSlotData);
				bObjectCreated = true;
			}
		}
	}

	if (!PGSParams.bDebugIsObjectsCreatingDisabled && !PGSParams.bIsGridGenEnabled)
	{
		if (pSelectedSMSetupTG)
		{
			if (pParentProcGenActor)
			{
				CreationResults.CreatedMeshComponentPtr = pParentProcGenActor->CreateNewSMComponent(pSelectedSMSetupTG->StaticMeshPtr, TempTransformNew, &pSelectedSMSetupTG->StaticMeshCollisionSetup, &pSelectedSMSetupTG->StaticMeshRenderingOverrideSetup);
				bObjectCreated = true;
			}
		}

		if (pDecalTypeSetup)
		{
			if (pParentProcGenActor)
			{
				CreationResults.CreatedDecalComponentPtr = pParentProcGenActor->CreateNewDecalComponent(pDecalTypeSetup->DecalMaterial, TempTransformNew, pDecalTypeSetup->DecalInitialScale, &pDecalTypeSetup->DecalRenderingOverrideSetup);
				bObjectCreated = true;
			}
		}
	}
	
	PGSParams.TempTransformsForObjects.Add(TempTransformWithoutFlip);
	if (PGSParams.bEnableGridBasedDistanceCheckOptimization)
	{
		FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(TempTransformNew.GetLocation());
		if (pGridCell)
		{
			FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
			CellGenSlot.TempTransformsForDistanceChecks.Add(TempTransformWithoutFlip);
		}
	}

	if (PGSParams.bDebugIsObjectsCreatingDisabled || PGSParams.bIsGridGenEnabled)
	{
		return bObjectCreated;
	}

	if (!pSelectedActorClass)
		return bObjectCreated;

	if (bDelayedGeneration)
	{
		if (pProcGenManager)
		{
			FActorToDelayedCreateParams ActorToDelayedCreateParams = FActorToDelayedCreateParams();
			ActorToDelayedCreateParams.ActorClassPtr = pSelectedActorClass;
			ActorToDelayedCreateParams.ActorTransform = TempTransformNew;
			ActorToDelayedCreateParams.bSpawnInEditor = bPlaceActorsInEditor;
			ActorToDelayedCreateParams.ParentProcGenSlotObjPtr = this;
			ActorToDelayedCreateParams.ParentProcGenSlotParamsPtr = &PGSParams;
			if (pParentProcGenActor)
			{
				ActorToDelayedCreateParams.LinkedProcGenActorPtr = pParentProcGenActor;
			}

			if (!PGSParams.bDebugIsObjectsCreatingDisabled)
			{
				pProcGenManager->DelayedCreateActorsQueue.Enqueue(ActorToDelayedCreateParams);
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation interrupted! !UProcGenManager, Generation ERROR C8"));
		}
		return bObjectCreated;
	}

	if (PGSParams.bDebugIsObjectsCreatingDisabled)
	{
		return bObjectCreated;
	}

	if (!bPlaceActorsInEditor)
	{
		FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
		SpawnInfo.OverrideLevel = pGenerationWorld->GetCurrentLevel();
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
		SpawnInfo.bHideFromSceneOutliner = true;
		SpawnInfo.bCreateActorPackage = true;
#endif
		SpawnInfo.ObjectFlags = RF_Transactional;
		//SpawnInfo.bCreateActorPackage = true;
		//SpawnInfo.ObjectFlags = InObjectFlags;

		AActor* pGeneratedActor = pGenerationWorld->SpawnActor(pSelectedActorClass, &TempTransformNew, SpawnInfo);
		if (pGeneratedActor)
		{
			CreationResults.CreatedActorPtr = pGeneratedActor;

			pGeneratedActor->SetActorScale3D(TempTransformNew.GetScale3D());

			PGSParams.GeneratedActorsPtrs.Add(pGeneratedActor);

			pProcGenManager->AddProceduralTagToActor(pGeneratedActor);
			if (pParentProcGenActor)
			{
				pGeneratedActor->OnDestroyed.AddDynamic(pParentProcGenActor, &AProcGenActor::OnLinkedActorDestroyed);
			}
			bObjectCreated = true;
		}
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		AActor* pGeneratedActor = GEditor->AddActor(pGenerationWorld->GetCurrentLevel(), pSelectedActorClass, TempTransformNew, true);
		if (pGeneratedActor)
		{
			CreationResults.CreatedActorPtr = pGeneratedActor;

			pGeneratedActor->SetActorScale3D(TempTransformNew.GetScale3D());

			PGSParams.GeneratedActorsPtrs.Add(pGeneratedActor);

			GEditor->SelectActor(pGeneratedActor, 1, 0);
			pGeneratedActor->InvalidateLightingCache();
			pGeneratedActor->PostEditMove(true);
			pGeneratedActor->MarkPackageDirty();

			pProcGenManager->AddProceduralTagToActor(pGeneratedActor);

			if (pParentProcGenActor)
			{
				pGeneratedActor->OnDestroyed.AddDynamic(pParentProcGenActor, &AProcGenActor::OnLinkedActorDestroyed);
			}
			bObjectCreated = true;
		}
	}
#endif
	return bObjectCreated;
}

void UPGSObj::GenerationFinishProcess(UPARAM(ref)FProcGenSlotParams& PGSParams)
{
	FProcGenSlotParams* pParams = GetProcGenSlotByUId(PGSParams.SlotUniqueId);
	if (pParams)
	{
		//fill this
		pParams->TempTransformsForObjects = PGSParams.TempTransformsForObjects;
	}
}

bool UPGSObj::GenerationDistanceCheck(const FGenerationInitialData& InitialData, UPARAM(ref)FProcGenSlotParams& PGSParams, const FTransform& TempTransform)
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return false;

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(InitialData.OptProcGenActor);
	if (!pParentProcGenActor)
	{
		return false;
	}

	TArray<FGenerationGridCell*> ArrGridCellsNearest = TArray<FGenerationGridCell*>();
	FBox BoundsOfNewObject = FBox(EForceInit::ForceInit);

	{
		float NewMinDistanceCalc = PGSParams.DistanceBetweenMinMax.GetRandomValue(&PGSParams);
		float BoxExt = NewMinDistanceCalc / 2.0f;
		BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));
		{
			if (!PGSParams.DistanceBetweenMinMax.IsIgnored())
			{
				if (!PGSParams.bEnableGridBasedDistanceCheckOptimization)
				{
					if (PGSParams.TempTransformsForObjects.Num() > 0)
					{
						bool bNeedContinue = false;
						for (FTransform OldTrasf : PGSParams.TempTransformsForObjects)
						{
							if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
							{
								if (FVector::Dist(OldTrasf.GetLocation(), TempTransform.GetLocation()) < NewMinDistanceCalc)
								{
									bNeedContinue = true;
									break;
								}
							}
						}

						if (bNeedContinue)
						{
							return false;
						}
					}
				}
				else
				{
					bool bNeedContinue = false;
					ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(TempTransform.GetLocation());
					if (ArrGridCellsNearest.Num() > 0)
					{
						for (FGenerationGridCell* pCell : ArrGridCellsNearest)
						{
							if (!pCell)
								continue;

							FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(PGSParams.SlotUniqueId);
							if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
							{
								for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
								{
									if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
									{
										if (FVector::Dist(OldTrasf.GetLocation(), TempTransform.GetLocation()) < NewMinDistanceCalc)
										{
											bNeedContinue = true;
											break;
										}
									}
								}
							}

							if (bNeedContinue)
							{
								break;
							}
						}
					}

					if (bNeedContinue)
					{
						return false;
					}
				}
			}
			//
			BoxExt = PGSParams.DefCollisionSize * 2.0f;
			BoundsOfNewObject = FBox::BuildAABB(TempTransform.GetLocation(), FVector(BoxExt, BoxExt, BoxExt));

			if (PGSParams.TakenIntoAccSlotsIds.Num() > 0)
			{
				bool bNeedContinue = false;
				TArray<FProcGenSlotParams*> TakenSlots = GetProcGenSlotsByUIds(PGSParams.TakenIntoAccSlotsIds);
				if (TakenSlots.Num() > 0)
				{
					for (FProcGenSlotParams* pSlotParams : TakenSlots)
					{
						if (!pSlotParams->bEnableGridBasedDistanceCheckOptimization)
						{
							if (pSlotParams->TempTransformsForObjects.Num() > 0)
							{
								for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
								{
									if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
									{
										if (FVector::Dist(OldTrasf.GetLocation(), TempTransform.GetLocation()) < (pSlotParams->DefCollisionSize + PGSParams.DefCollisionSize))
										{
											bNeedContinue = true;
											break;
										}
									}
								}
							}
						}
						else
						{
							ArrGridCellsNearest = pProcGenManager->GetNearestGridCellsGroupToPoint(TempTransform.GetLocation());
							if (ArrGridCellsNearest.Num() > 0)
							{
								for (FGenerationGridCell* pCell : ArrGridCellsNearest)
								{
									if (!pCell)
										continue;

									FGenerationGridCellGenSlot& CellGenSlot = pCell->CellSlotsInfo.FindOrAdd(pSlotParams->SlotUniqueId);
									if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
									{
										for (FTransform OldTrasf : CellGenSlot.TempTransformsForDistanceChecks)
										{
											if (BoundsOfNewObject.IsInsideXY(OldTrasf.GetLocation()))
											{
												if (FVector::Dist(OldTrasf.GetLocation(), TempTransform.GetLocation()) < (pSlotParams->DefCollisionSize + PGSParams.DefCollisionSize))
												{
													bNeedContinue = true;
													break;
												}
											}
										}
									}

									if (bNeedContinue)
									{
										break;
									}
								}
							}
						}

						if (bNeedContinue)
						{
							break;
						}
					}

					if (bNeedContinue)
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

FProcGenSlotParams* UPGSObj::GetProcGenSlotByUId(int32 Id)
{
	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (ProcGenSlot.SlotUniqueId == Id)
			return &ProcGenSlot;
	}
	return nullptr;
}

TArray<FProcGenSlotParams*> UPGSObj::GetProcGenSlotsByUIds(const TArray<int32>& IdsArr)
{
	TArray<FProcGenSlotParams*> SlotsArr = TArray<FProcGenSlotParams*>();
	if(IdsArr.Num() <= 0)
		return SlotsArr;

	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (IdsArr.Contains(ProcGenSlot.SlotUniqueId))
		{
			SlotsArr.Add(&ProcGenSlot);
		}
	}
	return SlotsArr;
}

FVector UPGSObj::RandomPointInBoxByGenSlotParams(FProcGenSlotParams& SlotParams, FBox& BBox)
{
	return FVector(UKismetMathLibrary::RandomFloatInRangeFromStream(BBox.Min.X, BBox.Max.X, SlotParams.CurrentGenerationStream),
		UKismetMathLibrary::RandomFloatInRangeFromStream(BBox.Min.Y, BBox.Max.Y, SlotParams.CurrentGenerationStream),
		UKismetMathLibrary::RandomFloatInRangeFromStream(BBox.Min.Z, BBox.Max.Z, SlotParams.CurrentGenerationStream));
}

bool UPGSObj::RandomBoolByGenSlotParams(FProcGenSlotParams& SlotParams, float fChance)
{
	return UKismetMathLibrary::RandomFloatInRangeFromStream(0.000001f, 100.0f, SlotParams.CurrentGenerationStream) <= fChance;
}

bool UPGSObj::LinePolyIntersection(TArray<FPoly>& PolyObjs, const FVector& Start, const FVector& End)
{
	bool bIsIntersected = false;
	FVector InfoVecs[2];
	for (FPoly& PolyObj : PolyObjs)
	{
		if (PolyObj.Vertices.Num() >= 3)
		{
			bIsIntersected = FMath::SegmentTriangleIntersection(Start, End, PolyObj.Vertices[0], PolyObj.Vertices[1], PolyObj.Vertices[2], InfoVecs[0], InfoVecs[1]);
			if (bIsIntersected)
			{
				break;
			}
		}
	}
	return bIsIntersected;
}

void UPGSObj::RemoveTempTrasfFromSlots(const FTransform& TrasfToRemove)
{
	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		for (int32 i = 0; i < ProcGenSlot.TempTransformsForObjects.Num(); ++i)
		{
			if (ProcGenSlot.TempTransformsForObjects[i].GetLocation() == TrasfToRemove.GetLocation())
			{
				ProcGenSlot.TempTransformsForObjects.RemoveAtSwap(i);
				break;
			}
		}
		//ProcGenSlot.TempTransformsForObjects.Remove(TrasfToRemove);
	}
}

void UPGSObj::RemoveTempTrasfsFromGrid()
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (!pProcGenManager)
		return;

	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (ProcGenSlot.bEnableGridBasedDistanceCheckOptimization)
		{
			for (const FTransform& Transf : ProcGenSlot.TempTransformsForObjects)
			{
				FGenerationGridCell* pGridCell = pProcGenManager->GetNearestGridCellToPointFast(Transf.GetLocation());
				if (pGridCell)
				{
					FGenerationGridCellGenSlot& CellGenSlot = pGridCell->CellSlotsInfo.FindOrAdd(ProcGenSlot.SlotUniqueId);
					if (CellGenSlot.TempTransformsForDistanceChecks.Num() > 0)
					{
						CellGenSlot.TempTransformsForDistanceChecks.Empty();
					}
				}
			}
		}
	}
}

float FPGSlotMinMaxParams::GetRandomValue(FProcGenSlotParams* pParams)
{
	if(!pParams)
		return 0.0f;

	if(IsIgnored())
		return 0.0f;

	return UKismetMathLibrary::RandomFloatInRangeFromStream(MinValue, MaxValue, pParams->CurrentGenerationStream);
}
