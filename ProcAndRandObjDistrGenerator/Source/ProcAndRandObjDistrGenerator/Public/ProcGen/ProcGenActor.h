// Free to copy, edit and use in any work/projects, commercial or free

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProcGen/ProcGenSlotObject.h"
#include "Engine/Polys.h"
#include "ProcGenActor.generated.h"

/**
 * 
 */

struct SProcessScopeExec 
{
	/*SProcessScopeExec(bool* variablePtr)
	{
		bVarPtr = variablePtr;
		bVarPtr = reinterpret_cast<bool*>(true);
	}

	~SProcessScopeExec()
	{
		bVarPtr = reinterpret_cast<bool*>(false);
		bVarPtr = nullptr;
	}

	bool* bVarPtr;*/

	SProcessScopeExec(bool& variableL)
	{
		bVarPtr = &variableL;
		bVarPtr = reinterpret_cast<bool*>(true);
	}

	~SProcessScopeExec()
	{
		bVarPtr = reinterpret_cast<bool*>(false);
		bVarPtr = nullptr;
	}

	//bool& bVarL;
	bool* bVarPtr;
};

struct FInstMeshCellInfo
{
	class UHierarchicalInstancedStaticMeshComponent* InstancedStaticMeshComponentPtr;
	int32 InstanceCreateCounter;
	bool bNeedTreeRebuild;

	FInstMeshCellInfo()
	{
		InstancedStaticMeshComponentPtr = nullptr;
		InstanceCreateCounter = 0;
		bNeedTreeRebuild = false;
	}
};

struct FInstMeshCellsData
{
	TMap<int32, FInstMeshCellInfo> CellsInfo;

	FInstMeshCellsData() : CellsInfo()
	{

	}
};

UCLASS()
class AProcGenActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AProcGenActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Actor, meta = (AllowPrivateAccess = "true"))
	class UArrowComponent* GenerationDirectionArrowComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Actor, meta = (AllowPrivateAccess = "true"))
	class UArrowComponent* GenerationDirectionAlignmentArrowComp;

	virtual void OnConstruction(const FTransform& Transform) override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	TArray<FVector> GetSplinePointsForShapeTriagulation();
	TArray<FVector> GetSplinePointsForShapeTriagulationWithoutOffset(bool bSupportNonClosedShape = false);
	TArray<FVector> GetSplinePointsForGenOnSpline();
	TArray<FTransform> GetSplineTransformsForGenOnSpline();

	void FillExludePolys(TArray<FPoly>& PolysArr);

	UFUNCTION(BlueprintCallable, Category = Generation)
	void TestBuildPolyShapeToGen();

	UFUNCTION(BlueprintCallable, Category = Generation)
	float GetDistanceToShapeEdgeFromPoint(const FVector& Point, bool b2D);

	UFUNCTION(BlueprintCallable, Category = Generation)
	float GetPercentDistanceToShapeEdgeFromPointToShapeCenter(const FVector& Point, bool b2D);
	UFUNCTION(BlueprintCallable, Category = Generation)
	float GetPercentDistanceToSplineNearestCenterFromPoint(const FVector& Point, bool b2D);
	UFUNCTION(BlueprintCallable, Category = Generation)
	float GetDistanceToExcludedShapes(const FVector& PointPos);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Generation)
	void GenerateRequest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Clearing)
	void ClearPreviousRequest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Clearing)
	void ClearAttachedProceduralComponentsRequest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Results)
	void FreezeResultsRequest();

	//UFUNCTION(BlueprintCallable, CallInEditor, Category = Results)
	void StartTest1Request();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Debug)
	void DrawGenerationDebugInfo();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Clearing)
	void ClearingProcGenActorsInRangeRequest();

	void GenerateOnTargetLandscape();

	UFUNCTION()
	void OnLinkedActorDestroyed(AActor* DestroyedActor);

	class UStaticMeshComponent* CreateNewSMComponent(class UStaticMesh* pSM, const FTransform& SMTrasf, struct FStaticMeshCollisionSetup* StaticMeshCollisionSetupPtr = nullptr, struct FStaticMeshRenderingOverrideSetup* StaticMeshRenderingOverrideSetupPtr = nullptr, bool bIsSimple = false);

	void RemoveSMComp(class UStaticMeshComponent* pStaticMeshComponent);

	class UDecalComponent* CreateNewDecalComponent(class UMaterialInterface* DecalMaterial, const FTransform& DecalTrasf, const FVector& DecalScale, struct FDecalRenderingOverrideSetup* DecalRenderingOverrideSetupPtr = nullptr);

	void ClearAttachedSMs();

	void ClearAttachedDecals();

	int32 CreateNewInstancedSMInst(class UStaticMesh* pSM, FTransform& SMTrasf, int32 ParentGridCellId = -1);
	bool RemoveInstancedSMInstById(class UStaticMesh* pSM, int32 Id, int32 ParentGridCellId = -1);
	void ClearInstancedSMsAndInsts();

	void PaintBySphere(const FVector& SpherePos, float SphereSize);

	bool IsPointInsideConnectedShapeLimitActors(const FVector& PointPos);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Generation, meta = (AllowPrivateAccess = "true"))
	class USplineComponent* GenerationSplineShape;
/** Spline component divide distance, divide spline to segments with length == this distance, if triangulation(and generation with her) fail - try to play with this value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	float SplineDivideDistanceForTriangulation;
/** Generation slot object(PGSObj class), contains most of data for generation, generation will not work without it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	TSubclassOf<UPGSObj> ProcGenSlotObjectClass;

	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GenerationParams)
	//UPGSObj* CurCreatedProcGenSlotObject;
	TWeakObjectPtr<UPGSObj> CurCreatedProcGenSlotObject;
	
/** Generation bounding box Z coordinate offset of generation actor root component position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	float GenerationZOffset;
/** Generation bounding box additional scale by Z coordinate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	float GenerationZExtent;
/** Multiplier for RandomGenerationPowerMinMax parameter in generation slot object, if <= zero - generation be failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	float GenerationPower;
/** Option to scale multiplier for RandomGenerationPowerMinMax parameter in generation slot object by generation bounding box size, bigger bounding box - more generation attempts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bScaleGenerationPowerBySize;
/** Option to generate objects along spline component with using SplineGenThickness value and SplineDivideDistanceForTriangulation for creating spline generation segments */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bGenerationOnSpline;
/** Option for spline generation segments thickness, used only with GenerationOnSpline option enabled, must be > 0 for successful generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	float SplineGenThickness;
/** Option for start generation process on begin play - when game starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bGenerateOnBeginPlay;
/** Option to generate objects by using target landscape(TargetLandscapePtr must be selected in editor, also can be used with selected LandscapeGenerationMask)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bGenerateOnTargetLandscape;
/** Generation on spline option(use only with GenerationOnSpline option enabled) to use spline segments direction for generated objects rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bGOSUseNormalAndYawToRotate;
/** Generation on spline option(use only with GenerationOnSpline and GOSUseNormalAndYawToRotate options enabled) to add Yaw for generated objects rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	float fGOSYawAddOnRotate;
/** Use only spline control points in triangulation process, can be used if spline represents simple and not curved shape, like rectangle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bUseSplineControlPointsOnlyToGenShape;
/** Option for using with GenerateOnTargetLandscape, this is the reference to landscape actor, GenerateOnTargetLandscape option require this to be valid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationLandscapeTarget)
	class ALandscape* TargetLandscapePtr;
/** Option for using with GenerateOnTargetLandscape, this is landscape generation mask as a texture 2D, in generation process this mask be projected to target 
landscape and will be assigned to each landscape virtual cell when process decide how many objects must be generated in cell, texture 2D to use in this process
 must have compression setting selected as vector displacement map and mip maps generation setting(Mip Gen Setting) selected as NoMipmaps, this two parameters are
 very important to generation process, process will fail if parameters not valid*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationLandscapeTarget)
	UTexture2D* LandscapeGenerationMask;
/** Self collide for generation process rays casts, used if this generation actor have attached generated static mesh components or generate SM components at this moment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bSelfCollideWhenGeneration;
/** Other ProcGenActor class actors on level can be used as forbidden shapes(defined by spline component) in generation process */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationExternalTIAOptions)
	TArray<AProcGenActor*> OthersPGAsUsedAsExcludedShapes;
/** Other ProcGenActor class actors on level can be used in generation process as taken into account objects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationExternalTIAOptions)
	TArray<AProcGenActor*> OthersPGAsUsedAsTIA;
/** Simply static mesh actors(or any other with static mesh component(support only simple static meshes like boxes and spheres)) on level can be used as generation shape limit(objects can be generated only in this shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationExternalTIAOptions)
	TArray<AActor*> ShapeLimitActors;
/** Option for use GenerationDirectionArrow component of this ProcGenActor to set generation rays casts direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationDirection)
	bool bUseGenerationDirectionAC;
/** Option for use GenerationDirectionAlignmentArrow component of this ProcGenActor to set rotation alignment direction for generated objects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationDirection)
	bool bUseGenerationAlignDirectionAC;
/** Already calculated spline shape center (same for closed spline shape and opened spline shape) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GenerationPreCalculatedVars)
	FVector GenShapeCenterPoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GridGeneration)
	TArray<int32> LinkedGenSlotsUIDs;

	bool bIsProcess;

	TArray<class UStaticMeshComponent*> GeneratedStaticMeshComponents;
	TArray<class UDecalComponent*> GeneratedDecalsComponents;

	FBox ShapeBoundingBox;

	TMap<class UStaticMesh*, class UInstancedStaticMeshComponent*> InstancedStaticMeshComponentsMap;
	TMap<class UStaticMesh*, FInstMeshCellsData> InstancedStaticMeshComponentsMap2;

	float TreeBuildRecool = 0.0f;
	bool bNeedTreeRebuild = false;

	int32 CachedParentGridCellId = -1;

	FInstMeshCellInfo* CachedInstMeshCellInfoPointer = nullptr;
	class UStaticMesh* LastSMPtr = nullptr;
};
