// Free to copy, edit and use in any work/projects, commercial or free

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProcGen/ProcGenSlotObject.h"
#include "Engine/Polys.h"
#include "ProcGenParamsModifierActor.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FMinMaxParam
{
	GENERATED_USTRUCT_BODY()

	FMinMaxParam()
	{
		MinValue = 0.0f;
		MaxValue = 0.0f;
		ShapeEdgesToCenterPowerCurve = nullptr;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float MinValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float MaxValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	UCurveFloat* ShapeEdgesToCenterPowerCurve;

	bool IsIgnored()
	{
		return MinValue == 0.0f && MaxValue == 0.0f;
	}
};

USTRUCT(BlueprintType)
struct FGenModifierSlotParams
{
	GENERATED_USTRUCT_BODY()

	FGenModifierSlotParams() :
	ScaleMinMaxModifier(),
	GenerationChanceMinMaxModifier()
	{
		bAdditiveModifier = true;
		bUseLinearModifierPowerFromShapeEdgesToShapeCenter = true;
		ShapeEdgesToCenterPowerGlobalCurve = nullptr;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FMinMaxParam ScaleMinMaxModifier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FMinMaxParam GenerationChanceMinMaxModifier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bAdditiveModifier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bUseLinearModifierPowerFromShapeEdgesToShapeCenter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	UCurveFloat* ShapeEdgesToCenterPowerGlobalCurve;
};

UCLASS()
class AProcGenParamsModifierActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AProcGenParamsModifierActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	virtual void OnConstruction(const FTransform& Transform) override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	TArray<FVector> GetSplinePointsForShapeTriagulationWithoutOffset();

	UFUNCTION(BlueprintCallable, Category = AreaShape)
	float GetDistanceToShapeEdgeFromPoint(const FVector& Point, bool b2D);

	UFUNCTION(BlueprintCallable, Category = AreaShape)
	float GetPercentDistanceToShapeEdgeFromPointToShapeCenter(const FVector& Point, bool b2D);

	UFUNCTION(BlueprintCallable, Category = AreaShape)
	bool IsPointInsideShape2D(const FVector& Point);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = AreaShape, meta = (AllowPrivateAccess = "true"))
	class USplineComponent* AreaSplineShape;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AreaShape)
	float SplineDivideDistanceForTriangulation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bUseSplineShapeForAreaDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bUseNonClosedSplineShapeForAreaDefinition;
/** Used to define area radius if modifier actor not have area spline shape component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float ModifierAreaRadius;
/** Slot with unique id related modifier parameters, generation slots of UPGSObj generation process be modified if slot unique id and this map key be equal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TMap<int32, FGenModifierSlotParams> ModifierSlotsMap;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GenerationPreCalculatedVars)
	FVector ShapeCenterPoint;
	TArray<FPoly> SplineShapeTriangles;
	FBox ShapeBoundingBox;
};
