// Free to copy, edit and use in any work/projects, commercial or free

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Polys.h"
#include "ProcGenSlotObject.generated.h"

USTRUCT(BlueprintType)
struct FPGSlotMinMaxParams
{
	GENERATED_USTRUCT_BODY()

	FPGSlotMinMaxParams()
	{
		MinValue = 0.0f;
		MaxValue = 0.0f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float MinValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float MaxValue;

	bool IsIgnored()
	{
		return MinValue == 0.0f && MaxValue == 0.0f;
	}

	float GetRandomValue(struct FProcGenSlotParams* pParams);
};

USTRUCT(BlueprintType)
struct FActorTypeWithChanceTG
{
	GENERATED_USTRUCT_BODY()

	FActorTypeWithChanceTG() : ActorClass()
	{
		GenerationChance = 100.0f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TSubclassOf<AActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float GenerationChance;
};

//FName collisionProfile;

//ECollisionEnabled collisionEnable;

USTRUCT(BlueprintType)
struct FStaticMeshCollisionSetup
{
	GENERATED_USTRUCT_BODY()

	FStaticMeshCollisionSetup()
	{
		collisionProfile = TEXT("BlockAll");
		collisionEnable = uint8(ECollisionEnabled::QueryAndPhysics);
	}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FName collisionProfile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params, meta = (ToolTip = "0 - no coll, 1 - QueryOnly, 2 - PhysicsOnly, 3 - QueryAndPhysics"))
	uint8 collisionEnable;
};

USTRUCT(BlueprintType)
struct FStaticMeshRenderingOverrideSetup
{
	GENERATED_USTRUCT_BODY()

	FStaticMeshRenderingOverrideSetup()
	{
		OverrideMinimalLOD = -1;
	}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	int32 OverrideMinimalLOD;
};

USTRUCT(BlueprintType)
struct FStaticMeshTypeWithChanceTG
{
	GENERATED_USTRUCT_BODY()

	FStaticMeshTypeWithChanceTG() : StaticMeshCollisionSetup(), StaticMeshRenderingOverrideSetup()
	{
		GenerationChance = 100.0f;
		StaticMeshPtr = nullptr;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float GenerationChance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	class UStaticMesh* StaticMeshPtr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FStaticMeshCollisionSetup StaticMeshCollisionSetup;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FStaticMeshRenderingOverrideSetup StaticMeshRenderingOverrideSetup;
};

USTRUCT(BlueprintType)
struct FDecalTypeWithChanceTG
{
	GENERATED_USTRUCT_BODY()

	FDecalTypeWithChanceTG() : DecalInitialScale(0)
	{
		GenerationChance = 100.0f;
		DecalMaterial = nullptr;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float GenerationChance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	class UMaterialInterface* DecalMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FVector DecalInitialScale;
};

USTRUCT(BlueprintType)
struct FProcGenSlotParams
{
	GENERATED_USTRUCT_BODY()

	FProcGenSlotParams() : 
		ActorsTypesToGenerate(), 
		ActorsTypesWithChanceToGenerate(),
		StaticMeshesTypesToGenerateWithChance(),
		DecalTypesToGenerateWithChance(),
		TerrainLayersAllowed(), 
		SurfaceTypesAllowed(), 
		AdditionalLocationVector(), 
		AdditionalRotation(), 
		TakenIntoAccSlotsIds(), 
		CurrentGenerationStream(), 
		GeneratedActorsPtrs(), 
		TempTransformsForObjects(),

		ScaleMinMax(),
		HeightMinMax(),
		SlopeMinMax(),
		SlopeMinVariation(),
		SlopeMaxVariation(),
		DistanceBetweenMinMax(),
		PressInGroundMinMax(),
		PressInGroundNZMinMax(),
		AddZPosFGNMinMax(),
		RandomRotationRollMinMax(),
		RandomRotationPitchMinMax(),
		RandomRotationYawMinMax(),
		RandomNUScaleXMinMax(),
		RandomNUScaleYMinMax(),
		RandomNUScaleZMinMax(),
		RandomGenerationCoofMinMax(),
		RandomGenerationPowerMinMax(),
		AlignToNormalMaxAngleMinMax()
	{
		RandomSeed = 371;
		RotateToGroundNormal = false;
		AddZPosFromGroundNormal = 0.0f;
		SlopeOverMinGenChance = 0.0f;
		SlopeOverMaxGenChance = 0.0f;
		PressInGroundBySlopeForce = 0.0f;
		FloatingInAirGeneration = false;
		UnderTerrainFixGeneration = false;
		GenerationTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
		bGenerateOnLandscape = true;
		bGenerateOnStaticMeshes = true;
		bCollisionCheckBeforeGenerate = false;
		DefCollisionSize = 0.0f;
		DefCollisionHeightSize = 0.0f;
		bCollisionCheckExcludeOnlyOtherPGMeshes = false;
		bCollisionCheckExcludeOnlyAllPGMeshes = false;
		bPressInGroundByNormalZ = false;
		bUseMultiLT = false;
		bAllowGenerationOnOtherGenActors = true;
		bPrefFirstHit = true;
		bPrefLandscape = true;
		SlotUniqueId = 0;
		bIsActive = true;
	}
/** Actors classes to generate actors, selected randomly from this array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<TSubclassOf<AActor>> ActorsTypesToGenerate;
/** Actors classes to generate actors, selected randomly from this array but takes defined chance into selection process */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<FActorTypeWithChanceTG> ActorsTypesWithChanceToGenerate;
/** Static meshes to generate as ProcGenActor component, selected randomly with defined chance from this array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<FStaticMeshTypeWithChanceTG> StaticMeshesTypesToGenerateWithChance;
/** Decals to generate as ProcGenActor component, selected randomly with defined chance from this array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<FDecalTypeWithChanceTG> DecalTypesToGenerateWithChance;
/** Minimum and maximum scale for generated objects, if zero - default object scale be used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams ScaleMinMax;
/** Minimum and maximum height limit for generated objects(by object Z coordinate), if zero - no generation height limits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams HeightMinMax;
/** Minimum and maximum slope limit for generated objects(based on surface normal), if zero - no generation slope limits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams SlopeMinMax;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams SlopeMinVariation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams SlopeMaxVariation;
/** Minimum and maximum distance(between previously generated objects in this slot) limit for generated objects, if zero - no generation distance limits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams DistanceBetweenMinMax;
/** Minimum and maximum denting depth of generated object into surface, if zero - no denting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams PressInGroundMinMax;
/** Minimum and maximum denting depth of generated object into surface by surface normal inverted direction, if zero - no denting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams PressInGroundNZMinMax;
/** Minimum and maximum adding position by Z coordinate of generated object position over surface by surface normal direction, if zero - position not changed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams AddZPosFGNMinMax;
/** Minimum and maximum rotation roll of generated object, if zero - rotation roll be zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomRotationRollMinMax;
/** Minimum and maximum rotation pitch of generated object, if zero - rotation pitch be zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomRotationPitchMinMax;
/** Minimum and maximum rotation yaw of generated object, if zero - rotation yaw be zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomRotationYawMinMax;
/** Minimum and maximum random non uniform scale of generated object by X coordinate, if zero - non uniform scaling by X not applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomNUScaleXMinMax;
/** Minimum and maximum random non uniform scale of generated object by Y coordinate, if zero - non uniform scaling by Y not applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomNUScaleYMinMax;
/** Minimum and maximum random non uniform scale of generated object by Z coordinate, if zero - non uniform scaling by Z not applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomNUScaleZMinMax;
/** Minimum and maximum chance of generate object in current generation attempt(in generation cycle), if zero - objects will not be created, so this value must be non zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomGenerationCoofMinMax;
/** Minimum and maximum generation attempts per one generation request, if zero - objects will not be created, so this value must be non zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams RandomGenerationPowerMinMax;
/** Minimum and maximum surface normal angle to align object rotation to normal(works only if RotateToGroundNormal option is enabled), if zero - align object rotation to normal be at any surface normal angle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FPGSlotMinMaxParams AlignToNormalMaxAngleMinMax;
/** At now ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<FString> TerrainLayersAllowed;
/** Ids of surface types are allowed to create object on surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<uint8> SurfaceTypesAllowed;
/** Seed for all generation processes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	int32 RandomSeed;
/** Additional location to generated object position add */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FVector AdditionalLocationVector;
/** Additional rotation to generated object rotation add */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FRotator AdditionalRotation;
/** Align rotation of generated object by surface normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool RotateToGroundNormal;
/** Value to add to generated object position on Z coordinate by inverted ground normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float AddZPosFromGroundNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float SlopeOverMinGenChance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float SlopeOverMaxGenChance;
/** Max value of denting depth for generated object into surface by slope angle, bigger slope angle - higher denting*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float PressInGroundBySlopeForce;
/** Option to generate objects in air without ray casts for any surface check */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool FloatingInAirGeneration;
/** Option to do ray casts in inverse direction if no surface detected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool UnderTerrainFixGeneration;
/** Ray casts trace channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TEnumAsByte<ETraceTypeQuery> GenerationTraceChannel;
/** Generate objects on landscape */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bGenerateOnLandscape;
/** Generate objects on static meshes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bGenerateOnStaticMeshes;
/** Special collision check before generate object, use with DefCollisionSize and DefCollisionHeightSize */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bCollisionCheckBeforeGenerate;
/** Default collision size(sphere), used with CollisionCheckBeforeGenerate and in other generation slots processing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float DefCollisionSize;
/** Default collision height size(ray), used only with CollisionCheckBeforeGenerate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	float DefCollisionHeightSize;
/** Special collision check option, if any other procedurally created meshes/actors be detected this check will fall and generation attempt failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bCollisionCheckExcludeOnlyOtherPGMeshes;
/** Special collision check option, if any(include self) procedurally created meshes/actors be detected this check will fall and generation attempt failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bCollisionCheckExcludeOnlyAllPGMeshes;
/** Use denting of generated object into surface by surface normal inverted direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bPressInGroundByNormalZ;
/** Use multi(does not stop at the first collision) ray casts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bUseMultiLT;
/** Allow to generate objects on other generated actors/components with this plugin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bAllowGenerationOnOtherGenActors;
/** Option for UseMultiLT, first ray cast collision be preferred */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bPrefFirstHit;
/** Option for UseMultiLT, ray cast collision with landscape be preferred */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bPrefLandscape;
/** This generation slot unique id, can be used in others slots processing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	int32 SlotUniqueId;
/** Ids on other generation slots which will be taken into account in generation of current slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	TArray<int32> TakenIntoAccSlotsIds;
/** Option to disable generation on this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bIsActive;
	//UKismetMathLibrary::SetRandomStreamSeed
	FRandomStream CurrentGenerationStream;

	TArray<AActor*>GeneratedActorsPtrs;

	TArray<FTransform> TempTransformsForObjects;
};

UCLASS(BlueprintType, Blueprintable, MinimalAPI)
class UPGSObj : public UObject
{
	//GENERATED_UCLASS_BODY()
	GENERATED_BODY()

public:
	UPGSObj();
	//UPGSObj(const class FObjectInitializer& ObjectInitializer);
	/*UPGSObj(const FObjectInitializer& ObjectInitializer)
	{

	}*/
	virtual void BeginDestroy() override;


	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintCallable, Category = Generation)
	void PrepareToGeneration();
	UFUNCTION(BlueprintCallable, Category = Generation)
	void RequestGenerateInBBox(const TArray<FVector>& GenerationBBoxPoints, /*const */UWorld* pGenerationWorld, float GenerationPower, AActor* OptProcGenActor = nullptr);
	//UFUNCTION(BlueprintCallable, Category = Generation)
	void RequestGenerateInBBoxWithShapeBorder(const TArray<FVector>& GenerationBBoxPoints, /*const */UWorld* pGenerationWorld, float GenerationPower, FPoly& ShapeBorder,
		TArray<FPoly>& ExclusionBorders, AActor* OptProcGenActor = nullptr, FVector OptGenerationDir = FVector::ZeroVector, FVector OptAlignDir = FVector::ZeroVector,
		float OptGenDirTraceMaxDist = 0.0f, bool bOptGenDirNoOutOfBounds = false, float OptAlignYaw = 0.0f);
	UFUNCTION(BlueprintCallable, Category = End)
	void PrepareToDestroy();
	UFUNCTION(BlueprintCallable, Category = End)
	void FreezeCreatedObjectsInEditor();

	FProcGenSlotParams* GetProcGenSlotByUId(int32 Id);
	TArray<FProcGenSlotParams*> GetProcGenSlotsByUIds(const TArray<int32>& IdsArr);

	FVector RandomPointInBoxByGenSlotParams(FProcGenSlotParams& SlotParams, FBox& BBox);
	bool RandomBoolByGenSlotParams(FProcGenSlotParams& SlotParams, float fChance);

	static bool LinePolyIntersection(TArray<FPoly>& PolyObjs, const FVector& Start, const FVector& End);

	void RemoveTempTrasfFromSlots(const FTransform& TrasfToRemove);
/** Array of Generation slots(as slots params), for successful generation minimum one slot required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	TArray<FProcGenSlotParams> GenerationParamsArr;

/** Generation of object(if objects are actors) be delayed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bDelayedGeneration;
/** Option to place generated actors in editor scene outliner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)
	bool bPlaceActorsInEditor;

	bool bIsPreparedToGeneration;
};
