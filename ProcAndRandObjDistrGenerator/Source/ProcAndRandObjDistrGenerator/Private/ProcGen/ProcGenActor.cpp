// Free to copy, edit and use in any work/projects, commercial or free


#include "ProcGen/ProcGenActor.h"
#include "Runtime/Engine/Classes/Components/SplineComponent.h"
#include "..\..\Public\ProcGen\ProcGenActor.h"
#include "Engine/Polys.h"
#include "Engine.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Runtime/Landscape/Classes/Landscape.h"
#include "Runtime/Landscape/Classes/LandscapeInfo.h"
#include "Runtime/Landscape/Classes/LandscapeComponent.h"
#include "Runtime/Landscape/Public/LandscapeEdit.h"
#include "ProcGen/ProcGenManager.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

#include "Components/ArrowComponent.h"

#include "Misc/ScopedSlowTask.h"

static FTransform ZeroTrasf = FTransform();
static bool bInitOnceN600 = false;

AProcGenActor::AProcGenActor() : Super(), 
ProcGenSlotObjectClass(), 
CurCreatedProcGenSlotObject(), 
GeneratedStaticMeshComponents(),
GeneratedDecalsComponents(),
OthersPGAsUsedAsExcludedShapes(),
OthersPGAsUsedAsTIA(),
ShapeLimitActors(),
ShapeBoundingBox(EForceInit::ForceInit),
LinkedGenSlotsUIDs(),
InstancedStaticMeshComponentsMap(),
InstancedStaticMeshComponentsMap2()
{
	PrimaryActorTick.bCanEverTick = false;

	GenerationSplineShape = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));

	GenerationDirectionArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("GenerationDirectionArrowComponent"));
	GenerationDirectionArrowComp->SetArrowColor(FLinearColor::Green);
	GenerationDirectionArrowComp->SetupAttachment(GenerationSplineShape);
	GenerationDirectionArrowComp->SetRelativeLocation(FVector(0, 0, 100));
	GenerationDirectionArrowComp->SetRelativeRotation(FVector(0,0,-1).ToOrientationRotator());
	GenerationDirectionAlignmentArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("GenerationDirectionAlignmentArrowComponent"));
	GenerationDirectionAlignmentArrowComp->SetArrowColor(FLinearColor::Yellow);
	GenerationDirectionAlignmentArrowComp->SetupAttachment(GenerationSplineShape);
	GenerationDirectionAlignmentArrowComp->SetRelativeLocation(FVector(0, 0, 200));
	GenerationDirectionAlignmentArrowComp->SetRelativeRotation(FVector(0, 0, -1).ToOrientationRotator());

	SplineDivideDistanceForTriangulation = 100.0f;
	CurCreatedProcGenSlotObject = nullptr;
	GenerationZOffset = 1000.0f;
	GenerationZExtent = 2000.0f;
	GenerationPower = 1.0f;
	bScaleGenerationPowerBySize = true;
	bIsProcess = false;
	bGenerationOnSpline = false;
	SplineGenThickness = 100.0f;
	bGenerateOnBeginPlay = false;
	TargetLandscapePtr = nullptr;
	bGenerateOnTargetLandscape = false;
	LandscapeGenerationMask = nullptr;
	bSelfCollideWhenGeneration = false;
	bGOSUseNormalAndYawToRotate = false;
	fGOSYawAddOnRotate = 0.0f;
	bUseSplineControlPointsOnlyToGenShape = false;
	bUseGenerationDirectionAC = false;
	bUseGenerationAlignDirectionAC = false;

	GenShapeCenterPoint = FVector::ZeroVector;

	if (!bInitOnceN600)
	{
		ZeroTrasf.SetLocation(FVector(0, 0, -5000000));
		ZeroTrasf.RemoveScaling(0);
		bInitOnceN600 = true;
	}

	CachedParentGridCellId = -1;

	CachedInstMeshCellInfoPointer = nullptr;
	LastSMPtr = nullptr;
}

void AProcGenActor::BeginPlay()
{
	Super::BeginPlay();

	if (bGenerateOnBeginPlay)
	{
		GenerateRequest();
	}
}

void AProcGenActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	SProcessScopeExec ProcessScopeObj(bIsProcess);

	if (CurCreatedProcGenSlotObject.Get())
	{
		CurCreatedProcGenSlotObject.Get()->PrepareToDestroy();
		CurCreatedProcGenSlotObject.Get()->RemoveFromRoot();
		if (CurCreatedProcGenSlotObject.Get()->IsDestructionThreadSafe())
		{
			CurCreatedProcGenSlotObject.Get()->ConditionalBeginDestroy();
		}
		CurCreatedProcGenSlotObject = nullptr;

	}

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (pProcGenManager)
	{
		for (int32 Id : LinkedGenSlotsUIDs)
		{
			pProcGenManager->ClearGenerationGridCellsByGenSlotId(Id);
		}
		LinkedGenSlotsUIDs.Empty();

		int32 ids = pProcGenManager->ProcGenActorsPtrs.Find(this);
		if (ids > -1)
		{
			pProcGenManager->ProcGenActorsPtrs.RemoveAt(ids);
		}
	}

	ClearInstancedSMsAndInsts();
}

void AProcGenActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (pProcGenManager)
	{
		pProcGenManager->ProcGenActorsPtrs.AddUnique(this);
	}

	TArray<FVector> PointsArr = GetSplinePointsForShapeTriagulationWithoutOffset(true);
	if (PointsArr.Num() >= 2)
	{
		FVector Center = FVector::ZeroVector;
		for (FVector& CurPoint : PointsArr)
		{
			Center += (CurPoint * 0.001f);
		}
		Center = (Center / PointsArr.Num()) * 1000.0f;
		GenShapeCenterPoint = Center;
	}
	else
	{
		GenShapeCenterPoint = GetActorLocation();
	}

	ShapeBoundingBox = FBox(PointsArr);
}

void AProcGenActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (InstancedStaticMeshComponentsMap.Num() > 0)
	{
		for (TPair<UStaticMesh*, UInstancedStaticMeshComponent*>& InstCompData : InstancedStaticMeshComponentsMap)
		{
			UHierarchicalInstancedStaticMeshComponent* pMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InstCompData.Value);
			if (pMC)
			{
				if (TreeBuildRecool <= 0.0f)
				{
					if (pMC->IsTreeFullyBuilt() || bNeedTreeRebuild)
					{
						pMC->BuildTreeIfOutdated(true, false);
						TreeBuildRecool = FMath::RandRange(1.5f, 4.0f);
						bNeedTreeRebuild = false;
					}
				}

			}
		}
	}

	if (TreeBuildRecool <= 0.0f)
	{
		for (TPair<UStaticMesh*, FInstMeshCellsData>& InstCompData : InstancedStaticMeshComponentsMap2)
		{
			for (TPair<int32, FInstMeshCellInfo>& CellData : InstCompData.Value.CellsInfo)
			{
				if (CellData.Value.InstancedStaticMeshComponentPtr)
				{
					if (CellData.Value.bNeedTreeRebuild)
					{
						CellData.Value.bNeedTreeRebuild = false;
						CellData.Value.InstancedStaticMeshComponentPtr->BuildTreeIfOutdated(true, false);
					}
				}
			}
		}

		TreeBuildRecool = FMath::RandRange(0.2f, 0.6f);
	}

	if (TreeBuildRecool > 0.0f)
	{
		TreeBuildRecool -= DeltaTime;
	}
}

TArray<FVector> AProcGenActor::GetSplinePointsForShapeTriagulation()
{
	FVector SplinePoint;
	TArray<FVector> PointsArr = TArray<FVector>();
	if (!GenerationSplineShape->IsClosedLoop())
	{
		return PointsArr;
	}
	if (bUseSplineControlPointsOnlyToGenShape)
	{
		if (GenerationSplineShape->GetNumberOfSplinePoints() < 3)
			return PointsArr;

		for (int32 i = 0; i < GenerationSplineShape->GetNumberOfSplinePoints(); ++i)
		{
			SplinePoint = GenerationSplineShape->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			SplinePoint.Z += GenerationZOffset;
			PointsArr.Add(SplinePoint);
		}
		return PointsArr;
	}
	int32 numPoints = int32(GenerationSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
	if (numPoints < 3)
	{
		return PointsArr;
	}
	float totalDist = 0.0f;
	//FVector SplinePoint;
	for (int32 i = 0; i < numPoints; ++i)
	{
		SplinePoint = GenerationSplineShape->GetLocationAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
		SplinePoint.Z += GenerationZOffset;
		PointsArr.Add(SplinePoint);
		totalDist += SplineDivideDistanceForTriangulation;
	}

	return PointsArr;
}

TArray<FVector> AProcGenActor::GetSplinePointsForShapeTriagulationWithoutOffset(bool bSupportNonClosedShape)
{
	FVector SplinePoint;
	TArray<FVector> PointsArr = TArray<FVector>();
	if (!GenerationSplineShape->IsClosedLoop())
	{
		if (!bSupportNonClosedShape)
		{
			return PointsArr;
		}
		else
		{
			if (bUseSplineControlPointsOnlyToGenShape)
			{
				if (GenerationSplineShape->GetNumberOfSplinePoints() < 2)
					return PointsArr;

				for (int32 i = 0; i < GenerationSplineShape->GetNumberOfSplinePoints(); ++i)
				{
					SplinePoint = GenerationSplineShape->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
					PointsArr.Add(SplinePoint);
				}
				return PointsArr;
			}
			int32 numPoints = int32(GenerationSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
			if (numPoints < 2)
			{
				return PointsArr;
			}

			float totalDist = 0.0f;
			for (int32 i = 0; i < numPoints; ++i)
			{
				SplinePoint = GenerationSplineShape->GetLocationAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
				PointsArr.Add(SplinePoint);
				totalDist += SplineDivideDistanceForTriangulation;
			}

			SplinePoint = GenerationSplineShape->GetLocationAtDistanceAlongSpline(GenerationSplineShape->GetSplineLength(), ESplineCoordinateSpace::World);
			PointsArr.Add(SplinePoint);

			return PointsArr;
		}
	}
	if (bUseSplineControlPointsOnlyToGenShape)
	{
		if (GenerationSplineShape->GetNumberOfSplinePoints() < 3)
			return PointsArr;

		for (int32 i = 0; i < GenerationSplineShape->GetNumberOfSplinePoints(); ++i)
		{
			SplinePoint = GenerationSplineShape->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			PointsArr.Add(SplinePoint);
		}
		return PointsArr;
	}
	int32 numPoints = int32(GenerationSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
	if (numPoints < 3)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("AProcGenActor::GetSplinePointsForShapeTriagulationWithoutOffset() return empty PointsArr, error 5"));
		return PointsArr;
	}
	float totalDist = 0.0f;
	//FVector SplinePoint;
	for (int32 i = 0; i < numPoints; ++i)
	{
		SplinePoint = GenerationSplineShape->GetLocationAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
		PointsArr.Add(SplinePoint);
		totalDist += SplineDivideDistanceForTriangulation;
	}

	return PointsArr;
}

TArray<FVector> AProcGenActor::GetSplinePointsForGenOnSpline()
{
	FVector SplinePoint;
	TArray<FVector> PointsArr = TArray<FVector>();
	if (bUseSplineControlPointsOnlyToGenShape)
	{
		if (GenerationSplineShape->GetNumberOfSplinePoints() < 2)
			return PointsArr;

		for (int32 i = 0; i < GenerationSplineShape->GetNumberOfSplinePoints(); ++i)
		{
			SplinePoint = GenerationSplineShape->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			SplinePoint.Z += GenerationZOffset;
			PointsArr.Add(SplinePoint);
		}
		return PointsArr;
	}
	int32 numPoints = int32(GenerationSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
	if (numPoints < 2)
	{
		return PointsArr;
	}
	float totalDist = 0.0f;
	//FVector SplinePoint;
	for (int32 i = 0; i < numPoints; ++i)
	{
		SplinePoint = GenerationSplineShape->GetLocationAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
		SplinePoint.Z += GenerationZOffset;
		PointsArr.Add(SplinePoint);
		totalDist += SplineDivideDistanceForTriangulation;
	}
	//if any
	//if (!GenerationSplineShape->IsClosedLoop())
	{
		SplinePoint = GenerationSplineShape->GetLocationAtDistanceAlongSpline(GenerationSplineShape->GetSplineLength(), ESplineCoordinateSpace::World);
		SplinePoint.Z += GenerationZOffset;
		PointsArr.Add(SplinePoint);
	}

	return PointsArr;
}

TArray<FTransform> AProcGenActor::GetSplineTransformsForGenOnSpline()
{
	FTransform SplineTransf;
	TArray<FTransform> TransfArr = TArray<FTransform>();
	if (bUseSplineControlPointsOnlyToGenShape)
	{
		if(GenerationSplineShape->GetNumberOfSplinePoints() < 2)
			return TransfArr;

		for (int32 i = 0; i < GenerationSplineShape->GetNumberOfSplinePoints(); ++i)
		{
			SplineTransf = GenerationSplineShape->GetTransformAtSplinePoint(i, ESplineCoordinateSpace::World);
			SplineTransf.SetLocation(SplineTransf.GetLocation() + FVector(0, 0, GenerationZOffset));
			TransfArr.Add(SplineTransf);
		}
		return TransfArr;
	}
	int32 numPoints = int32(GenerationSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
	if (numPoints < 2)
	{
		return TransfArr;
	}
	float totalDist = 0.0f;
	//FTransform SplineTransf;
	for (int32 i = 0; i < numPoints; ++i)
	{
		SplineTransf = GenerationSplineShape->GetTransformAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
		SplineTransf.SetLocation(SplineTransf.GetLocation() + FVector(0,0,GenerationZOffset));
		TransfArr.Add(SplineTransf);
		totalDist += SplineDivideDistanceForTriangulation;
	}
	//if any
	//if (!GenerationSplineShape->IsClosedLoop())
	{
		SplineTransf = GenerationSplineShape->GetTransformAtDistanceAlongSpline(GenerationSplineShape->GetSplineLength(), ESplineCoordinateSpace::World);
		SplineTransf.SetLocation(SplineTransf.GetLocation() + FVector(0, 0, GenerationZOffset));
		TransfArr.Add(SplineTransf);
	}

	return TransfArr;
}

void AProcGenActor::FillExludePolys(TArray<FPoly>& PolysArr)
{
	if (OthersPGAsUsedAsExcludedShapes.Num() <= 0)
		return;

	for (AProcGenActor* OthersPGA_Ptr : OthersPGAsUsedAsExcludedShapes)
	{
		if(!OthersPGA_Ptr)
			continue;

		//is spline or closed shape
		if (OthersPGA_Ptr->bGenerationOnSpline)
		{
			TArray<FTransform> TransfPointsArr = OthersPGA_Ptr->GetSplineTransformsForGenOnSpline();
			if (TransfPointsArr.Num() <= 0)
				continue;

			FTransform SplineTransCur;
			FTransform SplineTransNext;
			FVector PolyBoxBorder[4];
			for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
			{
				TransfPointsArr[i].SetLocation(TransfPointsArr[i].GetLocation() - FVector(0, 0, OthersPGA_Ptr->GenerationZOffset));
			}
			for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
			{
				if ((i + 1) < TransfPointsArr.Num())
				{
					SplineTransCur = TransfPointsArr[i];
					SplineTransNext = TransfPointsArr[i + 1];
					PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * OthersPGA_Ptr->SplineGenThickness);
					PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * OthersPGA_Ptr->SplineGenThickness);
					PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * OthersPGA_Ptr->SplineGenThickness);
					PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * OthersPGA_Ptr->SplineGenThickness);
					FPoly PolyShapeNv = FPoly();
					PolyShapeNv.Init();
					for (int32 ic = 0; ic < 4; ++ic)
					{
						PolyShapeNv.InsertVertex(ic, PolyBoxBorder[ic]);
					}

					TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
					PolyShapeNv.Triangulate(nullptr, TrianglesPolyArr);
					for (FPoly& TriangleL : TrianglesPolyArr)
					{
						PolysArr.Add(TriangleL);
					}
					//PolysArr.Add(PolyShapeNv);
				}
			}
		}
		else
		{
			TArray<FVector> MeshPoints = OthersPGA_Ptr->GetSplinePointsForShapeTriagulationWithoutOffset();
			if (MeshPoints.Num() <= 0)
				continue;

			FPoly PolyShapeNv = FPoly();
			PolyShapeNv.Init();
			for (int32 i = 0; i < MeshPoints.Num(); ++i)
			{
				PolyShapeNv.InsertVertex(i, MeshPoints[i]);
			}

			TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
			PolyShapeNv.Triangulate(nullptr, TrianglesPolyArr);
			for (FPoly& TriangleL : TrianglesPolyArr)
			{
				PolysArr.Add(TriangleL);
			}
			//PolysArr.Add(PolyShapeNv);
		}
	}
}

void AProcGenActor::TestBuildPolyShapeToGen()
{
	UWorld* pCurEdWorld = nullptr;//GEditor->GetEditorWorldContext().World();
	UProcGenManager* pPGManager = UProcGenManager::GetCurManager();
	if (pPGManager)
	{
		pCurEdWorld = pPGManager->GetWorldPRFEditor();
	}

	if (bGenerationOnSpline)
	{
		TArray<FTransform> TransfPointsArr = GetSplineTransformsForGenOnSpline();
		if (TransfPointsArr.Num() <= 0)
			return;

		FTransform SplineTransCur;
		FTransform SplineTransNext;
		FVector PolyBoxBorder[4];
		for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
		{
			TransfPointsArr[i].SetLocation(TransfPointsArr[i].GetLocation() - FVector(0, 0, GenerationZOffset));
		}
		for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
		{
			if ((i + 1) < TransfPointsArr.Num())
			{
				SplineTransCur = TransfPointsArr[i];
				SplineTransNext = TransfPointsArr[i + 1];
				PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
				PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
				PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
				PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
				FPoly PolyShapeNv = FPoly();
				PolyShapeNv.Init();
				for (int32 ic = 0; ic < 4; ++ic)
				{
					PolyShapeNv.InsertVertex(ic, PolyBoxBorder[ic]);
				}

				TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
				PolyShapeNv.Triangulate(nullptr, TrianglesPolyArr);
				for (int32 i2 = 0; i2 < TrianglesPolyArr.Num(); ++i2)
				{
					FPoly& CurPly = TrianglesPolyArr[i2];
					FVector OldVert = FVector(0);
					for (int32 ic = 0; ic < CurPly.Vertices.Num(); ++ic)
					{
						if (OldVert.IsZero())
						{
							OldVert = CurPly.Vertices[CurPly.Vertices.Num() - 1];
						}

						UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, CurPly.Vertices[ic], OldVert, FLinearColor::Green, 5.3f);
						OldVert = CurPly.Vertices[ic];
					}
				}
			}
		}

		return;
	}

	TArray<FVector> MeshPoints = GetSplinePointsForShapeTriagulationWithoutOffset();
	if (MeshPoints.Num() <= 0)
		return;

	FPoly PolyShapeNv = FPoly();
	PolyShapeNv.Init();
	for (int32 i = 0; i < MeshPoints.Num(); ++i)
	{
		PolyShapeNv.InsertVertex(i, MeshPoints[i]);
	}
	TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
	PolyShapeNv.Triangulate(nullptr, TrianglesPolyArr);
	for (int32 i = 0; i < TrianglesPolyArr.Num(); ++i)
	{
		FPoly& CurPly = TrianglesPolyArr[i];
		FVector OldVert = FVector(0);
		for (int32 ic = 0; ic < CurPly.Vertices.Num(); ++ic)
		{
			if (OldVert.IsZero())
			{
				OldVert = CurPly.Vertices[CurPly.Vertices.Num()-1];
			}
			
			UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, CurPly.Vertices[ic], OldVert, FLinearColor::Green, 5.3f);
			OldVert = CurPly.Vertices[ic];
		}
	}
}

float AProcGenActor::GetDistanceToShapeEdgeFromPoint(const FVector& Point, bool b2D)
{
	FVector NearestSplinePoint = GenerationSplineShape->FindLocationClosestToWorldLocation(Point, ESplineCoordinateSpace::World);
	if (b2D)
	{
		NearestSplinePoint.Z = Point.Z;
	}
	if (bGenerationOnSpline)
	{
		FVector DirToEdge = Point - NearestSplinePoint;
		DirToEdge.Normalize();
		NearestSplinePoint = NearestSplinePoint + (DirToEdge * SplineGenThickness);
	}
	return FVector::Dist(Point, NearestSplinePoint);
}

float AProcGenActor::GetPercentDistanceToShapeEdgeFromPointToShapeCenter(const FVector& Point, bool b2D)
{
	FVector NearestSplinePoint = GenerationSplineShape->FindLocationClosestToWorldLocation(Point, ESplineCoordinateSpace::World);
	FVector Center = GenShapeCenterPoint;
	if (b2D)
	{
		NearestSplinePoint.Z = Point.Z;
		Center.Z = Point.Z;
	}
	float distToCenter = FVector::Dist(Center, NearestSplinePoint);
	float distToPoint = FVector::Dist(Point, NearestSplinePoint);
	if (!bGenerationOnSpline)
	{
		return distToPoint / distToCenter;
	}
	else
	{
		return distToPoint / SplineGenThickness;
	}
}

float AProcGenActor::GetPercentDistanceToSplineNearestCenterFromPoint(const FVector& Point, bool b2D)
{
	if (SplineGenThickness == 0.0f)
		return 0.0f;

	FVector NearestSplinePoint = GenerationSplineShape->FindLocationClosestToWorldLocation(Point, ESplineCoordinateSpace::World);
	if (b2D)
	{
		NearestSplinePoint.Z = Point.Z;
	}
	float distToPoint = FVector::Dist(Point, NearestSplinePoint);
	return distToPoint / SplineGenThickness;
}

float AProcGenActor::GetDistanceToExcludedShapes(const FVector& PointPos)
{
	TArray<float> ArrValues = TArray<float>();
	for (AProcGenActor* OthersPGA_Ptr : OthersPGAsUsedAsExcludedShapes)
	{
		if (!OthersPGA_Ptr)
			continue;

		float distTo = OthersPGA_Ptr->GetDistanceToShapeEdgeFromPoint(PointPos, true);
		if ((distTo / 500.0f) <= 1.0f)
		{
			ArrValues.Add(distTo);
		}
	}

	if (ArrValues.Num() > 1)
	{
		ArrValues.Sort([](const float Left, const float Right) -> bool
			{
				return Left > Right;
			});

		return ArrValues.Last();
	}
	else if (ArrValues.Num() > 0)
	{
		return ArrValues[0];
	}

	return 10000.0f;
}

void AProcGenActor::GenerateRequest()
{
	if (!ProcGenSlotObjectClass.Get())
		return;

	SProcessScopeExec ProcessScopeObj(bIsProcess);

	FVector GenDirDef = bUseGenerationDirectionAC ? GenerationDirectionArrowComp->GetComponentRotation().Vector() : FVector::ZeroVector;
	FVector AlignDirDef = bUseGenerationAlignDirectionAC ? GenerationDirectionAlignmentArrowComp->GetComponentRotation().Vector() : FVector::ZeroVector;

	if (!CurCreatedProcGenSlotObject.Get())
	{
		CurCreatedProcGenSlotObject = NewObject<UPGSObj>(this, ProcGenSlotObjectClass.Get());
		CurCreatedProcGenSlotObject.Get()->AddToRoot();
	}
	else
	{
		CurCreatedProcGenSlotObject.Get()->PrepareToDestroy();
		CurCreatedProcGenSlotObject.Get()->RemoveFromRoot();
		if (CurCreatedProcGenSlotObject.Get()->IsDestructionThreadSafe())
		{
			CurCreatedProcGenSlotObject.Get()->ConditionalBeginDestroy();
		}
		CurCreatedProcGenSlotObject = nullptr;

		CurCreatedProcGenSlotObject = NewObject<UPGSObj>(this, ProcGenSlotObjectClass.Get());
		CurCreatedProcGenSlotObject.Get()->AddToRoot();
	}

	ClearAttachedSMs();
	ClearAttachedDecals();

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (pProcGenManager)
	{
		for (int32 Id : LinkedGenSlotsUIDs)
		{
			pProcGenManager->ClearGenerationGridCellsByGenSlotId(Id);
		}
		LinkedGenSlotsUIDs.Empty();

		pProcGenManager->ClearGeneratorsCache();
	}

	CurCreatedProcGenSlotObject.Get()->PrepareToGeneration();

	if (bGenerateOnTargetLandscape && TargetLandscapePtr)
	{
		GenerateOnTargetLandscape();
		return;
	}

	UWorld* pCurEdWorld = nullptr;// GEditor->GetEditorWorldContext().World();
	UProcGenManager* pPGManager = UProcGenManager::GetCurManager();
	if (pPGManager)
	{
		pCurEdWorld = pPGManager->GetWorldPRFEditor();
	}
	TArray<FPoly> ExcludedPls = TArray<FPoly>();
	FillExludePolys(ExcludedPls);

	if (bGenerationOnSpline)
	{
		TArray<FTransform> TransfPointsArr = GetSplineTransformsForGenOnSpline();
		if (TransfPointsArr.Num() <= 0)
			return;

		FString STaskMsgTextStr = "";
		FScopedSlowTask GSTTargetSpline(100.0f, FText::FromString("Procedural Generation On Spline started"));
		GSTTargetSpline.MakeDialog();

		FTransform SplineTransCur;
		FTransform SplineTransNext;
		FVector PolyBoxBorder[4];
		for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
		{
			if ((i + 1) < TransfPointsArr.Num())
			{
				STaskMsgTextStr = FString::Printf(TEXT("Left spline Segments to generate - %i"), TransfPointsArr.Num() - i);
				GSTTargetSpline.CurrentFrameScope = 0.0f;
				GSTTargetSpline.CompletedWork = (float(i) / float(TransfPointsArr.Num())) * 100.0f;
				GSTTargetSpline.EnterProgressFrame(0.0f, FText::FromString(STaskMsgTextStr));

				SplineTransCur = TransfPointsArr[i];
				SplineTransNext = TransfPointsArr[i + 1];
				PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
				PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
				PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
				PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
				FPoly PolyShapeNv = FPoly();
				PolyShapeNv.Init();
				TArray<FVector> MeshPoints = TArray<FVector>();
				MeshPoints.Reserve(5);
				for (int32 ic = 0; ic < 4; ++ic)
				{
					PolyShapeNv.InsertVertex(ic, PolyBoxBorder[ic]);
					MeshPoints.Add(PolyBoxBorder[ic]);
				}

				if (GenerationZExtent != 0.0f)
				{
					MeshPoints.Add(MeshPoints[0] + FVector(0, 0, GenerationZExtent));
				}

				float fGenPowScaled = GenerationPower;
				if (bScaleGenerationPowerBySize)
				{
					FBox NvBox = FBox(MeshPoints);
					FVector BoxSize = NvBox.GetSize();
					//BoxSize.Z = 0.0f;
					float boxSzLngt = BoxSize.Size2D();
					float PercentSz = boxSzLngt / 5000.0f;
					fGenPowScaled *= PercentSz;
				}

				if (!bGOSUseNormalAndYawToRotate)
				{
					CurCreatedProcGenSlotObject.Get()->RequestGenerateInBBoxWithShapeBorder(MeshPoints, pCurEdWorld, fGenPowScaled, PolyShapeNv, ExcludedPls, this, GenDirDef, AlignDirDef);
				}
				else
				{
					FVector DirToSplnPt = SplineTransCur.GetLocation() - SplineTransNext.GetLocation();
					DirToSplnPt.Normalize();
					float YAWAdd = fGOSYawAddOnRotate + DirToSplnPt.ToOrientationRotator().Yaw;
					if (YAWAdd > 360.0f)
					{
						YAWAdd -= 360.0f;
					}
					DirToSplnPt = DirToSplnPt.ToOrientationQuat() * FVector(0, 0, 1);
					DirToSplnPt.Normalize();
					CurCreatedProcGenSlotObject.Get()->RequestGenerateInBBoxWithShapeBorder(MeshPoints, pCurEdWorld, fGenPowScaled, PolyShapeNv, ExcludedPls, this, GenDirDef, DirToSplnPt, 0.0f, false, YAWAdd);
				}
			}
		}
	}
	else
	{
		TArray<FVector> MeshPoints = GetSplinePointsForShapeTriagulation();
		if (MeshPoints.Num() <= 0)
			return;

		FPoly PolyShapeNv = FPoly();
		PolyShapeNv.Init();
		for (int32 i = 0; i < MeshPoints.Num(); ++i)
		{
			PolyShapeNv.InsertVertex(i, MeshPoints[i]);
		}

		if (GenerationZExtent != 0.0f)
		{
			MeshPoints.Add(MeshPoints[0] + FVector(0, 0, GenerationZExtent));
		}

		float fGenPowScaled = GenerationPower;
		if (bScaleGenerationPowerBySize)
		{
			FBox NvBox = FBox(MeshPoints);
			FVector BoxSize = NvBox.GetSize();
			//BoxSize.Z = 0.0f;
			float boxSzLngt = BoxSize.Size2D();
			float PercentSz = boxSzLngt / 5000.0f;
			fGenPowScaled *= PercentSz;
		}

		CurCreatedProcGenSlotObject.Get()->RequestGenerateInBBoxWithShapeBorder(MeshPoints, pCurEdWorld, fGenPowScaled, PolyShapeNv, ExcludedPls, this, GenDirDef, AlignDirDef);

		//FBox NvBox = FBox(MeshPoints);
		//UKismetSystemLibrary::DrawDebugBox(pCurEdWorld, NvBox.GetCenter(), NvBox.GetExtent(), FLinearColor::Red, FRotator::ZeroRotator, 15.0f, 1.0f);
	}
}

void AProcGenActor::ClearPreviousRequest()
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (CurCreatedProcGenSlotObject.Get())
	{
		SProcessScopeExec ProcessScopeObj(bIsProcess);

		CurCreatedProcGenSlotObject.Get()->PrepareToDestroy();
		CurCreatedProcGenSlotObject.Get()->RemoveFromRoot();
		if (CurCreatedProcGenSlotObject.Get()->IsDestructionThreadSafe())
		{
			CurCreatedProcGenSlotObject.Get()->ConditionalBeginDestroy();
		}
		CurCreatedProcGenSlotObject = nullptr;

		ClearAttachedSMs();
		ClearAttachedDecals();
		if (pProcGenManager)
		{
			for (int32 Id : LinkedGenSlotsUIDs)
			{
				pProcGenManager->ClearGenerationGridCellsByGenSlotId(Id);
			}
			LinkedGenSlotsUIDs.Empty();

			pProcGenManager->ClearGeneratorsCache();
		}

		ClearInstancedSMsAndInsts();
	}
	else
	{
		if (pProcGenManager)
		{
			for (int32 Id : LinkedGenSlotsUIDs)
			{
				pProcGenManager->ClearGenerationGridCellsByGenSlotId(Id);
			}
			LinkedGenSlotsUIDs.Empty();

			pProcGenManager->ClearGeneratorsCache();
		}

		ClearInstancedSMsAndInsts();
	}

}

void AProcGenActor::ClearAttachedProceduralComponentsRequest()
{
	ClearInstancedSMsAndInsts();

	GeneratedDecalsComponents.Empty();
	GeneratedStaticMeshComponents.Empty();

	const TSet<UActorComponent*> ActComponents = GetComponents();
	for (UActorComponent* pActComp : ActComponents)
	{
		if (!pActComp)
			continue;

		if (!pActComp->IsDefaultSubobject() && (Cast<UStaticMeshComponent>(pActComp) || Cast<UDecalComponent>(pActComp)))
		{
			USceneComponent* pScComp = Cast<USceneComponent>(pActComp);
			if (pScComp)
			{
				pScComp->DetachFromParent();
				pScComp->UnregisterComponent();
				pScComp->RemoveFromRoot();
				pScComp->ConditionalBeginDestroy();
			}
		}
	}
}

void AProcGenActor::FreezeResultsRequest()
{
	SProcessScopeExec ProcessScopeObj(bIsProcess);

	if (CurCreatedProcGenSlotObject.Get())
	{
		for (FProcGenSlotParams& ProcGenSlot : CurCreatedProcGenSlotObject.Get()->GenerationParamsArr)
		{
			if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
			{
				for (AActor* pGenActor : ProcGenSlot.GeneratedActorsPtrs)
				{
					pGenActor->OnDestroyed.RemoveDynamic(this, &AProcGenActor::OnLinkedActorDestroyed);
				}
			}
		}
		CurCreatedProcGenSlotObject.Get()->FreezeCreatedObjectsInEditor();
		CurCreatedProcGenSlotObject.Get()->RemoveFromRoot();
		if (CurCreatedProcGenSlotObject.Get()->IsDestructionThreadSafe())
		{
			CurCreatedProcGenSlotObject.Get()->ConditionalBeginDestroy();
		}
		CurCreatedProcGenSlotObject = nullptr;
	}

	if (GeneratedStaticMeshComponents.Num() > 0)
	{
		GeneratedStaticMeshComponents.Empty();
	}

	if (GeneratedDecalsComponents.Num() > 0)
	{
		GeneratedDecalsComponents.Empty();
	}
}

static TWeakObjectPtr<UObject> TestWPTR = nullptr;
void AProcGenActor::StartTest1Request()
{
	if (TestWPTR.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Test1 WPTR Failure!"));
	}
	TestWPTR = NewObject<UObject>();
	if (TestWPTR.Get())
	{
		TestWPTR.Get()->AddToRoot();
		TestWPTR.Get()->RemoveFromRoot();
		//TestWPTR.Get()->ConditionalBeginDestroy();
		TestWPTR.Get()->MarkPendingKill();
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Test1 Started, press this button again to get result, you see message if it failure"));
	}
}

void AProcGenActor::DrawGenerationDebugInfo()
{
	if (!GEditor)
		return;

	UWorld* pCurEdWorld = nullptr;// GEditor->GetEditorWorldContext().World();
	UProcGenManager* pPGManager = UProcGenManager::GetCurManager();
	if (pPGManager)
	{
		pCurEdWorld = pPGManager->GetWorldPRFEditor();
	}

	UKismetSystemLibrary::DrawDebugSphere(pCurEdWorld, GenShapeCenterPoint, 200.0f, 12, FLinearColor::Yellow, 15.0f, 2.0f);

	if (bGenerationOnSpline)
	{
		{
			TArray<FTransform> TransfPointsArr = GetSplineTransformsForGenOnSpline();
			if (TransfPointsArr.Num() <= 0)
				return;

			FTransform SplineTransCur;
			FTransform SplineTransNext;
			FVector PolyBoxBorder[4];
			for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
			{
				TransfPointsArr[i].SetLocation(TransfPointsArr[i].GetLocation() - FVector(0, 0, GenerationZOffset));
			}
			for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
			{
				if ((i + 1) < TransfPointsArr.Num())
				{
					SplineTransCur = TransfPointsArr[i];
					SplineTransNext = TransfPointsArr[i + 1];
					PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
					PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
					PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
					PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
					FPoly PolyShapeNv = FPoly();
					PolyShapeNv.Init();
					for (int32 ic = 0; ic < 4; ++ic)
					{
						PolyShapeNv.InsertVertex(ic, PolyBoxBorder[ic]);
					}

					TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
					PolyShapeNv.Triangulate(nullptr, TrianglesPolyArr);
					for (int32 i2 = 0; i2 < TrianglesPolyArr.Num(); ++i2)
					{
						FPoly& CurPly = TrianglesPolyArr[i2];
						FVector OldVert = FVector(0);
						for (int32 ic = 0; ic < CurPly.Vertices.Num(); ++ic)
						{
							if (OldVert.IsZero())
							{
								OldVert = CurPly.Vertices[CurPly.Vertices.Num() - 1];
							}

							UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, CurPly.Vertices[ic], OldVert, FLinearColor::Green, 5.3f);
							OldVert = CurPly.Vertices[ic];
						}
					}
				}
			}
		}

		{
			TArray<FTransform> TransfPointsArr = GetSplineTransformsForGenOnSpline();
			if (TransfPointsArr.Num() <= 0)
				return;

			FTransform SplineTransCur;
			FTransform SplineTransNext;
			FVector PolyBoxBorder[4];

			for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
			{
				if ((i + 1) < TransfPointsArr.Num())
				{
					SplineTransCur = TransfPointsArr[i];
					SplineTransNext = TransfPointsArr[i + 1];
					PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
					PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
					PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
					PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);

					TArray<FVector> MeshPoints = TArray<FVector>();
					MeshPoints.Reserve(5);

					for (int32 ic = 0; ic < 4; ++ic)
					{
						MeshPoints.Add(PolyBoxBorder[ic]);
					}

					if (GenerationZExtent != 0.0f)
					{
						MeshPoints.Add(MeshPoints[0] + FVector(0, 0, GenerationZExtent));
					}

					FBox NvBox = FBox(MeshPoints);
					UKismetSystemLibrary::DrawDebugBox(pCurEdWorld, NvBox.GetCenter(), NvBox.GetExtent(), FLinearColor::Red, FRotator::ZeroRotator, 15.0f, 1.0f);
				}
			}
		}

		return;
	}

	{
		TArray<FVector> MeshPoints = GetSplinePointsForShapeTriagulationWithoutOffset();
		if (MeshPoints.Num() <= 0)
			return;

		FPoly PolyShapeNv = FPoly();
		PolyShapeNv.Init();
		for (int32 i = 0; i < MeshPoints.Num(); ++i)
		{
			PolyShapeNv.InsertVertex(i, MeshPoints[i]);
		}
		TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
		PolyShapeNv.Triangulate(nullptr, TrianglesPolyArr);
		for (int32 i = 0; i < TrianglesPolyArr.Num(); ++i)
		{
			FPoly& CurPly = TrianglesPolyArr[i];
			FVector OldVert = FVector(0);
			for (int32 ic = 0; ic < CurPly.Vertices.Num(); ++ic)
			{
				if (OldVert.IsZero())
				{
					OldVert = CurPly.Vertices[CurPly.Vertices.Num() - 1];
				}

				UKismetSystemLibrary::DrawDebugLine(pCurEdWorld, CurPly.Vertices[ic], OldVert, FLinearColor::Green, 5.3f);
				OldVert = CurPly.Vertices[ic];
			}
		}
	}

	{
		TArray<FVector> MeshPoints = GetSplinePointsForShapeTriagulation();
		if (MeshPoints.Num() <= 0)
			return;

		if (GenerationZExtent != 0.0f)
		{
			MeshPoints.Add(MeshPoints[0] + FVector(0, 0, GenerationZExtent));
		}

		FBox NvBox = FBox(MeshPoints);
		UKismetSystemLibrary::DrawDebugBox(pCurEdWorld, NvBox.GetCenter(), NvBox.GetExtent(), FLinearColor::Red, FRotator::ZeroRotator, 15.0f, 1.0f);
	}
}

void AProcGenActor::ClearingProcGenActorsInRangeRequest()
{
	if (!UProcGenManager::GetCurManager())
		return;

	TArray<FPoly> ArrShapesAll = TArray<FPoly>();
	if (bGenerationOnSpline)
	{
		TArray<FTransform> TransfPointsArr = GetSplineTransformsForGenOnSpline();
		if (TransfPointsArr.Num() <= 0)
			return;

		FTransform SplineTransCur;
		FTransform SplineTransNext;
		FVector PolyBoxBorder[4];
		for (int32 i = 0; i < TransfPointsArr.Num(); ++i)
		{
			if ((i + 1) < TransfPointsArr.Num())
			{
				SplineTransCur = TransfPointsArr[i];
				SplineTransNext = TransfPointsArr[i + 1];
				PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
				PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
				PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * SplineGenThickness);
				PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * SplineGenThickness);
				FPoly PolyShapeNv = FPoly();
				PolyShapeNv.Init();
				TArray<FVector> MeshPoints = TArray<FVector>();
				MeshPoints.Reserve(5);
				for (int32 ic = 0; ic < 4; ++ic)
				{
					PolyShapeNv.InsertVertex(ic, PolyBoxBorder[ic]);
				}
				ArrShapesAll.Add(PolyShapeNv);
			}
		}
	}
	else
	{
		TArray<FVector> MeshPoints = GetSplinePointsForShapeTriagulation();
		if (MeshPoints.Num() <= 0)
			return;

		FPoly PolyShapeNv = FPoly();
		PolyShapeNv.Init();
		for (int32 i = 0; i < MeshPoints.Num(); ++i)
		{
			PolyShapeNv.InsertVertex(i, MeshPoints[i]);
		}
		ArrShapesAll.Add(PolyShapeNv);
	}
	TArray<FPoly> ArrTrianglesAll = TArray<FPoly>();
	for (FPoly& CurPlg : ArrShapesAll)
	{
		TArray<FPoly> TrianglesPolyArr = TArray<FPoly>();
		CurPlg.Triangulate(nullptr, TrianglesPolyArr);
		ArrTrianglesAll += TrianglesPolyArr;
	}

	TArray<AActor*> ActorsList = TArray<AActor*>();
	ActorsList.Reserve(10000);
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), ActorsList);
	for (AActor* pActor : ActorsList)
	{
		if (!pActor)
			continue;

		if (UProcGenManager::GetCurManager()->IsActorCreatedByGenerator(pActor))
		{
			FVector ActorLoc = pActor->GetActorLocation();
			FVector LineStartPos = ActorLoc + FVector(0, 0, 6000000);
			FVector LineEndPos = ActorLoc - FVector(0, 0, 6000000);
			if (UPGSObj::LinePolyIntersection(ArrTrianglesAll, LineStartPos, LineEndPos))
			{
				pActor->Destroy();
			}
		}
	}
}

void AProcGenActor::GenerateOnTargetLandscape()
{
	if (!TargetLandscapePtr)
		return;

	UWorld* pCurEdWorld = TargetLandscapePtr->GetWorld();
	if (!pCurEdWorld)
		return;

	ULandscapeInfo* landInf = TargetLandscapePtr->GetLandscapeInfo();
	if (!landInf)
		return;

	int32 MinX, MinY, MaxX, MaxY;
	FVector LandPos = TargetLandscapePtr->GetActorLocation();
	if (!landInf->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		return;

	TArray<FVector> MeshPoints = GetSplinePointsForShapeTriagulation();
	if (MeshPoints.Num() <= 0)
		return;

	FVector GenDirDef = bUseGenerationDirectionAC ? GenerationDirectionArrowComp->GetComponentRotation().Vector() : FVector::ZeroVector;
	FVector AlignDirDef = bUseGenerationAlignDirectionAC ? GenerationDirectionAlignmentArrowComp->GetComponentRotation().Vector() : FVector::ZeroVector;

	FPoly PolyShapeNv = FPoly();
	PolyShapeNv.Init();
	for (int32 i = 0; i < MeshPoints.Num(); ++i)
	{
		PolyShapeNv.InsertVertex(i, MeshPoints[i]);
	}

	TArray<FPoly> ExcludedPls = TArray<FPoly>();
	FillExludePolys(ExcludedPls);

	float BoxSizeBB = 1.0f;
	FVector CellCoords;
	float ZExt = GenerationZExtent * 0.01f;

	FString STaskMsgTextStr = "";//FString::Printf(TEXT("Generate On Target Landscape started"));
	FScopedSlowTask GSTTargetLandscape(100.0f, FText::FromString("Procedural Generation On Target Landscape started"));

	GSTTargetLandscape.MakeDialog();

	if (LandscapeGenerationMask)
	{
		TArray<FLinearColor> ColorArr = TArray<FLinearColor>();
		FTexture2DMipMap* BitmapMipMap = &LandscapeGenerationMask->PlatformData->Mips[0];
		FByteBulkData* RawImageData = &BitmapMipMap->BulkData;
		FColor* FormatedImageData = reinterpret_cast<FColor*>(RawImageData->Lock(LOCK_READ_ONLY));
		uint32 TextureWidth = LandscapeGenerationMask->GetSizeX(), TextureHeight = LandscapeGenerationMask->GetSizeY();
		FColor PixelColor = FColor();
		float scaleCoof = float(MaxX > 0 ? MaxX : 1) / float(TextureWidth > 0 ? TextureWidth : 1);
		BoxSizeBB *= scaleCoof;
		if (RawImageData->GetBulkDataSize() < (TextureHeight * TextureWidth))
		{
			RawImageData->Unlock();
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation interrupted! Generation ERROR C2"));
			return;
		}

		for (uint32 y = 0; y < TextureHeight; ++y)
		{
			for (uint32 x = 0; x < TextureWidth; ++x)
			{
				PixelColor = FormatedImageData[(y * TextureWidth) + x];
				FLinearColor LColor = FLinearColor::FromSRGBColor(PixelColor);
				ColorArr.Add(LColor);
			}
		}

		RawImageData->Unlock();
		FLinearColor LColor;
		if (ColorArr.Num() < int32(TextureHeight * TextureWidth))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Generation interrupted! Generation ERROR C3"));
			return;
		}
		uint32 NextSegmentNum = 0;
		uint32 NextSegmentIter = 0;
		for (uint32 y = 0; y < TextureHeight; ++y)
		{
			for (uint32 x = 0; x < TextureWidth; ++x)
			{
				if (NextSegmentNum >= NextSegmentIter)
				{
					NextSegmentNum += 128;
					STaskMsgTextStr = FString::Printf(TEXT("Left landscape Segments to generate - %i"), ColorArr.Num() - NextSegmentIter);
					GSTTargetLandscape.CurrentFrameScope = 0.0f;
					GSTTargetLandscape.CompletedWork = ((float(NextSegmentIter) / float(ColorArr.Num())) * 90.0f) + 10.0f;
					GSTTargetLandscape.EnterProgressFrame(0.0f, FText::FromString(STaskMsgTextStr));
				}
				++NextSegmentIter;

				MeshPoints.Empty();

				LColor = ColorArr[(y * TextureWidth) + x];

				FVector ChkLoc = FVector(float(x) * scaleCoof, float(y) * scaleCoof, ZExt);

				CellCoords.X = ChkLoc.X;
				CellCoords.Y = ChkLoc.Y;
				CellCoords.Z = ZExt;
				CellCoords = TargetLandscapePtr->GetTransform().TransformPosition(CellCoords);

				MeshPoints.Add(CellCoords);

				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f)));
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(0.0f, BoxSizeBB, 0.0f)));
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f) + FVector(0.0f, BoxSizeBB, 0.0f)));
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(0.0f, 0.0f, BoxSizeBB)));

				float fGenPowScaled = GenerationPower;
				if (bScaleGenerationPowerBySize)
				{
					FBox NvBox = FBox(MeshPoints);
					FVector BoxSize = NvBox.GetSize();
					float boxSzLngt = BoxSize.Size2D();
					float PercentSz = boxSzLngt / 5000.0f;
					fGenPowScaled *= PercentSz;
					//UKismetSystemLibrary::DrawDebugBox(TargetLandscapePtr->GetWorld(), NvBox.GetCenter(), NvBox.GetExtent(), FLinearColor::Red, FRotator::ZeroRotator, 15.0f, 1.0f);
				}
				//UKismetSystemLibrary::DrawDebugLine(GetWorld(), CellCoords, CellCoords + FVector(0, 0, 5000), LColor, 15.3f, 50.0f);
				fGenPowScaled *= ((LColor.R + LColor.G + LColor.B) / 3.0f);

				CurCreatedProcGenSlotObject.Get()->RequestGenerateInBBoxWithShapeBorder(MeshPoints, pCurEdWorld, fGenPowScaled, PolyShapeNv, ExcludedPls, this, GenDirDef, AlignDirDef);
			}
		}
	}
	else
	{
		uint32 NextSegmentNum = 0;
		uint32 NextSegmentIter = 0;
		//FVector CellCoords;
		for (int32 y = 0; y < MaxY; ++y)
		{
			for (int32 x = 0; x < MaxX; ++x)
			{
				if (NextSegmentNum >= NextSegmentIter)
				{
					NextSegmentNum += 128;
					STaskMsgTextStr = FString::Printf(TEXT("Left landscape Segments to generate(WO LandscapeGenerationMask) - %i"), uint32(MaxX + MaxY) - NextSegmentIter);
					GSTTargetLandscape.CurrentFrameScope = 0.0f;
					GSTTargetLandscape.CompletedWork = ((float(NextSegmentIter) / float(uint32(MaxX + MaxY))) * 90.0f) + 10.0f;
					GSTTargetLandscape.EnterProgressFrame(0.0f, FText::FromString(STaskMsgTextStr));
				}
				++NextSegmentIter;

				if (uint32(MaxX + MaxY) < (NextSegmentIter + 128))
				{
					//infinite loop detected
					break;
				}

				MeshPoints.Empty();

				CellCoords.X = x;
				CellCoords.Y = y;
				CellCoords.Z = ZExt;
				CellCoords = TargetLandscapePtr->GetTransform().TransformPosition(CellCoords);
				//UKismetSystemLibrary::DrawDebugLine(GetWorld(), CellCoords, CellCoords + FVector(0,0,65535), FLinearColor::Green, 15.3f);

				MeshPoints.Add(CellCoords);

				FVector ChkLoc = FVector(x, y, ZExt);
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f)));
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(0.0f, BoxSizeBB, 0.0f)));
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(BoxSizeBB, 0.0f, 0.0f) + FVector(0.0f, BoxSizeBB, 0.0f)));
				MeshPoints.Add(TargetLandscapePtr->GetTransform().TransformPosition(ChkLoc + FVector(0.0f, 0.0f, BoxSizeBB)));

				float fGenPowScaled = GenerationPower;
				if (bScaleGenerationPowerBySize)
				{
					FBox NvBox = FBox(MeshPoints);
					FVector BoxSize = NvBox.GetSize();
					//BoxSize.Z = 0.0f;
					float boxSzLngt = BoxSize.Size2D();
					float PercentSz = boxSzLngt / 5000.0f;
					fGenPowScaled *= PercentSz;
				}

				CurCreatedProcGenSlotObject.Get()->RequestGenerateInBBoxWithShapeBorder(MeshPoints, pCurEdWorld, fGenPowScaled, PolyShapeNv, ExcludedPls, this, GenDirDef, AlignDirDef);
			}
		}
	}
}

void AProcGenActor::OnLinkedActorDestroyed(AActor* DestroyedActor)
{
	if (CurCreatedProcGenSlotObject.Get() && !bIsProcess)
	{
		for (FProcGenSlotParams& ProcGenSlot : CurCreatedProcGenSlotObject.Get()->GenerationParamsArr)
		{
			if (ProcGenSlot.GeneratedActorsPtrs.Num() > 0)
			{
				int32 Id = ProcGenSlot.GeneratedActorsPtrs.Find(DestroyedActor);
				if (Id >= 0)
				{
					ProcGenSlot.GeneratedActorsPtrs.RemoveAt(Id);
					break;
				}
			}
		}
	}
}
static FName NoCollisionProf(TEXT("NoCollision"));

UStaticMeshComponent* AProcGenActor::CreateNewSMComponent(UStaticMesh* pSM, const FTransform& SMTrasf, FStaticMeshCollisionSetup* StaticMeshCollisionSetupPtr, FStaticMeshRenderingOverrideSetup* StaticMeshRenderingOverrideSetupPtr, bool bIsSimple)
{
	if (!pSM)
		return nullptr;

	if (bIsSimple)
	{
		//UInstancedStaticMeshComponent::AddInstanceInternal()
		//UInstancedStaticMeshComponent::AddInstance()
		//UInstancedStaticMeshComponent::RemoveInstance()
		//UInstancedStaticMeshComponent::GetInstanceCount()
		//UInstancedStaticMeshComponent::SetStaticMesh(pSM)
		UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		StaticMeshComponent->Mobility = EComponentMobility::Static;
		StaticMeshComponent->bSelectable = false;
		StaticMeshComponent->bHasPerInstanceHitProxies = false;
		StaticMeshComponent->bAffectDistanceFieldLighting = false;
		StaticMeshComponent->SetCollisionProfileName(NoCollisionProf);
		StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);//bDisableCollision = true;
		StaticMeshComponent->SetCanEverAffectNavigation(false);

		StaticMeshComponent->SetStaticMesh(pSM);

		StaticMeshComponent->RegisterComponent();
		StaticMeshComponent->AttachToComponent(GenerationSplineShape, FAttachmentTransformRules::KeepRelativeTransform);
		StaticMeshComponent->SetWorldTransform(SMTrasf);
		
		//StaticMeshComponent->SetVisibility(true);
		//StaticMeshComponent->SetWorldTransform(SMTrasf);
		//StaticMeshComponent->GetDistanceToCollision()
		GeneratedStaticMeshComponents.Add(StaticMeshComponent);
		return StaticMeshComponent;
	}

	//UStaticMeshComponent* NewStaticMeshComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), GetOwner(), NAME_None, RF_Transient);
	UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, NAME_None/*FName(TEXT("123"))*/, RF_Transactional);//ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), this, FName(TEXT("123")));//NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional);
	StaticMeshComponent->RegisterComponent();
	StaticMeshComponent->SetWorldTransform(SMTrasf);
	StaticMeshComponent->AttachToComponent(GenerationSplineShape, FAttachmentTransformRules::KeepWorldTransform);
	AddInstanceComponent(StaticMeshComponent);
	StaticMeshComponent->SetStaticMesh(pSM);
	StaticMeshComponent->RegisterComponent();
	StaticMeshComponent->SetActive(true);
	StaticMeshComponent->SetVisibility(true);
	StaticMeshComponent->SetWorldTransform(SMTrasf);
	//StaticMeshComponent->GetDistanceToCollision()
	GeneratedStaticMeshComponents.Add(StaticMeshComponent);
	if (StaticMeshCollisionSetupPtr)
	{
		StaticMeshComponent->BodyInstance.SetCollisionProfileName(StaticMeshCollisionSetupPtr->collisionProfile);
		StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::Type(StaticMeshCollisionSetupPtr->collisionEnable));
	}

	if (StaticMeshRenderingOverrideSetupPtr)
	{
		if (StaticMeshRenderingOverrideSetupPtr->OverrideMinimalLOD > -1)
		{
			StaticMeshComponent->bOverrideMinLOD = true;
			StaticMeshComponent->MinLOD = StaticMeshRenderingOverrideSetupPtr->OverrideMinimalLOD;
		}

		if (StaticMeshRenderingOverrideSetupPtr->DesiredDrawDistance > -1.0f)
		{
			StaticMeshComponent->LDMaxDrawDistance = StaticMeshRenderingOverrideSetupPtr->DesiredDrawDistance;
		}
	}

	//UKismetSystemLibrary::DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + FVector(FMath::RandRange(-1000.0f, 1000.0f), FMath::RandRange(-1000.0f, 1000.0f), 10000), FLinearColor::Green, 15.3f);
	return StaticMeshComponent;
}

void AProcGenActor::RemoveSMComp(UStaticMeshComponent* pStaticMeshComponent)
{
	GeneratedStaticMeshComponents.RemoveSwap(pStaticMeshComponent);

	if (pStaticMeshComponent)
	{
		RemoveInstanceComponent(pStaticMeshComponent);
		pStaticMeshComponent->DetachFromParent();
		pStaticMeshComponent->UnregisterComponent();
		pStaticMeshComponent->RemoveFromRoot();
		pStaticMeshComponent->ConditionalBeginDestroy();
	}
}

UDecalComponent* AProcGenActor::CreateNewDecalComponent(UMaterialInterface* DecalMaterial, const FTransform& DecalTrasf, const FVector& DecalScale, FDecalRenderingOverrideSetup* DecalRenderingOverrideSetupPtr)
{
	UDecalComponent* pDecalComponent = NewObject<UDecalComponent>(this, NAME_None, RF_Transactional);
	pDecalComponent->RegisterComponent();
	pDecalComponent->SetWorldTransform(DecalTrasf);
	pDecalComponent->AttachToComponent(GenerationSplineShape, FAttachmentTransformRules::KeepWorldTransform);
	AddInstanceComponent(pDecalComponent);
	pDecalComponent->SetDecalMaterial(DecalMaterial);
	if (!DecalScale.IsZero())
	{
		pDecalComponent->DecalSize = DecalScale;
	}
	//pDecalComponent->S
	pDecalComponent->RegisterComponent();
	pDecalComponent->SetActive(true);
	pDecalComponent->SetVisibility(true);
	pDecalComponent->SetWorldTransform(DecalTrasf);
	pDecalComponent->SetRelativeRotation(pDecalComponent->GetRelativeRotation() + FRotator(-90, 0, 0));

	if (DecalRenderingOverrideSetupPtr)
	{
		if (DecalRenderingOverrideSetupPtr->FadeScreenSize > -1.0f)
			pDecalComponent->FadeScreenSize = DecalRenderingOverrideSetupPtr->FadeScreenSize;
	}

	GeneratedDecalsComponents.Add(pDecalComponent);

	return pDecalComponent;
}

void AProcGenActor::ClearAttachedSMs()
{
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (GeneratedStaticMeshComponents.Num() > 0)
	{
		for (UStaticMeshComponent* StaticMeshComponent : GeneratedStaticMeshComponents)
		{
			if (StaticMeshComponent)
			{
				//temp--------------------------------------------------------------------------------------------------------
				/*if (pProcGenManager)
				{
					pProcGenManager->ClearGenerationGridCellsByLocation(StaticMeshComponent->GetComponentLocation());
				}*/
				//--------------------------------------------------------------------------------------------------------------
				StaticMeshComponent->DetachFromParent();
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->RemoveFromRoot();
				StaticMeshComponent->ConditionalBeginDestroy();
			}
		}

		GeneratedStaticMeshComponents.Empty();
	}
}

void AProcGenActor::ClearAttachedDecals()
{
	if (GeneratedDecalsComponents.Num() > 0)
	{
		for (UDecalComponent* pDecalComponent : GeneratedDecalsComponents)
		{
			if (pDecalComponent)
			{
				pDecalComponent->DetachFromParent();
				pDecalComponent->UnregisterComponent();
				pDecalComponent->RemoveFromRoot();
				pDecalComponent->ConditionalBeginDestroy();
			}
		}

		GeneratedDecalsComponents.Empty();
	}
}

static int32 currentInstanceCounter = 0;
static int32 ReservedInstancesNum = 200000;

//static int32 CachedParentGridCellId = -1;

//static FInstMeshCellInfo* CachedInstMeshCellInfoPointer = nullptr;



int32 AProcGenActor::CreateNewInstancedSMInst(UStaticMesh* pSM, FTransform& SMTrasf, int32 ParentGridCellId)
{
	if (!pSM)
		return -1;

	if (ParentGridCellId > -1)
	{
		bool bCachedPGCIdMatch = CachedParentGridCellId == ParentGridCellId && CachedInstMeshCellInfoPointer != nullptr && LastSMPtr == pSM;

		FInstMeshCellsData& InstMeshCellsData = InstancedStaticMeshComponentsMap2.FindOrAdd(pSM);
		FInstMeshCellInfo& InstMeshCellInfo = bCachedPGCIdMatch ? *CachedInstMeshCellInfoPointer : InstMeshCellsData.CellsInfo.FindOrAdd(ParentGridCellId);
		if (!InstMeshCellInfo.InstancedStaticMeshComponentPtr)
		{
			UHierarchicalInstancedStaticMeshComponent* StaticMeshComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
			StaticMeshComponent->Mobility = EComponentMobility::Static;
			StaticMeshComponent->bSelectable = false;
			StaticMeshComponent->bHasPerInstanceHitProxies = false;
			StaticMeshComponent->bAffectDistanceFieldLighting = false;
			StaticMeshComponent->SetCollisionProfileName(NoCollisionProf);
			StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);//bDisableCollision = true;
			StaticMeshComponent->SetCanEverAffectNavigation(false);
			StaticMeshComponent->bAutoRebuildTreeOnInstanceChanges = false;
			StaticMeshComponent->SetStaticMesh(pSM);
			StaticMeshComponent->bDisableCollision = true;

			StaticMeshComponent->SetLODDataCount(pSM->GetNumLODs(), pSM->GetNumLODs());
			StaticMeshComponent->SetCullDistances(3500.0f, 20000.0f);
			StaticMeshComponent->CachedMaxDrawDistance = 22500.0f;
			
			StaticMeshComponent->RegisterComponent();
			StaticMeshComponent->AttachToComponent(GenerationSplineShape, FAttachmentTransformRules::KeepRelativeTransform);
			InstMeshCellInfo.InstancedStaticMeshComponentPtr = StaticMeshComponent;

			/*FTransform ZeroTrasf = FTransform();
			ZeroTrasf.SetLocation(FVector(0, 0, -5000000));
			ZeroTrasf.RemoveScaling(0);
			for (int32 i = 0; i < ReservedInstancesNum; ++i)
			{
				InstMeshCellInfo.InstancedStaticMeshComponentPtr->AddInstance(ZeroTrasf);
			}*/
		}

		if (InstMeshCellInfo.InstancedStaticMeshComponentPtr)
		{
			//InstMeshCellInfo.InstancedStaticMeshComponentPtr->UpdateInstanceTransform(InstMeshCellInfo.InstanceCreateCounter, SMTrasf, true/*, true, true*/);
			int32 instanceId = InstMeshCellInfo.InstancedStaticMeshComponentPtr->AddInstance(ZeroTrasf);
			InstMeshCellInfo.InstancedStaticMeshComponentPtr->UpdateInstanceTransform(instanceId, SMTrasf, true/*, true, true*/);
			int32 oldIc = InstMeshCellInfo.InstanceCreateCounter;
			++InstMeshCellInfo.InstanceCreateCounter;
			if (InstMeshCellInfo.InstanceCreateCounter >= ReservedInstancesNum)
			{
				InstMeshCellInfo.InstanceCreateCounter = 0;
			}
			InstMeshCellInfo.bNeedTreeRebuild = true;

			CachedParentGridCellId = ParentGridCellId;
			CachedInstMeshCellInfoPointer = &InstMeshCellInfo;
			LastSMPtr = pSM;
			return oldIc;
		}
		return -1;
	}

	UInstancedStaticMeshComponent*& InstancedStaticMeshComponent = InstancedStaticMeshComponentsMap.FindOrAdd(pSM);
	if (InstancedStaticMeshComponent == nullptr)
	{
		//UHierarchicalInstancedStaticMeshComponent::ActualInstancesPerLeaf()
		//UInstancedStaticMeshComponent* StaticMeshComponent = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		UHierarchicalInstancedStaticMeshComponent* StaticMeshComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		StaticMeshComponent->Mobility = EComponentMobility::Static;
		StaticMeshComponent->bSelectable = false;
		StaticMeshComponent->bHasPerInstanceHitProxies = false;
		StaticMeshComponent->bAffectDistanceFieldLighting = false;
		StaticMeshComponent->SetCollisionProfileName(NoCollisionProf);
		StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);//bDisableCollision = true;
		StaticMeshComponent->SetCanEverAffectNavigation(false);
		StaticMeshComponent->bAutoRebuildTreeOnInstanceChanges = false;
		StaticMeshComponent->SetStaticMesh(pSM);
		StaticMeshComponent->bDisableCollision = true;

		StaticMeshComponent->SetLODDataCount(pSM->GetNumLODs(), pSM->GetNumLODs());
		StaticMeshComponent->SetCullDistances(3500.0f, 20000.0f);

		StaticMeshComponent->RegisterComponent();
		StaticMeshComponent->AttachToComponent(GenerationSplineShape, FAttachmentTransformRules::KeepRelativeTransform);
		InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent);
		//test
		if (InstancedStaticMeshComponent)
		{
			/*FTransform ZeroTrasf = FTransform();
			ZeroTrasf.SetLocation(FVector(0, 0, -5000000));
			ZeroTrasf.RemoveScaling(0);*/
			for (int32 i = 0; i < ReservedInstancesNum; ++i)
			{
				InstancedStaticMeshComponent->AddInstance(ZeroTrasf);
			}
			//StaticMeshComponent->BuildTreeIfOutdated(true, true);
		}
	}

	if (InstancedStaticMeshComponent != nullptr)
	{
		//InstancedStaticMeshComponent->GetInstanceTransform()
		InstancedStaticMeshComponent->UpdateInstanceTransform(currentInstanceCounter, SMTrasf, true/*, true, true*/);
		int32 oldIc = currentInstanceCounter;
		++currentInstanceCounter;
		if (currentInstanceCounter >= ReservedInstancesNum)
		{
			currentInstanceCounter = 0;
		}
		/*UHierarchicalInstancedStaticMeshComponent* pMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InstancedStaticMeshComponent);
		if (pMC)
		{
			pMC->BuildTreeIfOutdated(true, false);
		}*/
		//currentInstanceCounter = FMath::Clamp(currentInstanceCounter, 0, 5000000 - 1);
		bNeedTreeRebuild = true;
		return oldIc;
		//return InstancedStaticMeshComponent->AddInstance(SMTrasf);
	}
	return -1;
}



bool AProcGenActor::RemoveInstancedSMInstById(UStaticMesh* pSM, int32 Id, int32 ParentGridCellId)
{
	if (ParentGridCellId > -1)
	{
		bool bCachedPGCIdMatch = CachedParentGridCellId == ParentGridCellId && CachedInstMeshCellInfoPointer != nullptr && LastSMPtr == pSM;

		FInstMeshCellsData& InstMeshCellsData = InstancedStaticMeshComponentsMap2.FindOrAdd(pSM);
		FInstMeshCellInfo& InstMeshCellInfo = bCachedPGCIdMatch ? *CachedInstMeshCellInfoPointer : InstMeshCellsData.CellsInfo.FindOrAdd(ParentGridCellId);
		if (!InstMeshCellInfo.InstancedStaticMeshComponentPtr)
		{

		}

		if (InstMeshCellInfo.InstancedStaticMeshComponentPtr)
		{
			/*FTransform ZeroTrasf = FTransform();
			ZeroTrasf.SetLocation(FVector(0, 0, -5000000));
			ZeroTrasf.RemoveScaling(0);
			InstMeshCellInfo.InstancedStaticMeshComponentPtr->UpdateInstanceTransform(InstMeshCellInfo.InstanceCreateCounter, ZeroTrasf, true/ *, true, true* /);*/
			--InstMeshCellInfo.InstanceCreateCounter;
			if (InstMeshCellInfo.InstanceCreateCounter < 0)
			{
				InstMeshCellInfo.InstanceCreateCounter = 0;
			}
			InstMeshCellInfo.InstancedStaticMeshComponentPtr->RemoveInstance(InstMeshCellInfo.InstanceCreateCounter);
			InstMeshCellInfo.bNeedTreeRebuild = true;
			
			if (InstMeshCellInfo.InstanceCreateCounter < 0)
			{
				if (InstMeshCellInfo.InstancedStaticMeshComponentPtr->GetInstanceCount() <= 0)
				{
					/*InstMeshCellInfo.InstancedStaticMeshComponentPtr->DetachFromParent();
					InstMeshCellInfo.InstancedStaticMeshComponentPtr->UnregisterComponent();
					InstMeshCellInfo.InstancedStaticMeshComponentPtr->RemoveFromRoot();
					InstMeshCellInfo.InstancedStaticMeshComponentPtr->ConditionalBeginDestroy();
					InstMeshCellInfo.InstancedStaticMeshComponentPtr = nullptr;*/
				}
				else
				{

				}
				InstMeshCellInfo.InstanceCreateCounter = 0;
			}
		}

		CachedParentGridCellId = ParentGridCellId;
		CachedInstMeshCellInfoPointer = &InstMeshCellInfo;
		LastSMPtr = pSM;

		return true;
	}
	UInstancedStaticMeshComponent*& InstancedStaticMeshComponent = InstancedStaticMeshComponentsMap.FindOrAdd(pSM);
	if (InstancedStaticMeshComponent != nullptr)
	{
		bool bRemoved = true;//InstancedStaticMeshComponent->RemoveInstance(Id);

		/*FTransform ZeroTrasf = FTransform();
		ZeroTrasf.SetLocation(FVector(0, 0, -5000000));
		ZeroTrasf.RemoveScaling(0);*/
		InstancedStaticMeshComponent->UpdateInstanceTransform(Id, ZeroTrasf, true, true, true);
		bNeedTreeRebuild = true;
		if (InstancedStaticMeshComponent->GetInstanceCount() <= 0)
		{
			InstancedStaticMeshComponent->DetachFromParent();
			InstancedStaticMeshComponent->UnregisterComponent();
			InstancedStaticMeshComponent->RemoveFromRoot();
			InstancedStaticMeshComponent->ConditionalBeginDestroy();
			InstancedStaticMeshComponent = nullptr;
		}
		return bRemoved;
	}
	return false;
}

void AProcGenActor::ClearInstancedSMsAndInsts()
{
	for (TPair<UStaticMesh*, UInstancedStaticMeshComponent*>& InstCompData : InstancedStaticMeshComponentsMap)
	{
		if (InstCompData.Value)
		{
			InstCompData.Value->ClearInstances();
			InstCompData.Value->DetachFromParent();
			InstCompData.Value->UnregisterComponent();
			InstCompData.Value->RemoveFromRoot();
			InstCompData.Value->ConditionalBeginDestroy();
		}
	}

	InstancedStaticMeshComponentsMap.Empty();

	for (TPair<UStaticMesh*, FInstMeshCellsData>& InstCompData : InstancedStaticMeshComponentsMap2)
	{
		for (TPair<int32, FInstMeshCellInfo>& CellData : InstCompData.Value.CellsInfo)
		{
			if (CellData.Value.InstancedStaticMeshComponentPtr)
			{
				CellData.Value.InstancedStaticMeshComponentPtr->ClearInstances();
				CellData.Value.InstancedStaticMeshComponentPtr->DetachFromParent();
				CellData.Value.InstancedStaticMeshComponentPtr->UnregisterComponent();
				CellData.Value.InstancedStaticMeshComponentPtr->RemoveFromRoot();
				CellData.Value.InstancedStaticMeshComponentPtr->ConditionalBeginDestroy();
				CellData.Value.InstancedStaticMeshComponentPtr = nullptr;
			}
		}
	}

	InstancedStaticMeshComponentsMap2.Empty();

	CachedParentGridCellId = -1;
	CachedInstMeshCellInfoPointer = nullptr;
	LastSMPtr = nullptr;
}

void AProcGenActor::PaintBySphere(const FVector& SpherePos, float SphereSize)
{
	if (!CurCreatedProcGenSlotObject.Get())
	{
		CurCreatedProcGenSlotObject = NewObject<UPGSObj>(this, ProcGenSlotObjectClass.Get());
		CurCreatedProcGenSlotObject.Get()->AddToRoot();
	}

	if (!CurCreatedProcGenSlotObject.IsValid())
	{
		return;
	}

	UWorld* pCurEdWorld = nullptr;//GEditor->GetEditorWorldContext().World();
	UProcGenManager* pPGManager = UProcGenManager::GetCurManager();
	if (pPGManager)
	{
		pCurEdWorld = pPGManager->GetWorldPRFEditor();
	}

	TArray<FVector> MeshPoints = TArray<FVector>();
	float cp_coof = 15.0f / SphereSize;
	int32 numPoints = int32(1.0f / cp_coof);
	FVector ZAng = FVector(0);
	FQuat rot_z = FQuat::Identity;
	FVector xDir = FVector(1, 0, 0);
	FVector xPos = SpherePos;
	for (int32 i = 0; i < numPoints; ++i)
	{
		ZAng = FVector(0, 0, (cp_coof * i) * 360.0f);
		rot_z = FQuat::MakeFromEuler(ZAng);
		xDir = FVector(1, 0, 0);
		xDir = rot_z * xDir;
		xPos = SpherePos + (xDir * SphereSize);
		xPos.Z += GenerationZOffset;
		MeshPoints.Add(xPos);
	}

	if (MeshPoints.Num() < 3)
		return;

	FPoly PolyShapeNv = FPoly();
	PolyShapeNv.Init();
	for (int32 i = 0; i < MeshPoints.Num(); ++i)
	{
		PolyShapeNv.InsertVertex(i, MeshPoints[i]);
	}

	if (GenerationZExtent != 0.0f)
	{
		MeshPoints.Add(MeshPoints[0] + FVector(0, 0, GenerationZExtent));
	}

	float fGenPowScaled = GenerationPower;
	if (bScaleGenerationPowerBySize)
	{
		FBox NvBox = FBox(MeshPoints);
		FVector BoxSize = NvBox.GetSize();
		//BoxSize.Z = 0.0f;
		float boxSzLngt = BoxSize.Size2D();
		float PercentSz = boxSzLngt / 5000.0f;
		fGenPowScaled *= PercentSz;
	}

	TArray<FPoly> ExcludedPls = TArray<FPoly>();
	FillExludePolys(ExcludedPls);

	if (!CurCreatedProcGenSlotObject.Get()->bIsPreparedToGeneration)
	{
		CurCreatedProcGenSlotObject.Get()->PrepareToGeneration();
	}

	FVector GenDir = bUseGenerationDirectionAC ? GenerationDirectionArrowComp->GetComponentRotation().Vector() : FVector::ZeroVector;
	FVector AlignDir = bUseGenerationAlignDirectionAC ? GenerationDirectionAlignmentArrowComp->GetComponentRotation().Vector() : FVector::ZeroVector;
	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (pProcGenManager)
	{
		if (pProcGenManager->bUseCameraDirToGenerationInPaintMode)
		{
			GenDir = pProcGenManager->CurrentPaintCameraDir;
		}
		if (pProcGenManager->bUseCameraDirToRotationAlignGenObjectsInPaintMode)
		{
			AlignDir = -pProcGenManager->CurrentPaintCameraDir;
		}
	}

	CurCreatedProcGenSlotObject.Get()->RequestGenerateInBBoxWithShapeBorder(MeshPoints, pCurEdWorld, fGenPowScaled, PolyShapeNv, ExcludedPls, this, GenDir, AlignDir);
}

bool AProcGenActor::IsPointInsideConnectedShapeLimitActors(const FVector& PointPos)
{
	if(ShapeLimitActors.Num() <= 0)
		return true;

	FVector ClosestPoint = FVector::ZeroVector;
	for (AActor* pCSLActor : ShapeLimitActors)
	{
		if (pCSLActor)
		{
			UStaticMeshComponent* pActMeshComp = Cast<UStaticMeshComponent>(pCSLActor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
			if (pActMeshComp)
			{
				float distTC = pActMeshComp->GetDistanceToCollision(PointPos, ClosestPoint);
				if (distTC <= 0.0f)
				{
					return true;
				}
			}
		}
	}

	return false;
}
