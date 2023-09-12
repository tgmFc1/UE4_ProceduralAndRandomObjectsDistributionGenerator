// Free to copy, edit and use in any work/projects, commercial or free


#include "ProcGen/ProcGenParamsModifierActor.h"
#include "Runtime/Engine/Classes/Components/SplineComponent.h"
#include "..\..\Public\ProcGen\ProcGenParamsModifierActor.h"
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
#include "ProcGen/ProcGenSlotObject.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

#include "Components/ArrowComponent.h"

AProcGenParamsModifierActor::AProcGenParamsModifierActor() : Super(), ModifierSlotsMap(), SplineShapeTriangles(), ShapeBoundingBox(EForceInit::ForceInit)
{
	PrimaryActorTick.bCanEverTick = false;

	AreaSplineShape = nullptr;
	SplineDivideDistanceForTriangulation = 100.0f;
	bUseSplineShapeForAreaDefinition = false;
	bUseNonClosedSplineShapeForAreaDefinition = false;
	ModifierAreaRadius = 0.0f;
	ShapeCenterPoint = FVector::ZeroVector;
	//ShapeBoundingBox = FBox::
}

void AProcGenParamsModifierActor::BeginPlay()
{
	Super::BeginPlay();
}

void AProcGenParamsModifierActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (pProcGenManager)
	{
		int32 ids = pProcGenManager->ProcGenParamsModifierActorsPtrs.Find(this);
		if (ids > -1)
		{
			pProcGenManager->ProcGenParamsModifierActorsPtrs.RemoveAt(ids);
		}
	}
}

void AProcGenParamsModifierActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	USplineComponent* SplineShapeComponent = FindComponentByClass<USplineComponent>();
	AreaSplineShape = SplineShapeComponent;

	TArray<FVector> PointsArr = GetSplinePointsForShapeTriagulationWithoutOffset();
	if (bUseSplineShapeForAreaDefinition && PointsArr.Num() >= 2)
	{
		FVector Center = FVector::ZeroVector;
		for (FVector& CurPoint : PointsArr)
		{
			Center += (CurPoint * 0.001f);
		}
		Center = (Center / PointsArr.Num()) * 1000.0f;
		ShapeCenterPoint = Center;
		TArray<FVector> PointsArr2 = PointsArr;
		PointsArr2.Add(ShapeCenterPoint + FVector(0, 0, 50000));
		PointsArr2.Add(ShapeCenterPoint + FVector(0, 0, -50000));

		ShapeBoundingBox = FBox(PointsArr2);
	}
	else
	{
		ShapeCenterPoint = GetActorLocation();
		PointsArr.Empty();
		PointsArr.Add(ShapeCenterPoint + (FVector(0, 0, 1) * ModifierAreaRadius));
		PointsArr.Add(ShapeCenterPoint + (FVector(0, 0, -1) * ModifierAreaRadius));
		PointsArr.Add(ShapeCenterPoint + (FVector(0, 1, 0) * ModifierAreaRadius));
		PointsArr.Add(ShapeCenterPoint + (FVector(0, -1, 0) * ModifierAreaRadius));
		PointsArr.Add(ShapeCenterPoint + (FVector(1, 0, 0) * ModifierAreaRadius));
		PointsArr.Add(ShapeCenterPoint + (FVector(-1, 0, 0) * ModifierAreaRadius));

		ShapeBoundingBox = FBox(PointsArr);

		PointsArr.Empty();
	}

	UProcGenManager* pProcGenManager = UProcGenManager::GetCurManager();
	if (pProcGenManager)
	{
		pProcGenManager->ProcGenParamsModifierActorsPtrs.AddUnique(this);
	}

	SplineShapeTriangles.Empty();

	FPoly PolyShapeNv = FPoly();
	PolyShapeNv.Init();

	if (bUseSplineShapeForAreaDefinition && PointsArr.Num() >= 2)
	{
		for (int32 i = 0; i < PointsArr.Num(); ++i)
		{
			PolyShapeNv.InsertVertex(i, PointsArr[i]);
		}
		PolyShapeNv.Fix();
		PolyShapeNv.Triangulate(nullptr, SplineShapeTriangles);
	}
	else
	{
		float cp_coof = SplineDivideDistanceForTriangulation / ModifierAreaRadius;
		if (cp_coof > 0.25f)
			cp_coof = 0.25f;//four points minimum

		int32 numPoints = int32(1.0f / cp_coof);
		FVector fwdTargetVector = GetActorForwardVector();
		FVector ToPointDirVector = FVector(0);
		FVector ZAng = FVector(0);
		FQuat rot_z = FQuat::Identity;
		FVector xDir = FVector(1, 0, 0);
		FVector xPos = GetActorLocation();
		for (int32 i = 0; i < numPoints; ++i)
		{
			ZAng = FVector(0, 0, (cp_coof * i) * 360.0f);
			rot_z = FQuat::MakeFromEuler(ZAng);
			xDir = FVector(1, 0, 0);
			xDir = rot_z * xDir;
			xDir.Normalize();
			xPos = GetActorLocation() + (xDir * ModifierAreaRadius);
			PolyShapeNv.InsertVertex(i, xPos);
			//UKismetSystemLibrary::DrawDebugLine(GetWorld(), xPos, GetActorLocation(), FLinearColor::Red, 5.3f);
		}
		//PolyShapeNv.InsertVertex(numPoints, GetActorLocation());
		//PolyShapeNv.Fix();
		PolyShapeNv.Triangulate(nullptr, SplineShapeTriangles);
	}

	/*for (int32 i2 = 0; i2 < SplineShapeTriangles.Num(); ++i2)
	{
		FPoly& CurPly = SplineShapeTriangles[i2];
		FVector OldVert = FVector(0);
		for (int32 ic = 0; ic < CurPly.Vertices.Num(); ++ic)
		{
			if (OldVert.IsZero())
			{
				OldVert = CurPly.Vertices[CurPly.Vertices.Num() - 1];
			}

			UKismetSystemLibrary::DrawDebugLine(GetWorld(), CurPly.Vertices[ic], OldVert, FLinearColor::Green, 5.3f);
			OldVert = CurPly.Vertices[ic];
		}
	}*/
}

void AProcGenParamsModifierActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

TArray<FVector> AProcGenParamsModifierActor::GetSplinePointsForShapeTriagulationWithoutOffset()
{
	FVector SplinePoint;
	TArray<FVector> PointsArr = TArray<FVector>();
	if (!AreaSplineShape)
		return PointsArr;

	if (bUseNonClosedSplineShapeForAreaDefinition)
	{
		FTransform SplineTransf;
		TArray<FTransform> TransfArr = TArray<FTransform>();
		int32 numPoints = int32(AreaSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
		if (numPoints >= 2)
		{
			float totalDist = 0.0f;
			for (int32 i = 0; i < numPoints; ++i)
			{
				SplineTransf = AreaSplineShape->GetTransformAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
				TransfArr.Add(SplineTransf);
				totalDist += SplineDivideDistanceForTriangulation;
			}
			
			{
				SplineTransf = AreaSplineShape->GetTransformAtDistanceAlongSpline(AreaSplineShape->GetSplineLength(), ESplineCoordinateSpace::World);
				TransfArr.Add(SplineTransf);
			}

			FTransform SplineTransCur;
			FTransform SplineTransNext;
			FVector PolyBoxBorder[4];
			for (int32 i = 0; i < TransfArr.Num(); ++i)
			{
				if ((i + 1) < TransfArr.Num())
				{
					SplineTransCur = TransfArr[i];
					SplineTransNext = TransfArr[i + 1];
					PolyBoxBorder[0] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, 1, 0)) * ModifierAreaRadius);
					PolyBoxBorder[1] = SplineTransCur.GetLocation() + ((SplineTransCur.GetRotation() * FVector(0, -1, 0)) * ModifierAreaRadius);
					PolyBoxBorder[2] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, -1, 0)) * ModifierAreaRadius);
					PolyBoxBorder[3] = SplineTransNext.GetLocation() + ((SplineTransNext.GetRotation() * FVector(0, 1, 0)) * ModifierAreaRadius);
					for (int32 ic = 0; ic < 4; ++ic)
					{
						PointsArr.AddUnique(PolyBoxBorder[ic]);
					}
				}
			}
		}

		if(PointsArr.Num() > 0)
			return PointsArr;
	}

	if (!AreaSplineShape->IsClosedLoop())
	{
		int32 numPoints = int32(AreaSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
		if (numPoints < 2)
		{
			return PointsArr;
		}

		float totalDist = 0.0f;
		for (int32 i = 0; i < numPoints; ++i)
		{
			SplinePoint = AreaSplineShape->GetLocationAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
			PointsArr.Add(SplinePoint);
			totalDist += SplineDivideDistanceForTriangulation;
		}

		SplinePoint = AreaSplineShape->GetLocationAtDistanceAlongSpline(AreaSplineShape->GetSplineLength(), ESplineCoordinateSpace::World);
		PointsArr.Add(SplinePoint);

		return PointsArr;
	}
	int32 numPoints = int32(AreaSplineShape->GetSplineLength() / SplineDivideDistanceForTriangulation);
	if (numPoints < 3)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("AProcGenParamsModifierActor::GetSplinePointsForShapeTriagulationWithoutOffset() return empty PointsArr, error 5"));
		return PointsArr;
	}

	float totalDist = 0.0f;
	for (int32 i = 0; i < numPoints; ++i)
	{
		SplinePoint = AreaSplineShape->GetLocationAtDistanceAlongSpline(totalDist, ESplineCoordinateSpace::World);
		PointsArr.Add(SplinePoint);
		totalDist += SplineDivideDistanceForTriangulation;
	}

	return PointsArr;
}

float AProcGenParamsModifierActor::GetDistanceToShapeEdgeFromPoint(const FVector& Point, bool b2D)
{
	if (bUseSplineShapeForAreaDefinition && AreaSplineShape)
	{
		FVector NearestSplinePoint = AreaSplineShape->FindLocationClosestToWorldLocation(Point, ESplineCoordinateSpace::World);
		if (b2D)
		{
			NearestSplinePoint.Z = Point.Z;
		}
		return FVector::Dist(Point, NearestSplinePoint);
	}
	else
	{
		if(ModifierAreaRadius == 0.0f)
			return 0.0f;

		FVector Center = ShapeCenterPoint;
		if (b2D)
		{
			Center.Z = Point.Z;
		}
		FVector DirectionToEdge = Point - Center;
		DirectionToEdge.Normalize();
		FVector NearestEdgePoint = DirectionToEdge * ModifierAreaRadius;
		if (b2D)
		{
			NearestEdgePoint.Z = Point.Z;
		}
		return FVector::Dist(Point, NearestEdgePoint);
	}
	return 0.0f;
}

float AProcGenParamsModifierActor::GetPercentDistanceToShapeEdgeFromPointToShapeCenter(const FVector& Point, bool b2D)
{
	if (bUseSplineShapeForAreaDefinition && AreaSplineShape)
	{
		FVector NearestSplinePoint = AreaSplineShape->FindLocationClosestToWorldLocation(Point, ESplineCoordinateSpace::World);
		FVector Center = ShapeCenterPoint;
		if (b2D)
		{
			NearestSplinePoint.Z = Point.Z;
			Center.Z = Point.Z;
		}
		float distToCenter = FVector::Dist(Center, NearestSplinePoint);
		float distToPoint = FVector::Dist(Point, NearestSplinePoint);
		if (bUseNonClosedSplineShapeForAreaDefinition)
		{
			if (ModifierAreaRadius == 0.0f)
				return 0.0f;

			return distToPoint / ModifierAreaRadius;
		}
		else
		{
			return distToPoint / distToCenter;
		}
	}
	else
	{
		if (ModifierAreaRadius == 0.0f)
			return 0.0f;

		FVector Center = ShapeCenterPoint;
		if (b2D)
		{
			Center.Z = Point.Z;
		}
		FVector DirectionToEdge = Point - Center;
		DirectionToEdge.Normalize();
		FVector NearestEdgePoint = Center + (DirectionToEdge * ModifierAreaRadius);
		if (b2D)
		{
			NearestEdgePoint.Z = Point.Z;
		}
		float distToPoint = FVector::Dist(Point, NearestEdgePoint);
		return distToPoint / ModifierAreaRadius;
	}
	return 0.0f;
}

bool AProcGenParamsModifierActor::IsPointInsideShape2D(const FVector& Point)
{
	FVector AdditivePos = FVector(0, 0, 10000000);
	return UPGSObj::LinePolyIntersection(SplineShapeTriangles, Point + AdditivePos, Point - AdditivePos);
}
