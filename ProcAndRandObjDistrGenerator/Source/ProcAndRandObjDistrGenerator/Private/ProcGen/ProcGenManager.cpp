// Free to copy, edit and use in any work/projects, commercial or free

#include "ProcGen/ProcGenManager.h"
#include "..\..\Public\ProcGen\ProcGenManager.h"
#include "Engine.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Landscape.h"
#include "UnrealClient.h"
#include "LevelEditorViewport.h"
#endif
#include "Engine/StaticMeshActor.h"
#include "ProcAndRandObjDistrGenerator.h"
#include "ProcGen/ProcGenSlotObject.h"
#include "ProcGen/ProcGenParamsModifierActor.h"
#include "ProcGen/ProcGenActor.h"
//#include "..\..\Public\ProcGen\ProcGenSlotObject.h"
#include <EngineUtils.h>
#include <Engine/GameViewportClient.h>
#include <GameDelegates.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/CoreDelegates.h>

static TWeakObjectPtr<UProcGenManager> CurProcGenManager = nullptr;

UProcGenManager::UProcGenManager() : Super(), DelayedCreateActorsQueue(), ProcGenActorsPtrs(), SelectedToPaintProcGenActorsPtrs(), ProcGenParamsModifierActorsPtrs(),
ArrGridCells(),
ArrGridCellsCurrentlyActivePtrs(),
ArrGridCellsCurrentlyInactivePtrs(),
CameraViewPosTL(),
CurrentCameraViewPosTSVar()
{

	bPaintMode = false;
	PaintSphereSize = 1000.0f;
	CurrentPaintPos = FVector::ZeroVector;
	CurrentPaintCameraDir = FVector::ZeroVector;

	CurrentCameraViewPos = FVector::ZeroVector;

	CurProcGenPaintMode = EProcGenPaintMode::Default;
	ColPaintInNextFrame = false;

	bUseCameraDirToGenerationInPaintMode = false;
	bUseCameraDirToRotationAlignGenObjectsInPaintMode = false;
	bUseCollisionChecksInPaintRemoveMode = false;
	TimeDelayBCreateNextPGActorsGroup = 0.0f;

	bPIESessionStarted = false;

	int32 posXMin = -300000;
	int32 posYMin = -300000;
	int32 posXMax = 300000;
	int32 posYMax = 300000;
	int32 cellXYSize = 2500;
	int32 cellZSize = 150000;
	int32 numCellsX = (posXMax * 2) / cellXYSize;
	int32 numCellsY = (posYMax * 2) / cellXYSize;
	int32 numCells = numCellsX * numCellsY;
	//int32 numCells = (posXMax * 2) / cellXYSize;
	FGenerationGridCell NewGridCell = FGenerationGridCell();
	FVector CellPos = FVector(0);
	FVector CellExt = FVector(0);

	CellExt.X = cellXYSize / 2;
	CellExt.Y = CellExt.X;
	CellExt.Z = cellZSize / 2;

	CellPos.Z = CellPos.Z - (cellZSize / 4);
	int32 NumCellsG = 0;
	for (int32 i = 0; i < numCellsX; ++i)
	{
		CellPos.X = (cellXYSize / 2) - posXMax;
		posXMax -= cellXYSize;
		posYMax = 300000;
		for (int32 ic = 0; ic < numCellsY; ++ic)
		{
			CellPos.Y = (cellXYSize / 2) - posYMax;
			posYMax -= cellXYSize;
			NewGridCell.CellBounds = FBox::BuildAABB(CellPos, CellExt);
			NewGridCell.CellId = NumCellsG;
			ArrGridCells.Add(NewGridCell);
			++NumCellsG;
		}
	}
#if WITH_EDITOR
	FEditorDelegates::BeginPIE.AddUObject(this, &UProcGenManager::OnPIESessionStarted);
	FEditorDelegates::EndPIE.AddUObject(this, &UProcGenManager::OnPIESessionEnded);
	//FEditorDelegates::BeginPIE.AddUFunction(this, FName(TEXT("OnPIESessionStarted")));
	//FEditorDelegates::EndPIE.AddUFunction(this, FName(TEXT("OnPIESessionEnded")));
#endif
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UProcGenManager::OnLevelLoadStarted);

	pGenerationHelperThread.Reset(new FGenerationHelperThread);
	pGenerationHelperThread->StartWork();
}

UWorld* UProcGenManager::GetWorld() const
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

void UProcGenManager::Tick(float DeltaTime)
{
	UWorld* pCurEdWorld = GetWorldPRFEditor();
	if (GetCurManager() != this)
	{
		//test
		UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, FVector(FMath::RandRange(-100.0f, 100.0f)), FVector(0, 0, 1000), FLinearColor::Green, 0.3f);
		CurProcGenManager = this;
		FString MsgTextStr = FString::Printf(TEXT("UProcGenManager::Tick - GetCurManager() != this, set CurProcGenManager to this object, manager name - %s"), *GetName());
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));
		return;
	}
	bool bActorsCreationAllow = false;
	if (TimeDelayBCreateNextPGActorsGroup > 0.0f)
	{
		TimeDelayBCreateNextPGActorsGroup -= DeltaTime;
		if (TimeDelayBCreateNextPGActorsGroup <= 0.0f)
		{
			TimeDelayBCreateNextPGActorsGroup = 0.0f;
			//bActorsCreationAllow = true;
		}
	}
	else
	{
		bActorsCreationAllow = true;
	}

	if (!DelayedCreateActorsQueue.IsEmpty() && bActorsCreationAllow)
	{
		FActorToDelayedCreateParams ActorToDelayedCreateParams = FActorToDelayedCreateParams();
		uint32 QueueCounter = 0;
		while (!DelayedCreateActorsQueue.IsEmpty())
		{
			DelayedCreateActorsQueue.Dequeue(ActorToDelayedCreateParams);
			CreateActorByParams(ActorToDelayedCreateParams, pCurEdWorld);
			//UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, ActorToDelayedCreateParams.ActorTransform.GetLocation(), ActorToDelayedCreateParams.ActorTransform.GetLocation() + FVector(FMath::RandRange(-1000, 1000)), FLinearColor::Green, 16.3f);
			++QueueCounter;
			if (QueueCounter > 25)
			{
				//25 objects per frame
				break;
			}
		}
		TimeDelayBCreateNextPGActorsGroup += 0.15f;
	}
	FVector ViewLoc = FVector::ZeroVector;
#if WITH_EDITOR
	if (!bPIESessionStarted && GEditor)
	{
		if (bPaintMode && pCurEdWorld && GEditor && GEditor->GetActiveViewport())
		{
			FLevelEditorViewportClient* pLevelEditorVP = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
			if (pLevelEditorVP)
			{
				FViewportCursorLocation VCLoc = pLevelEditorVP->GetCursorWorldLocationFromMousePos();
				ECollisionChannel CollisionChannel = ECC_Visibility;
				FCollisionQueryParams TraceParams(FName(TEXT("CheckTrace")), true);
				CurrentPaintCameraDir = VCLoc.GetDirection();
				FHitResult CheckHit = FHitResult(ForceInit);
				bool bIsHit = false;
				bIsHit = pCurEdWorld->LineTraceSingleByChannel(CheckHit, VCLoc.GetOrigin(), VCLoc.GetDirection() * 66666666.0f, CollisionChannel, TraceParams);
				if (bIsHit)
				{
					UKismetSystemLibrary::DrawDebugSphere(pCurEdWorld, CheckHit.Location, PaintSphereSize, 32, (CurProcGenPaintMode == EProcGenPaintMode::Default) ? FLinearColor::Blue : (CurProcGenPaintMode == EProcGenPaintMode::Paint) ? FLinearColor::Green : FLinearColor::Red, 0.001f, 8.0f);
					CurrentPaintPos = CheckHit.Location;
				}
			}

			if (ColPaintInNextFrame == false)
			{
				CurProcGenPaintMode = EProcGenPaintMode::Default;
			}

			if (ColPaintInNextFrame)
			{
				ColPaintInNextFrame = false;
			}
		}

		if (pCurEdWorld && GEditor && GEditor->GetActiveViewport())
		{
			FLevelEditorViewportClient* pLevelEditorVP = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
			if (pLevelEditorVP)
			{
				ViewLoc = pLevelEditorVP->GetViewLocation();
			}
		}
	}
	else
	{
#endif
		if (pCurEdWorld && pCurEdWorld->GetFirstPlayerController() && pCurEdWorld->GetFirstPlayerController()->PlayerCameraManager)
		{
			//APlayerCameraManager* camManager = pCurEdWorld->GetFirstPlayerController()->PlayerCameraManager;
			ViewLoc = pCurEdWorld->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
		}
#if WITH_EDITOR
	}
#endif

	CurrentCameraViewPosTSVar.SetVar(ViewLoc);

	UpdateActiveAndInactiveGridCells();

	//Ticking "procgen" actors
	for (int32 i = 0; i < ProcGenActorsPtrs.Num(); ++i)
	{
		AProcGenActor* ActProcGen = ProcGenActorsPtrs[i].Get();
		if (!ActProcGen)
			continue;

		ActProcGen->Tick(DeltaTime);
	}
}

TStatId UProcGenManager::GetStatId() const
{
	return TStatId();
}

ETickableTickType UProcGenManager::GetTickableTickType() const
{
	return ETickableTickType::Always;
}

bool UProcGenManager::IsTickable() const
{
	return true;
}

void UProcGenManager::BeginDestroy()
{
#if WITH_EDITOR
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif

	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);

	pGenerationHelperThread.Reset(nullptr);

	Super::BeginDestroy();
}

void UProcGenManager::RequestPaint(bool bPaintOrClearBrush)
{
	UWorld* pCurEdWorld = GetWorldPRFEditor();
	ColPaintInNextFrame = true;
	if (!bPaintOrClearBrush)
	{
		CurProcGenPaintMode = EProcGenPaintMode::Clear;
		TArray<AActor*> ActorsList = TArray<AActor*>();
		ActorsList.Reserve(10000);
		UGameplayStatics::GetAllActorsOfClass(pCurEdWorld, AActor::StaticClass(), ActorsList);
		for (AActor* pActor : ActorsList)
		{
			if (!pActor)
				continue;

			if (IsActorCreatedByGenerator(pActor))
			{
				if (FVector::Dist(CurrentPaintPos, pActor->GetActorLocation()) <= PaintSphereSize)
				{
					FGenerationGridCell* pGridCell = GetNearestGridCellToPointFast(pActor->GetActorLocation());
					if (pGridCell)
					{
						for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : pGridCell->CellSlotsInfo)
						{
							//CurPair.Value.TempTransformsForDistanceChecks.RemoveSwap(pActor->GetTransform());
							RemoveTransfFromTransfArr(CurPair.Value.TempTransformsForDistanceChecks, pActor->GetTransform());
						}
					}
					pActor->Destroy();
				}
			}
		}

		for (int32 i = 0; i < ProcGenActorsPtrs.Num(); ++i)
		{
			AProcGenActor* ActProcGen = ProcGenActorsPtrs[i].Get();
			if (!ActProcGen)
				continue;

			const TSet<UActorComponent*> ActComponents = ActProcGen->GetComponents();
			for (UActorComponent* pActComp : ActComponents)
			{
				if (!pActComp || pActComp->IsDefaultSubobject())
					continue;

				UHierarchicalInstancedStaticMeshComponent* pHISM_Comp = Cast<UHierarchicalInstancedStaticMeshComponent>(pActComp);
				USceneComponent* pScene_Comp = Cast<USceneComponent>(pActComp);
				if (pScene_Comp && !pHISM_Comp)
				{
					FGenerationGridCell* pGridCell = GetNearestGridCellToPointFast(pScene_Comp->GetComponentLocation());
					if (pGridCell)
					{
						for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : pGridCell->CellSlotsInfo)
						{
							//CurPair.Value.TempTransformsForDistanceChecks.RemoveSwap(pScene_Comp->GetComponentTransform());
							RemoveTransfFromTransfArr(CurPair.Value.TempTransformsForDistanceChecks, pScene_Comp->GetComponentTransform());
						}
					}
				}

				UStaticMeshComponent* pSM_Comp = Cast<UStaticMeshComponent>(pActComp);
				
				if (!pHISM_Comp && pSM_Comp)
				{
					if (FVector::Dist(CurrentPaintPos, pSM_Comp->GetComponentTransform().GetLocation()) > PaintSphereSize)
					{
						continue;
					}

					FTransform ObjTransf = pSM_Comp->GetComponentTransform();

					ActProcGen->GeneratedStaticMeshComponents.RemoveSwap(pSM_Comp);
					pSM_Comp->DetachFromParent();
					pSM_Comp->UnregisterComponent();
					pSM_Comp->RemoveFromRoot();
					pSM_Comp->ConditionalBeginDestroy();

					if (ActProcGen->CurCreatedProcGenSlotObject.IsValid())
					{
						ActProcGen->CurCreatedProcGenSlotObject.Get()->RemoveTempTrasfFromSlots(ObjTransf);
					}
					continue;
				}

				UDecalComponent* pD_Comp = Cast<UDecalComponent>(pActComp);
				if (pD_Comp)
				{
					if (FVector::Dist(CurrentPaintPos, pD_Comp->GetComponentTransform().GetLocation()) > PaintSphereSize)
					{
						continue;
					}

					FTransform ObjTransf = pD_Comp->GetComponentTransform();
					ActProcGen->GeneratedDecalsComponents.RemoveSwap(pD_Comp);
					pD_Comp->DetachFromParent();
					pD_Comp->UnregisterComponent();
					pD_Comp->RemoveFromRoot();
					pD_Comp->ConditionalBeginDestroy();
					if (ActProcGen->CurCreatedProcGenSlotObject.IsValid())
					{
						ActProcGen->CurCreatedProcGenSlotObject.Get()->RemoveTempTrasfFromSlots(ObjTransf);
					}
					continue;
				}

				if (pHISM_Comp)
				{
					FTransform InstTransf = FTransform();
					TArray<int32> OverInstsIdsArr = pHISM_Comp->GetInstancesOverlappingSphere(CurrentPaintPos, PaintSphereSize);
					for (int32 instanceId : OverInstsIdsArr)
					{
						pHISM_Comp->GetInstanceTransform(instanceId, InstTransf, true);
						FGenerationGridCell* pGridCell = GetNearestGridCellToPointFast(InstTransf.GetLocation());
						if (pGridCell)
						{
							for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : pGridCell->CellSlotsInfo)
							{
								//CurPair.Value.TempTransformsForDistanceChecks.RemoveSwap(InstTransf);
								RemoveTransfFromTransfArr(CurPair.Value.TempTransformsForDistanceChecks, InstTransf);
							}
						}
					}
					pHISM_Comp->RemoveInstances(OverInstsIdsArr);
					continue;
				}
			}
		}
		//temp, another way to delete procedurally created actors and components
		if(!bUseCollisionChecksInPaintRemoveMode)
			return;

		ECollisionChannel CollisionChannel = ECC_Visibility;

		//FHitResult CheckHit = FHitResult(ForceInit);
		bool bIsHit = false;

		FCollisionShape sphereColl = FCollisionShape::MakeSphere(PaintSphereSize);

		FCollisionQueryParams FCQP = FCollisionQueryParams(FName(TEXT("Paint_Trace")), true);
		FCQP.bTraceComplex = true;
		FCQP.bReturnPhysicalMaterial = true;

		FCollisionResponseParams FCRP = FCollisionResponseParams(ECR_Overlap/*ECR_Block*/);
		TArray<FHitResult> arrHits = TArray<FHitResult>();
		bIsHit = pCurEdWorld->SweepMultiByChannel(arrHits, CurrentPaintPos, CurrentPaintPos + FVector(0, 0, 1), FQuat::Identity, CollisionChannel, sphereColl, FCQP, FCRP);
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

			for (AActor* pHitedActor : ArrHitedActors)
			{
				if (IsActorCreatedByGenerator(pHitedActor))
				{
					FGenerationGridCell* pGridCell = GetNearestGridCellToPointFast(pHitedActor->GetActorLocation());
					if (pGridCell)
					{
						for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : pGridCell->CellSlotsInfo)
						{
							//CurPair.Value.TempTransformsForDistanceChecks.RemoveSwap(pHitedActor->GetTransform());
							RemoveTransfFromTransfArr(CurPair.Value.TempTransformsForDistanceChecks, pHitedActor->GetTransform());
						}
					}
					pHitedActor->Destroy();
				}
			}

			for (UPrimitiveComponent* pHitedPrimitiveComponent : ArrHitedComponents)
			{
				AProcGenActor* OwnerProcGen = Cast<AProcGenActor>(pHitedPrimitiveComponent->GetOwner());
				if (OwnerProcGen)
				{
					UStaticMeshComponent* pSM_Comp = Cast<UStaticMeshComponent>(pHitedPrimitiveComponent);
					UHierarchicalInstancedStaticMeshComponent* pHISM_Comp = Cast<UHierarchicalInstancedStaticMeshComponent>(pHitedPrimitiveComponent);

					USceneComponent* pScene_Comp = Cast<USceneComponent>(pHitedPrimitiveComponent);
					if (pScene_Comp && !pHISM_Comp)
					{
						FGenerationGridCell* pGridCell = GetNearestGridCellToPointFast(pScene_Comp->GetComponentLocation());
						if (pGridCell)
						{
							for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : pGridCell->CellSlotsInfo)
							{
								//CurPair.Value.TempTransformsForDistanceChecks.RemoveSwap(pScene_Comp->GetComponentTransform());
								RemoveTransfFromTransfArr(CurPair.Value.TempTransformsForDistanceChecks, pScene_Comp->GetComponentTransform());
							}
						}
					}

					if (!pHISM_Comp && pSM_Comp && !pSM_Comp->IsDefaultSubobject())
					{
						FTransform ObjTransf = pSM_Comp->GetComponentTransform();

						OwnerProcGen->GeneratedStaticMeshComponents.Remove(pSM_Comp);
						pHitedPrimitiveComponent->DetachFromParent();
						pHitedPrimitiveComponent->UnregisterComponent();
						pHitedPrimitiveComponent->RemoveFromRoot();
						pHitedPrimitiveComponent->ConditionalBeginDestroy();

						if (OwnerProcGen->CurCreatedProcGenSlotObject.IsValid())
						{
							OwnerProcGen->CurCreatedProcGenSlotObject.Get()->RemoveTempTrasfFromSlots(ObjTransf);
						}
					}

					if (pHISM_Comp)
					{
						FTransform InstTransf = FTransform();
						TArray<int32> OverInstsIdsArr = pHISM_Comp->GetInstancesOverlappingSphere(CurrentPaintPos, PaintSphereSize);
						for (int32 instanceId : OverInstsIdsArr)
						{
							pHISM_Comp->GetInstanceTransform(instanceId, InstTransf, true);
							FGenerationGridCell* pGridCell = GetNearestGridCellToPointFast(InstTransf.GetLocation());
							if (pGridCell)
							{
								for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : pGridCell->CellSlotsInfo)
								{
									//CurPair.Value.TempTransformsForDistanceChecks.RemoveSwap(InstTransf);
									RemoveTransfFromTransfArr(CurPair.Value.TempTransformsForDistanceChecks, InstTransf);
								}
							}
						}
						pHISM_Comp->RemoveInstances(OverInstsIdsArr);
					}
				}
			}
		}
	}
	else
	{
		CurProcGenPaintMode = EProcGenPaintMode::Paint;
		if (SelectedToPaintProcGenActorsPtrs.Num() > 0)
		{
			for (TWeakObjectPtr<AProcGenActor>& PGActorPtr : SelectedToPaintProcGenActorsPtrs)
			{
				if (PGActorPtr.IsValid())
				{
					PGActorPtr.Get()->PaintBySphere(CurrentPaintPos, PaintSphereSize);
				}
			}
		}
	}
}

static const int32 numMeshesPerFrameLoad = 1500;

void UProcGenManager::UpdateActiveAndInactiveGridCells()
{
	if (pGenerationHelperThread.Get())
	{
		UWorld* CurrentWorld = GetWorldPRFEditor();
		bool DebugDrawEnabled = CVarGenerationGridEnableCellsDebugDraw.GetValueOnGameThread();
		FScopeLock lockGridPtrs(&pGenerationHelperThread->GridPtrsTL);

		for (int32 i = ArrGridCellsCurrentlyActivePtrs.Num() - 1; i >= 0; --i)
		{
			FGenerationGridCell* GridCellPtr = ArrGridCellsCurrentlyActivePtrs[i];
			bool bCellFilled = true;
			for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : GridCellPtr->CellSlotsInfo)
			{
				if (CurPair.Value.NumSlotDataLoaded < CurPair.Value.SlotData.Num() - 1)
				{
					bool bLoopEnded = false;
					for (int32 ic = 0; ic < numMeshesPerFrameLoad; ++ic)
					{
						CurPair.Value.LoadSlotDataById(CurPair.Value.NumSlotDataLoaded + ic);
						if ((CurPair.Value.NumSlotDataLoaded + ic) == CurPair.Value.SlotData.Num() - 1)
						{
							CurPair.Value.NumSlotDataLoaded = CurPair.Value.SlotData.Num() - 1;
							bLoopEnded = true;
							break;
						}
					}
					if (!bLoopEnded)
					{
						CurPair.Value.NumSlotDataLoaded += numMeshesPerFrameLoad;
						CurPair.Value.NumSlotDataLoaded = FMath::Clamp(CurPair.Value.NumSlotDataLoaded, 0, CurPair.Value.SlotData.Num() - 1);
					}
				}
			}

			if (DebugDrawEnabled)
			{
				UKismetSystemLibrary::DrawDebugBox(CurrentWorld, GridCellPtr->CellBounds.GetCenter(), GridCellPtr->CellBounds.GetExtent(), FLinearColor::Red, FRotator::ZeroRotator, 0.001f, 50.0f);
				//UKismetSystemLibrary::DrawDebugSphere(CurrentWorld, GridCellPtr->CellBounds.GetCenter(), 150.0f, 12, FLinearColor::Blue, 0.001f, 50.0f);
			}
		}

		for (int32 i = ArrGridCellsCurrentlyInactivePtrs.Num() - 1; i >= 0; --i)
		{
			FGenerationGridCell* GridCellPtr = ArrGridCellsCurrentlyInactivePtrs[i];
			bool bCellEmpty = true;
			for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : GridCellPtr->CellSlotsInfo)
			{
				if (CurPair.Value.NumSlotDataLoaded > 0)
				{
					bCellEmpty = false;
					if (CurPair.Value.NumSlotDataLoaded > CurPair.Value.SlotData.Num() - 1)
					{
						CurPair.Value.NumSlotDataLoaded = CurPair.Value.SlotData.Num() - 1;
					}
					bool bLoopEnded = false;
					for (int32 ic = 0; ic < numMeshesPerFrameLoad; ++ic)
					{
						CurPair.Value.UnloadSlotDataById(CurPair.Value.NumSlotDataLoaded - ic);
						if ((CurPair.Value.NumSlotDataLoaded - ic) == 0)
						{
							CurPair.Value.NumSlotDataLoaded = 0;
							bLoopEnded = true;
							break;
						}
					}
					if (!bLoopEnded)
					{
						CurPair.Value.NumSlotDataLoaded -= numMeshesPerFrameLoad;
						CurPair.Value.NumSlotDataLoaded = FMath::Clamp(CurPair.Value.NumSlotDataLoaded, 0, CurPair.Value.SlotData.Num() - 1);
					}
				}
				else
				{

				}
			}

			if (bCellEmpty)
			{
				ArrGridCellsCurrentlyInactivePtrs.RemoveAtSwap(i);
			}
			//GridCellPtr->CellSlotsInfo
		}

		if (DebugDrawEnabled)
		{
			FVector CameraPt = CurrentCameraViewPosTSVar.GetVar();
			FGenerationGridCell* NearestCellPtr = GetNearestGridCellToPointFast(CameraPt);
			if (NearestCellPtr)
			{
				UKismetSystemLibrary::DrawDebugLine(CurrentWorld, NearestCellPtr->CellBounds.GetCenter(), NearestCellPtr->CellBounds.GetCenter() + FVector(0, 0, 500000), FLinearColor::Blue, 0.001f, 150.0f);
			}
		}
	}
}

void UProcGenManager::CreateActorByParams(const FActorToDelayedCreateParams& DelayedCreateParams, UWorld* CurWorldPtr)
{
	if (DelayedCreateParams.bSpawnInEditor)
	{
#if WITH_EDITOR
		AActor* pGeneratedActor = GEditor->AddActor(CurWorldPtr->GetCurrentLevel(), DelayedCreateParams.ActorClassPtr, DelayedCreateParams.ActorTransform, true);
		if (pGeneratedActor)
		{
			pGeneratedActor->SetActorScale3D(DelayedCreateParams.ActorTransform.GetScale3D());
			if (DelayedCreateParams.ParentProcGenSlotParamsPtr && DelayedCreateParams.ParentProcGenSlotObjPtr)
			{
				DelayedCreateParams.ParentProcGenSlotParamsPtr->GeneratedActorsPtrs.Add(pGeneratedActor);
				if (DelayedCreateParams.LinkedProcGenActorPtr.IsValid())
				{
					pGeneratedActor->OnDestroyed.AddDynamic(DelayedCreateParams.LinkedProcGenActorPtr.Get(), &AProcGenActor::OnLinkedActorDestroyed);
				}
			}

			GEditor->SelectActor(pGeneratedActor, 1, 0);
			pGeneratedActor->InvalidateLightingCache();
			pGeneratedActor->PostEditMove(true);
			pGeneratedActor->MarkPackageDirty();

			AddProceduralTagToActor(pGeneratedActor);

			
		}
#endif
		//GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Green, FString::Printf(TEXT("UProcGenManager::CreateActorByParams pGeneratedActor name - %s"), *pGeneratedActor->GetName()));
	}
	else
	{
		FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
		SpawnInfo.OverrideLevel = CurWorldPtr->GetCurrentLevel();
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
		SpawnInfo.bHideFromSceneOutliner = true;
		SpawnInfo.bCreateActorPackage = true;
#endif
		SpawnInfo.ObjectFlags = RF_Transactional;
		//SpawnInfo.ObjectFlags = RF_Transient | RF_WasLoaded | RF_Transactional | RF_Dynamic;
		//SpawnInfo.bCreateActorPackage = true;
		//SpawnInfo.ObjectFlags = InObjectFlags;

		AActor* pGeneratedActor = CurWorldPtr->SpawnActor(DelayedCreateParams.ActorClassPtr, &DelayedCreateParams.ActorTransform, SpawnInfo);
		if (pGeneratedActor)
		{
			pGeneratedActor->SetActorScale3D(DelayedCreateParams.ActorTransform.GetScale3D());
			if (DelayedCreateParams.ParentProcGenSlotParamsPtr && DelayedCreateParams.ParentProcGenSlotObjPtr)
			{
				DelayedCreateParams.ParentProcGenSlotParamsPtr->GeneratedActorsPtrs.Add(pGeneratedActor);
				if (DelayedCreateParams.LinkedProcGenActorPtr.IsValid())
				{
					pGeneratedActor->OnDestroyed.AddDynamic(DelayedCreateParams.LinkedProcGenActorPtr.Get(), &AProcGenActor::OnLinkedActorDestroyed);
				}
			}
			
			AddProceduralTagToActor(pGeneratedActor);
		}
	}
}

UProcGenManager* UProcGenManager::GetCurManager()
{
	return CurProcGenManager.Get();
}

void UProcGenManager::SetCurManagerAsThisObject()
{
	CurProcGenManager = this;
}

bool UProcGenManager::IsActorAreGenerator(AActor* pActor)
{
	if(!pActor)
		return false;

	for (int32 i = 0; i < ProcGenActorsPtrs.Num(); ++i)
	{
		if (pActor == ProcGenActorsPtrs[i].Get())
			return true;
	}
	return false;
}

bool UProcGenManager::IsActorCreatedByGenerator(AActor* pActor)
{
	if (!pActor)
		return false;

	if (pActor->ActorHasTag(TEXT("XProc")))
		return true;

	//pActor->Tags.Add(TEXT("XProc"));
	return false;
}

void UProcGenManager::AddProceduralTagToActor(AActor* pActor)
{
	if (!pActor)
		return;

	if (pActor->ActorHasTag(TEXT("XProc")))
		return;

	pActor->Tags.Add(TEXT("XProc"));
}

void UProcGenManager::RemoveAllProcGeneratedActors()
{
	UWorld* pCurEdWorld = GetWorldPRFEditor();
	if (!pCurEdWorld)
		return;

	TArray<AActor*> ActorsList = TArray<AActor*>();
	ActorsList.Reserve(10000);
	UGameplayStatics::GetAllActorsOfClass(pCurEdWorld, AActor::StaticClass(), ActorsList);
	for (AActor* pActor : ActorsList)
	{
		if (!pActor)
			continue;

		if (IsActorCreatedByGenerator(pActor))
		{
			pActor->Destroy();
		}
	}
}

void UProcGenManager::ClearGeneratorsCache()
{
	for (int32 i = 0; i < ProcGenActorsPtrs.Num(); ++i)
	{
		AProcGenActor* ActProcGen = ProcGenActorsPtrs[i].Get();
		if (!ActProcGen)
			continue;

		ActProcGen->CachedInstMeshCellInfoPointer = nullptr;
		ActProcGen->CachedParentGridCellId = -1;
	}
}

TArray<AProcGenParamsModifierActor*> UProcGenManager::GetAllCollidedGenParamsModifierActors(const FBox& BoxToCollide)
{
	TArray<AProcGenParamsModifierActor*> ArrGPMActorsPtrs = TArray<AProcGenParamsModifierActor*>();
	AProcGenParamsModifierActor* pGPMActor = nullptr;
	for (int32 i = 0; i < ProcGenParamsModifierActorsPtrs.Num(); ++i)
	{
		pGPMActor = ProcGenParamsModifierActorsPtrs[i].Get();
		if (pGPMActor)
		{
			if (BoxToCollide.Intersect(pGPMActor->ShapeBoundingBox))
			{
				ArrGPMActorsPtrs.Add(pGPMActor);
			}
		}
	}
	return ArrGPMActorsPtrs;
}

FGenerationGridCell* UProcGenManager::GetGenerationGridCellByLocation(const FVector& Location)
{
	for (FGenerationGridCell& CurGridCell : ArrGridCells)
	{
		if (CurGridCell.CellBounds.IsInsideXY(Location))
		{
			return &CurGridCell;
		}
	}
	return nullptr;
}

FGenerationGridCell* UProcGenManager::GetGenerationGridCellInCustomArrByLocation(const FVector& Location, TArray<FGenerationGridCell*>& CustomCellsArr)
{
	for (FGenerationGridCell* CurGridCellPtr : CustomCellsArr)
	{
		if (CurGridCellPtr->CellBounds.IsInsideXY(Location))
		{
			return CurGridCellPtr;
		}
	}
	return nullptr;
}

TArray<FGenerationGridCell*> UProcGenManager::GetGridCellsArrByBounds(const FBox& Bounds)
{
	TArray<FGenerationGridCell*> ArrCellsPtrs = TArray<FGenerationGridCell*>();
	for (FGenerationGridCell& CurGridCell : ArrGridCells)
	{
		if (CurGridCell.CellBounds.IntersectXY(Bounds))
		{
			ArrCellsPtrs.Add(&CurGridCell);
		}
	}
	return ArrCellsPtrs;
}

bool UProcGenManager::ClearGenerationGridCellsByLocation(const FVector& Location)
{
	for (FGenerationGridCell& CurGridCell : ArrGridCells)
	{
		for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : CurGridCell.CellSlotsInfo)
		{
			for (int32 i = CurPair.Value.SlotData.Num() - 1; i >= 0; --i)
			{
				FGenerationInCellSlotData& curSlotData = CurPair.Value.SlotData[i];
				if (curSlotData.SlotTransf.GetLocation() == Location)
				{
					CurPair.Value.SlotData.RemoveAtSwap(i);
					return true;
				}
			}
			/*for (FGenerationInCellSlotData& curSlotData : CurPair.Value.SlotData)
			{
				if(curSlotData.SlotTransf.GetLocation() == Location)
			}*/
		}
	}
	return false;
}

bool UProcGenManager::ClearGenerationGridCellsByGenSlotId(int32 Id)
{
	bool bFinded = false;
	for (FGenerationGridCell& CurGridCell : ArrGridCells)
	{
		if (CurGridCell.CellSlotsInfo.Remove(Id) > 0)
		{
			bFinded = true;
		}
	}
	return bFinded;
}

FGenerationGridCell* UProcGenManager::GetNearestGridCellToPointFast(const FVector& Point)
{
	int32 posXMin = CVarGenerationGridXMin.GetValueOnGameThread();
	int32 posYMin = CVarGenerationGridYMin.GetValueOnGameThread();
	int32 posXMax = CVarGenerationGridXMax.GetValueOnGameThread();
	int32 posYMax = CVarGenerationGridYMax.GetValueOnGameThread();
	int32 cellXYSize = CVarGenerationGridCellXYSize.GetValueOnGameThread();
	int32 cellZSize = CVarGenerationGridCellZSize.GetValueOnGameThread();
	int32 numCellsX = (posXMax * 2) / cellXYSize;
	int32 numCellsY = (posYMax * 2) / cellXYSize;
	int32 numCells = numCellsX * numCellsY;
	//int32 XTest1 = (posXMax * 2) - Point.X;
	FVector CoordsConverted = FVector::ZeroVector;
	//CoordsConverted.X = ((posXMax * 2) - (Point.X + posXMax)) / cellXYSize;
	//CoordsConverted.Y = ((posYMax * 2) - (Point.Y + posYMax)) / cellXYSize;
	CoordsConverted.X = int32(Point.X + posXMax) / cellXYSize;
	CoordsConverted.Y = int32(Point.Y + posYMax) / cellXYSize;
	int32 ElemInArrayCoord = (CoordsConverted.X * numCellsX) + CoordsConverted.Y;
	if(ElemInArrayCoord >= ArrGridCells.Num() || ElemInArrayCoord < 0)
		return nullptr;

	return &ArrGridCells[ElemInArrayCoord];
}

TArray<FGenerationGridCell*> UProcGenManager::GetNearestGridCellsGroupToPoint(const FVector& Point)
{
	int32 posXMax = CVarGenerationGridXMax.GetValueOnGameThread();
	int32 posYMax = CVarGenerationGridYMax.GetValueOnGameThread();
	int32 cellXYSize = CVarGenerationGridCellXYSize.GetValueOnGameThread();
	int32 cellZSize = CVarGenerationGridCellZSize.GetValueOnGameThread();
	int32 numCellsX = (posXMax * 2) / cellXYSize;
	int32 numCellsY = (posYMax * 2) / cellXYSize;
	int32 numCells = numCellsX * numCellsY;

	TArray<FGenerationGridCell*> ArrCells = TArray<FGenerationGridCell*>();
	ArrCells.Reserve(9);
	FGenerationGridCell* CenterCellPtr = GetNearestGridCellToPointFast(Point);
	if (!CenterCellPtr)
	{
		return ArrCells;
	}

	ArrCells.Add(CenterCellPtr);

	int32 CenterCellId = CenterCellPtr->CellId;
	for (int32 i = 0; i < 8; ++i)
	{
		int32 SelectedCellId = -1;
		if (i == 0)
		{
			SelectedCellId = CenterCellId + numCellsX;
		}
		else if (i == 1)
		{
			SelectedCellId = CenterCellId - numCellsX;
		}
		else if (i == 2)
		{
			SelectedCellId = CenterCellId + 1;
		}
		else if (i == 3)
		{
			SelectedCellId = CenterCellId - 1;
		}
		else if (i == 4)
		{
			SelectedCellId = CenterCellId + numCellsX + 1;
		}
		else if (i == 5)
		{
			SelectedCellId = CenterCellId + numCellsX - 1;
		}
		else if (i == 6)
		{
			SelectedCellId = CenterCellId - numCellsX + 1;
		}
		else if (i == 7)
		{
			SelectedCellId = CenterCellId - numCellsX - 1;
		}

		if (SelectedCellId >= ArrGridCells.Num() || SelectedCellId < 0)
		{
			continue;
		}
		else
		{
			ArrCells.Add(&ArrGridCells[SelectedCellId]);
		}
	}
	return ArrCells;
}

void FGenerationInCellSlotData::LoadSlotData()
{
	if (bSlotLoaded)
		return;

	AProcGenActor* pPProcGenActor = Cast<AProcGenActor>(ParentActorPtr.Get());
	if (!pPProcGenActor)
	{
		return;
	}

	switch (CellSlotDataType)
	{
	case EGenCellSlotDataType::ActorToGen:
	{
		if (ActorToCreateClassPtr && !GeneratedActorPtr.IsValid() && pPProcGenActor->GetWorld())
		{
			FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
			SpawnInfo.OverrideLevel = pPProcGenActor->GetWorld()->GetCurrentLevel();
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
			SpawnInfo.bHideFromSceneOutliner = true;
			SpawnInfo.bCreateActorPackage = true;
#endif
			SpawnInfo.ObjectFlags = RF_Transient;//RF_Transactional;

			AActor* pGeneratedActor = pPProcGenActor->GetWorld()->SpawnActor(ActorToCreateClassPtr, &SlotTransf, SpawnInfo);
			if (pGeneratedActor)
			{
				pGeneratedActor->SetActorScale3D(SlotTransf.GetScale3D());
				GeneratedActorPtr = pGeneratedActor;

				if (!pGeneratedActor->ActorHasTag(TEXT("XTempProc")))
				{
					pGeneratedActor->Tags.Add(TEXT("XTempProc"));
				}
				//pProcGenManager->AddProceduralTagToActor(pGeneratedActor);

			}
		}
	}
	break;
	case EGenCellSlotDataType::SMeshToGen:
	{
		InstSM_Id = pPProcGenActor->CreateNewInstancedSMInst(StaticMeshPtr, SlotTransf, ParentCellId);
		//GeneratedSMeshCompPtr = pPProcGenActor->CreateNewSMComponent(StaticMeshPtr, SlotTransf, nullptr, nullptr, true);
	}
	break;
	case EGenCellSlotDataType::DecalToGen:
	{
		if (!GeneratedDecalCompPtr.IsValid())
		{
			GeneratedDecalCompPtr = pPProcGenActor->CreateNewDecalComponent(DecalMaterial, SlotTransf, DecalSize);
			if (GeneratedDecalCompPtr.IsValid())
			{
				GeneratedDecalCompPtr.Get()->SetFlags(GeneratedDecalCompPtr.Get()->GetFlags() | RF_Transient);
			}
		}
	}
	break;
	default:
		break;
	}

	bSlotLoaded = true;
}

void FGenerationInCellSlotData::UnloadSlotData()
{
	if (!bSlotLoaded)
		return;

	AProcGenActor* pPProcGenActor = Cast<AProcGenActor>(ParentActorPtr.Get());
	if (!pPProcGenActor)
	{
		return;
	}

	switch (CellSlotDataType)
	{
	case EGenCellSlotDataType::ActorToGen:
	{
		if (GeneratedActorPtr.IsValid())
		{
			GeneratedActorPtr.Get()->Destroy();
			GeneratedActorPtr.Reset();
		}
	}
	break;
	case EGenCellSlotDataType::SMeshToGen:
	{
		/*if (GeneratedSMeshCompPtr.Get())
		{
			pPProcGenActor->RemoveSMComp(GeneratedSMeshCompPtr.Get());
			GeneratedSMeshCompPtr = nullptr;
		}*/
		if (InstSM_Id > -1)
		{
			pPProcGenActor->RemoveInstancedSMInstById(StaticMeshPtr, InstSM_Id, ParentCellId);
		}
	}
	break;
	case EGenCellSlotDataType::DecalToGen:
	{
		if (GeneratedDecalCompPtr.IsValid())
		{
			auto* DecalCpPtr = GeneratedDecalCompPtr.Get();
			DecalCpPtr->DetachFromParent();
			DecalCpPtr->UnregisterComponent();
			DecalCpPtr->RemoveFromRoot();
			DecalCpPtr->ConditionalBeginDestroy();
			GeneratedDecalCompPtr.Reset();
		}
	}
	break;
	default:
		break;
	}

	bSlotLoaded = false;
}

void FGenerationGridCellGenSlot::LoadSlotDataById(int32 Id)
{
	if (Id >= SlotData.Num() || Id <= -1)
		return;

	SlotData[Id].LoadSlotData();
}

void FGenerationGridCellGenSlot::UnloadSlotDataById(int32 Id)
{
	if (Id >= SlotData.Num() || Id <= -1)
		return;

	SlotData[Id].UnloadSlotData();
}

UWorld* UProcGenManager::GetCurrentWorldStatic()
{
	UWorld* world = nullptr;
	if (!GEngine)
		return world;

#if WITH_EDITOR
	if (GIsEditor && GEditor != nullptr)
	{
		if (GPlayInEditorID == -1)
		{
			FWorldContext* worldContext = GEditor->GetPIEWorldContext(1);
			if (worldContext == nullptr)
			{
				if (UGameViewportClient * viewport = GEngine->GameViewport)
				{
					world = viewport->GetWorld();
				}
			}
			else
			{
				world = worldContext->World();
			}
		}
		else
		{
			FWorldContext* worldContext = GEditor->GetPIEWorldContext(GPlayInEditorID);
			if (worldContext == nullptr)
			{
				return nullptr;
			}
			world = worldContext->World();
		}
	}
	else
	{
		world = GEngine->GetCurrentPlayWorld(nullptr);
	}
#else
	world = GEngine->GetCurrentPlayWorld(nullptr);
#endif
	return world;
}

UWorld* UProcGenManager::GetWorldPRFEditor()
{
	UWorld* pCurSelectedWorld = nullptr;
	if (bPIESessionStarted)
	{
		pCurSelectedWorld = GetCurrentWorldStatic();
	}
	else
	{
#if WITH_EDITOR
		if (GEditor)
		{
			pCurSelectedWorld = GEditor->GetEditorWorldContext().World();
		}
		else
		{
			pCurSelectedWorld = GetCurrentWorldStatic();
		}

		if (!pCurSelectedWorld)
		{
			pCurSelectedWorld = GetCurrentWorldStatic();
		}
#else
		pCurSelectedWorld = GetCurrentWorldStatic();
#endif
	}

	return pCurSelectedWorld;
}

void UProcGenManager::RemoveTransfFromTransfArr(TArray<FTransform>& TransfsArr, const FTransform& TransfToDelete)
{
	if (TransfsArr.Num() <= 0)
		return;

	for (int32 i = TransfsArr.Num() - 1; i >= 0; --i)
	{
		if (TransfsArr[i].GetLocation() == TransfToDelete.GetLocation())
		{
			TransfsArr.RemoveAtSwap(i);
		}
	}
}

void UProcGenManager::OnPIESessionStarted(const bool bStarted)
{
	bPIESessionStarted = true;
	/*FString MsgTextStr = FString::Printf(TEXT("UProcGenManager::OnPIESessionStarted call, bStarted - %s"), bStarted ? *FString("True") : *FString("false"));
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));*/
}

void UProcGenManager::OnPIESessionEnded(const bool bEnded)
{
	bPIESessionStarted = false;
	/*FString MsgTextStr = FString::Printf(TEXT("UProcGenManager::OnPIESessionEnded call, bEnded - %s"), bEnded ? *FString("True") : *FString("false"));
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));*/
	//world switching fix
	for (FGenerationGridCell& CurGridCell : ArrGridCells)
	{
		for (TPair<int32, FGenerationGridCellGenSlot>& CurPair : CurGridCell.CellSlotsInfo)
		{
			for (int32 i = CurPair.Value.SlotData.Num() - 1; i >= 0; --i)
			{
				FGenerationInCellSlotData& curSlotData = CurPair.Value.SlotData[i];
				curSlotData.UnloadSlotData();
			}
		}
	}
}

void UProcGenManager::OnLevelLoadStarted(const FString& LevelName)
{
	//Reset all cells info
	for (FGenerationGridCell& CurGridCell : ArrGridCells)
	{
		CurGridCell.CellSlotsInfo.Empty();
	}
}

static FAutoConsoleCommand RebuildGenerationGrid(
	TEXT("ProcGen.RebuildGenerationGrid"),
	TEXT("Rebuild Generation Grid"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
		[](const TArray< FString >& Args)
		{
			UProcGenManager* pPGM = UProcGenManager::GetCurManager();
			if (!pPGM || !pPGM->pGenerationHelperThread.Get())
				return;

			FScopeLock lock(&pPGM->pGenerationHelperThread->ThreadLock);
			for (FGenerationGridCell& CurGridCell : pPGM->ArrGridCells)
			{
				CurGridCell.CellSlotsInfo.Empty();
			}

			pPGM->ArrGridCellsCurrentlyActivePtrs.Empty();
			pPGM->ArrGridCellsCurrentlyInactivePtrs.Empty();

			pPGM->ArrGridCells.Empty();

			int32 posXMin = CVarGenerationGridXMin.GetValueOnGameThread();
			int32 posYMin = CVarGenerationGridYMin.GetValueOnGameThread();
			int32 posXMax = CVarGenerationGridXMax.GetValueOnGameThread();
			int32 posYMax = CVarGenerationGridYMax.GetValueOnGameThread();
			int32 cellXYSize = CVarGenerationGridCellXYSize.GetValueOnGameThread();
			int32 cellZSize = CVarGenerationGridCellZSize.GetValueOnGameThread();
			int32 numCellsX = (posXMax * 2) / cellXYSize;
			int32 numCellsY = (posYMax * 2) / cellXYSize;
			int32 numCells = numCellsX * numCellsY;
			FGenerationGridCell NewGridCell = FGenerationGridCell();
			FVector CellPos = FVector(0);
			FVector CellExt = FVector(0);

			CellExt.X = cellXYSize / 2;
			CellExt.Y = CellExt.X;
			CellExt.Z = cellZSize / 2;

			CellPos.Z = CellPos.Z - (cellZSize / 4);
			int32 NumCellsG = 0;
			for (int32 i = 0; i < numCellsX; ++i)
			{
				CellPos.X = (cellXYSize / 2) - posXMax;
				posXMax -= cellXYSize;
				posYMax = CVarGenerationGridYMax.GetValueOnGameThread();
				for (int32 ic = 0; ic < numCellsY; ++ic)
				{
					CellPos.Y = (cellXYSize / 2) - posYMax;
					posYMax -= cellXYSize;
					NewGridCell.CellBounds = FBox::BuildAABB(CellPos, CellExt);
					NewGridCell.CellId = NumCellsG;
					pPGM->ArrGridCells.Add(NewGridCell);
					++NumCellsG;
				}
			}

			FString MsgTextStr = FString::Printf(TEXT("ProcGen.RebuildGenerationGrid - command executed successfully, generated grid cells num - %i"), pPGM->ArrGridCells.Num());
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MsgTextStr));
		})
);

FGenerationHelperThread::FGenerationHelperThread()
	: bIsRunning(false)
	, Thread(nullptr)
	, ThreadLock()
	, GridPtrsTL()
{
}

FGenerationHelperThread::~FGenerationHelperThread()
{
	StopWork();
	if (Thread.Get())
	{
		Thread.Reset(nullptr);
	}
}

void FGenerationHelperThread::StartWork()
{
	if (!bIsRunning)
	{
		bIsRunning = true;
		Thread.Reset(FRunnableThread::Create(this, TEXT("ProcGenerationHelperThread")));
	}
}

void FGenerationHelperThread::StopWork()
{
	if (bIsRunning)
	{
		Exit();
	}
}

uint32 FGenerationHelperThread::Run()
{
	while (bIsRunning)
	{
		{
			FScopeLock lock(&ThreadLock);
			UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
			if (pProcGenManager)
			{
				FVector CurCameraPos = pProcGenManager->CurrentCameraViewPosTSVar.GetVar();
				float GrabBBoxSZ = CVarGenerationGridGrabBBoxSize.GetValueOnAnyThread();
				float GrabDist = CVarGenerationGridGrabDistanceAround.GetValueOnAnyThread();
				FBox ViewShapeBox = FBox::BuildAABB(CurCameraPos, FVector(GrabBBoxSZ, GrabBBoxSZ, GrabBBoxSZ));
				FSphere ViewSph = FSphere(EForceInit::ForceInit);
				ViewSph.Center = CurCameraPos;
				ViewSph.W = GrabDist;
				TArray<FGenerationGridCell*> CellsCurrentlyActivePtrs = TArray<FGenerationGridCell*>();
				for (FGenerationGridCell& CurGridCell : pProcGenManager->ArrGridCells)
				{
					if (ViewShapeBox.IntersectXY(CurGridCell.CellBounds))
					{
						if (FMath::SphereAABBIntersection(ViewSph, CurGridCell.CellBounds))
						{
							CellsCurrentlyActivePtrs.Add(&CurGridCell);
						}
						//UKismetSystemLibrary::DrawDebugBox(pCurEdWorld, CurGridCell.CellBounds.GetCenter(), CurGridCell.CellBounds.GetExtent(), FLinearColor::Red, FRotator::ZeroRotator, 0.001f, 50.0f);
					}
				}

				for (FGenerationGridCell* GridCellPtr : CellsCurrentlyActivePtrs)
				{
					FScopeLock lock2(&GridPtrsTL);

					if (!pProcGenManager->ArrGridCellsCurrentlyActivePtrs.Contains(GridCellPtr))
					{
						pProcGenManager->ArrGridCellsCurrentlyActivePtrs.Add(GridCellPtr);
					}

					if (pProcGenManager->ArrGridCellsCurrentlyInactivePtrs.Contains(GridCellPtr))
					{
						pProcGenManager->ArrGridCellsCurrentlyInactivePtrs.RemoveSwap(GridCellPtr);
					}
				}

				{
					FScopeLock lock2(&GridPtrsTL);
					for (int32 i = pProcGenManager->ArrGridCellsCurrentlyActivePtrs.Num() - 1; i >= 0; --i)
					{
						FGenerationGridCell* GridCellPtr = pProcGenManager->ArrGridCellsCurrentlyActivePtrs[i];
						if (!CellsCurrentlyActivePtrs.Contains(GridCellPtr))
						{
							pProcGenManager->ArrGridCellsCurrentlyInactivePtrs.AddUnique(GridCellPtr);
							pProcGenManager->ArrGridCellsCurrentlyActivePtrs.RemoveSwap(GridCellPtr);
						}
					}

					for (int32 i = pProcGenManager->ArrGridCellsCurrentlyInactivePtrs.Num() - 1; i >= 0; --i)
					{
						FGenerationGridCell* GridCellPtr = pProcGenManager->ArrGridCellsCurrentlyInactivePtrs[i];
						if (CellsCurrentlyActivePtrs.Contains(GridCellPtr))
						{
							pProcGenManager->ArrGridCellsCurrentlyInactivePtrs.RemoveSwap(GridCellPtr);
						}
					}
				}
			}
		}
		FPlatformProcess::Sleep(0.010f);
	}
	return 0;
}

void FGenerationHelperThread::Exit()
{
	if (!bIsRunning)
	{
		return;
	}
	bIsRunning = false;
	if (Thread.Get())
	{
		Thread->WaitForCompletion();
	}
}
