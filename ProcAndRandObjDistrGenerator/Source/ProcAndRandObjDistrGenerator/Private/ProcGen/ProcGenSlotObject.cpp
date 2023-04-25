// Free to copy, edit and use in any work/projects, commercial or free

#include "ProcGen/ProcGenSlotObject.h"
#include "..\..\Public\ProcGen\ProcGenSlotObject.h"
#include "Engine.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Landscape.h"
#include "ProcAndRandObjDistrGenerator.h"
#include "Engine/Polys.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
//#include "..\..\Public\ProcGen\ProcGenSlotObject.h"
#include "..\..\Public\ProcGen\ProcGenManager.h"
#include "ProcGen/ProcGenActor.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

UPGSObj::UPGSObj() : Super(), GenerationParamsArr()
{
	bDelayedGeneration = false;
	bPlaceActorsInEditor = false;
	bIsPreparedToGeneration = false;
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
					UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();//FProcAndRandObjDistrGeneratorModule::GetProcGenManager();
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
				else
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
			}
		}
	}
}

static AActor* pCurOptProcGenActor = nullptr;

void UPGSObj::RequestGenerateInBBoxWithShapeBorder(const TArray<FVector>& GenerationBBoxPoints, /*const */UWorld* pGenerationWorld, float GenerationPower, FPoly& ShapeBorder,
	TArray<FPoly>& ExclusionBorders, AActor* OptProcGenActor, FVector OptGenerationDir, FVector OptAlignDir, float OptGenDirTraceMaxDist, bool bOptGenDirNoOutOfBounds, float OptAlignYaw)
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

	pCurOptProcGenActor = OptProcGenActor;

	struct GenerationHelper
	{
		static void ResizeFByScale3D(const FVector& Scale3DVec, float& SizeToScale)
		{
			float ScaleCoof = (Scale3DVec.X + Scale3DVec.Y + Scale3DVec.Z) / 3.0f;
			SizeToScale *= ScaleCoof;
		}

		static bool CheckCollision(UWorld* pGenWorld, FTransform& ObjectTransf, float ObjectSize, float ObjectHeight, bool bOnlyForGenerated = false, bool bOnlyForOthersGenerated = false)
		{
			if (!pGenWorld)
				return false;

			if(ObjectSize <= 0.0f && ObjectHeight <= 0.0f)
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

				FCollisionResponseParams FCRP = FCollisionResponseParams(/*ECR_Overlap*/ECR_Block);
				TArray<FHitResult> arrHits = TArray<FHitResult>();
				bIsHit = pGenWorld->SweepMultiByChannel(arrHits, ObjectTransf.GetLocation(), ObjectTransf.GetLocation() + FVector(0, 0, 1), FQuat::Identity, CollisionChannel, sphereColl, FCQP, FCRP);
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
							if(!pOwLandscape)
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
				FHitResult CheckHit = FHitResult(ForceInit);
				bool bIsHit = false;
				FVector PointEnd = ObjectTransf.GetLocation() + ((ObjectTransf.GetRotation() * FVector(0,0,1)) * ObjectHeight);
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

	FBox GenBox = FBox(GenerationBBoxPoints);

	TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
	ShapeBorder.Triangulate(nullptr, TrianglesPolyArr);

	AProcGenActor* pParentProcGenActor = Cast<AProcGenActor>(OptProcGenActor);

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

	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (!ProcGenSlot.bIsActive)
			continue;

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
		GenCoofSelected *= GenerationPower;
		uint32 NumGenChecks = uint32(GenPowerSelected);
		bool bGenerate = false;
		FVector TempPoint;
		FTransform TempTransform = FTransform::Identity;
		//FVector* ShapeIntersectPtr = nullptr;
		for (uint32 i = 0; i < NumGenChecks; ++i)
		{
			if (IsPendingKillOrUnreachable())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation inerrupted! Generation ERROR C7"));
				return;
			}

			bGenerate = RandomBoolByGenSlotParams(ProcGenSlot, GenCoofSelected);
			if (bGenerate)
			{
				TempPoint = RandomPointInBoxByGenSlotParams(ProcGenSlot, GenBox);
				//UKismetSystemLibrary::DrawDebugLine(pGenerationWorld, TempPoint + FVector(FMath::RandRange(-100, 100)), FVector(TempPoint.X, TempPoint.Y, -65535.0f), FLinearColor::Blue, 15.3f);
				
				if(!LinePolyIntersection(TrianglesPolyArr, TempPoint/* + FVector(0, 0, 65535)*/, TempPoint - FVector(0, 0, 6553500.0f)))
				{
					//no intersection
					continue;
				}
				else
				{
					//ShapeIntersectPtr = nullptr;
					//process excluded shapes

					bool needContinue = false;

					if (LinePolyIntersection(ExclusionBorders, TempPoint/* + FVector(0, 0, 65535)*/, TempPoint - FVector(0, 0, 6553500.0f)))
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
						}
					}
					else
					{
						TArray<FHitResult> HitsRes = TArray<FHitResult>();
						bIsHit = pGenerationWorld->LineTraceMultiByChannel(HitsRes, TempPoint, PointEnd, CollisionChannel, TraceParams);
						if (!bIsHit && ProcGenSlot.UnderTerrainFixGeneration)
						{
							bIsHit = pGenerationWorld->LineTraceMultiByChannel(HitsRes, TempPoint, PointEnd2, CollisionChannel, TraceParams);
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
					if (!bDelayedGeneration)
					{
						if (!ProcGenSlot.DistanceBetweenMinMax.IsIgnored())
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
											if (FVector::Dist(pActor->GetActorLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
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
						if (!ProcGenSlot.DistanceBetweenMinMax.IsIgnored())
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
											if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
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

				if (pParentProcGenActor)
				{
					if (pParentProcGenActor->OthersPGAsUsedAsTIA.Num() > 0)
					{
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
										if (pSlotParams->TempTransformsForObjects.Num() > 0)
										{
											for (FTransform OldTrasf : pSlotParams->TempTransformsForObjects)
											{
												if (FVector::Dist(OldTrasf.GetLocation(), bIsHit ? CheckHit.Location : TempPoint) < (pSlotParams->DefCollisionSize + ProcGenSlot.DefCollisionSize))
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

					if (ProcGenSlot.PressInGroundBySlopeForce != 0.0f)
					{
						float SlopePerc = GroundAngleValue / 90.0f;
						SlopePerc = FMath::Clamp(SlopePerc, 0.0f, 1.0f);
						CheckHit.Location.Z -= ProcGenSlot.PressInGroundBySlopeForce * SlopePerc;
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

				TempTransform.SetRotation((FQuat(InitialRotation) * FQuat(AdditionalRotation)) * FQuat(ProcGenSlot.AdditionalRotation));
				TempTransform.SetScale3D(SelectedScale);

				if (ProcGenSlot.bCollisionCheckBeforeGenerate)
				{
					FTransform TempTransf2 = TempTransform;
					TempTransf2.SetLocation(OriginalHitLoc);
					if (GenerationHelper::CheckCollision(pGenerationWorld, TempTransf2, ProcGenSlot.DefCollisionSize, ProcGenSlot.DefCollisionHeightSize,
						ProcGenSlot.bCollisionCheckExcludeOnlyAllPGMeshes, ProcGenSlot.bCollisionCheckExcludeOnlyOtherPGMeshes))
					{
						continue;
					}
					bool bNeedContinueBCCF = false;

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

							if (FVector::Dist(SegmentAPos, SegmentBPos) < ProcGenSlot.DefCollisionSize)
							{
								bNeedContinueBCCF = true;
								break;
							}
						}
					}
					else
					{
						for (FTransform& oldObjTrasf : ProcGenSlot.TempTransformsForObjects)
						{
							if (FVector::Dist(oldObjTrasf.GetLocation(), TempTransform.GetLocation()) < ProcGenSlot.DefCollisionSize)
							{
								bNeedContinueBCCF = true;
								break;
							}
						}
					}

					if (bNeedContinueBCCF)
					{
						continue;
					}

					//FMath::SegmentDistToSegmentSafe()
				}

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
						pParentProcGenActor->CreateNewDecalComponent(pDecalTypeSetup->DecalMaterial, TempTransform, pDecalTypeSetup->DecalInitialScale);
					}
				}
				//temp check
				//continue;
				//UKismetSystemLibrary::DrawDebugLine(pGenerationWorld, TempTransform.GetLocation(), FVector(FMath::RandRange(-100.0f, 100.0f), FMath::RandRange(-100.0f, 100.0f), FMath::RandRange(-100.0f, 100.0f)), FLinearColor::Red, 15.3f);

				ProcGenSlot.TempTransformsForObjects.Add(TempTransform);

				if (!pSelectedActorClass)
					continue;

				if (bDelayedGeneration)
				{
					//UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();//FProcAndRandObjDistrGeneratorModule::GetProcGenManager();
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

						pProcGenManager->DelayedCreateActorsQueue.Enqueue(ActorToDelayedCreateParams);
					}
					else
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation interrupted! !UProcGenManager, Generation ERROR C8"));
					}
					continue;
				}

				if (!bPlaceActorsInEditor)
				{
					FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
					SpawnInfo.OverrideLevel = pGenerationWorld->GetCurrentLevel();
					SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnInfo.bHideFromSceneOutliner = true;
					SpawnInfo.bCreateActorPackage = true;
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
				else
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
			}
		}
	}
}

void UPGSObj::PrepareToDestroy()
{
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
	for (FProcGenSlotParams& ProcGenSlot : GenerationParamsArr)
	{
		if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
		{
			ProcGenSlot.GeneratedActorsPtrs.Empty();
		}
		ProcGenSlot.TempTransformsForObjects.Empty();
	}
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

float FPGSlotMinMaxParams::GetRandomValue(FProcGenSlotParams* pParams)
{
	if(!pParams)
		return 0.0f;

	if(IsIgnored())
		return 0.0f;

	return UKismetMathLibrary::RandomFloatInRangeFromStream(MinValue, MaxValue, pParams->CurrentGenerationStream);
}
