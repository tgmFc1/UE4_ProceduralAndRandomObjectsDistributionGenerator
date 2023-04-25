// Free to copy, edit and use in any work/projects, commercial or free

#include "ProcGen/ProcGenManager.h"
#include "..\..\Public\ProcGen\ProcGenManager.h"
#include "Engine.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Landscape.h"
#include "ProcAndRandObjDistrGenerator.h"
#include "ProcGen/ProcGenSlotObject.h"
#include "UnrealClient.h"
#include "LevelEditorViewport.h"
//#include "..\..\Public\ProcGen\ProcGenSlotObject.h"

static TWeakObjectPtr<UProcGenManager> CurProcGenManager = nullptr;

UProcGenManager::UProcGenManager() : Super(), DelayedCreateActorsQueue(), ProcGenActorsPtrs(), SelectedToPaintProcGenActorsPtrs()
{
	//FProcAndRandObjDistrGeneratorModule::SetProcGenManager(this);

	//CurProcGenManager = this;

	bPaintMode = false;
	PaintSphereSize = 1000.0f;
	CurrentPaintPos = FVector::ZeroVector;
	CurrentPaintCameraDir = FVector::ZeroVector;

	CurProcGenPaintMode = EProcGenPaintMode::Default;
	ColPaintInNextFrame = false;

	bUseCameraDirToGenerationInPaintMode = false;
	bUseCameraDirToRotationAlignGenObjectsInPaintMode = false;
	bUseCollisionChecksInPaintRemoveMode = false;
	TimeDelayBCreateNextPGActorsGroup = 0.0f;
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
	UWorld* pCurEdWorld = GEditor->GetEditorWorldContext().World();
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

void UProcGenManager::RequestPaint(bool bPaintOrClearBrush)
{
	UWorld* pCurEdWorld = GEditor->GetEditorWorldContext().World();
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

				UStaticMeshComponent* pSM_Comp = Cast<UStaticMeshComponent>(pActComp);
				if (pSM_Comp)
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

		FCollisionResponseParams FCRP = FCollisionResponseParams(/*ECR_Overlap*/ECR_Block);
		TArray<FHitResult> arrHits = TArray<FHitResult>();
		bIsHit = pCurEdWorld->SweepMultiByChannel(arrHits, CurrentPaintPos, CurrentPaintPos + FVector(0, 0, 1), FQuat::Identity, CollisionChannel, sphereColl, FCQP, FCRP);
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
					pHitedActor->Destroy();
				}
			}

			for (UPrimitiveComponent* pHitedPrimitiveComponent : ArrHitedComponents)
			{
				AProcGenActor* OwnerProcGen = Cast<AProcGenActor>(pHitedPrimitiveComponent->GetOwner());
				if (OwnerProcGen)
				{
					UStaticMeshComponent* pSM_Comp = Cast<UStaticMeshComponent>(pHitedPrimitiveComponent);
					if (pSM_Comp && !pSM_Comp->IsDefaultSubobject())
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

void UProcGenManager::CreateActorByParams(const FActorToDelayedCreateParams& DelayedCreateParams, UWorld* CurWorldPtr)
{
	if (DelayedCreateParams.bSpawnInEditor)
	{
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
		//GEngine->AddOnScreenDebugMessage((uint64)-1, 8.0f, FColor::Green, FString::Printf(TEXT("UProcGenManager::CreateActorByParams pGeneratedActor name - %s"), *pGeneratedActor->GetName()));
	}
	else
	{
		FActorSpawnParameters SpawnInfo = FActorSpawnParameters();
		SpawnInfo.OverrideLevel = CurWorldPtr->GetCurrentLevel();
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.bHideFromSceneOutliner = true;
		SpawnInfo.bCreateActorPackage = true;
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
	UWorld* pCurEdWorld = GEditor->GetEditorWorldContext().World();
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
