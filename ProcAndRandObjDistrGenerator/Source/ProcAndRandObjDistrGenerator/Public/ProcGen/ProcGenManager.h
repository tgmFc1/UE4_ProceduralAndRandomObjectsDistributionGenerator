// Free to copy, edit and use in any work/projects, commercial or free

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"
#include "Kismet/KismetMathLibrary.h"
#include "Tickable.h"
#include "GameFramework/Actor.h"
//#include "ProcGen/ProcGenActor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/ScopeLock.h"
#include "HAL/CriticalSection.h"
#include "ProcGen/Tools/MTTools.h"
#include "ProcGenManager.generated.h"

USTRUCT(BlueprintType)
struct FActorToDelayedCreateParams
{
	GENERATED_USTRUCT_BODY()

	FActorToDelayedCreateParams() : ActorTransform()//, LinkedProcGenActorPtr()
	{
		ActorClassPtr = nullptr;
		bSpawnInEditor = true;
		ParentProcGenSlotObjPtr = nullptr;
		ParentProcGenSlotParamsPtr = nullptr;
		LinkedProcGenActorPtr = nullptr;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	UClass* ActorClassPtr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	FTransform ActorTransform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	bool bSpawnInEditor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Params)
	class UPGSObj* ParentProcGenSlotObjPtr;

	struct FProcGenSlotParams* ParentProcGenSlotParamsPtr;

	//class AProcGenActor* LinkedProcGenActorPtr;
	TWeakObjectPtr<class AProcGenActor> LinkedProcGenActorPtr;
};

UENUM(BlueprintType)
enum class EGenCellSlotDataType : uint8
{
	ActorToGen = 0,
	SMeshToGen,
	DecalToGen,
};

struct GenerationGridInfoSlot
{
	GenerationGridInfoSlot() : SlotActors(), SlotComponents()
	{
	}

	TArray<TWeakObjectPtr<class AActor>> SlotActors;
	TArray<TWeakObjectPtr<class UObject>> SlotComponents;
};

struct FGenerationInCellSlotData
{
	FGenerationInCellSlotData() : ParentActorPtr(), DecalSize(0), SlotTransf(), GeneratedActorPtr(), GeneratedSMeshCompPtr(), GeneratedDecalCompPtr()
	{
		CellSlotDataType = EGenCellSlotDataType::ActorToGen;
		DecalMaterial = nullptr;
		StaticMeshPtr = nullptr;
		ActorToCreateClassPtr = nullptr;
		bSlotLoaded = false;
		InstSM_Id = -1;
		ParentCellId = -1;
	}
	EGenCellSlotDataType CellSlotDataType;
	TWeakObjectPtr<class AActor> ParentActorPtr;
	class UMaterialInterface* DecalMaterial;
	FVector DecalSize;
	class UStaticMesh* StaticMeshPtr;
	UClass* ActorToCreateClassPtr;
	FTransform SlotTransf;
	TWeakObjectPtr<class AActor> GeneratedActorPtr;
	TWeakObjectPtr<class UStaticMeshComponent> GeneratedSMeshCompPtr;
	TWeakObjectPtr<class UDecalComponent> GeneratedDecalCompPtr;
	bool bSlotLoaded;
	int32 InstSM_Id;
	int32 ParentCellId;
	//TSubclassOf<>

	void LoadSlotData();
	void UnloadSlotData();
};

struct FGenerationGridCellGenSlot
{
	FGenerationGridCellGenSlot() : SlotData(), NumSlotDataLoaded(0), TempTransformsForDistanceChecks()
	{

	}

	TArray<FGenerationInCellSlotData> SlotData;
	int32 NumSlotDataLoaded;
	TArray<FTransform> TempTransformsForDistanceChecks;

	void LoadSlotDataById(int32 Id);
	void UnloadSlotDataById(int32 Id);
};

struct FGenerationGridCell
{
	FGenerationGridCell() : CellSlotsInfo(), CellBounds(EForceInit::ForceInit), CellId(0)
	{

	}

	TMap<int32, FGenerationGridCellGenSlot> CellSlotsInfo;
	FBox CellBounds;
	int32 CellId;
};

UENUM(BlueprintType)
enum class EProcGenPaintMode : uint8
{
	Default = 0,
	Paint,
	Clear,
	All
};

class FGenerationHelperThread : public FRunnable
{
public:
	FGenerationHelperThread();
	virtual ~FGenerationHelperThread();

	void StartWork();
	void StopWork();

	bool IsRunning() { return bIsRunning; }

	/**
	 * FRunnable Interface
	 */
	virtual uint32 Run() override;
	virtual void Exit() override;

private:

	FThreadSafeBool bIsRunning;
	TUniquePtr<FRunnableThread> Thread;
public:
	FCriticalSection ThreadLock;
	FCriticalSection GridPtrsTL;
};


UCLASS(BlueprintType, Blueprintable)
class PROCANDRANDOBJDISTRGENERATOR_API UProcGenManager : public UObject, public FTickableGameObject
{
	//GENERATED_UCLASS_BODY()
	GENERATED_BODY()

public:
	UProcGenManager();
	//UProcGenManager(const class FObjectInitializer& ObjectInitializer);
	/*UProcGenManager(const FObjectInitializer& ObjectInitializer)
	{

	}*/

	virtual UWorld* GetWorld() const override;

	// FTickableGameObject begin
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	// FTickableGameObject end

	virtual void BeginDestroy() override;

	// paint by default
	void RequestPaint(bool bPaintOrClearBrush = true);

	void UpdateActiveAndInactiveGridCells();

	void CreateActorByParams(const FActorToDelayedCreateParams& DelayedCreateParams, UWorld* CurWorldPtr);

	static UProcGenManager* GetCurManager();

	void SetCurManagerAsThisObject();

	bool IsActorAreGenerator(AActor* pActor);
	bool IsActorCreatedByGenerator(AActor* pActor);
	void AddProceduralTagToActor(AActor* pActor);

	void RemoveAllProcGeneratedActors();

	void ClearGeneratorsCache();

	TArray<class AProcGenParamsModifierActor*> GetAllCollidedGenParamsModifierActors(const FBox& BoxToCollide);

	FGenerationGridCell* GetGenerationGridCellByLocation(const FVector& Location);
	FGenerationGridCell* GetGenerationGridCellInCustomArrByLocation(const FVector& Location, TArray<FGenerationGridCell*>& CustomCellsArr);
	TArray<FGenerationGridCell*> GetGridCellsArrByBounds(const FBox& Bounds);
	bool ClearGenerationGridCellsByLocation(const FVector& Location);
	bool ClearGenerationGridCellsByGenSlotId(int32 Id);
	FGenerationGridCell* GetNearestGridCellToPointFast(const FVector& Point);
	TArray<FGenerationGridCell*> GetNearestGridCellsGroupToPoint(const FVector& Point);
	static class UWorld* GetCurrentWorldStatic();
	class UWorld* GetWorldPRFEditor();
	UFUNCTION()
	void OnPIESessionStarted(const bool bStarted);
	UFUNCTION()
	void OnPIESessionEnded(const bool bEnded);
	UFUNCTION()
	void OnLevelLoadStarted(const FString& LevelName);
	//UFUNCTION(BlueprintCallable, Category = Generation)

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)


	TQueue<FActorToDelayedCreateParams> DelayedCreateActorsQueue;

	TArray<TWeakObjectPtr<class AProcGenActor>> ProcGenActorsPtrs;
	TArray<TWeakObjectPtr<class AProcGenActor>> SelectedToPaintProcGenActorsPtrs;
	TArray<TWeakObjectPtr<class AProcGenParamsModifierActor>> ProcGenParamsModifierActorsPtrs;

	bool bPaintMode;
	float PaintSphereSize;
	FVector CurrentPaintPos;
	FVector CurrentPaintCameraDir;
	EProcGenPaintMode CurProcGenPaintMode;
	bool ColPaintInNextFrame;

	bool bUseCameraDirToGenerationInPaintMode;
	bool bUseCameraDirToRotationAlignGenObjectsInPaintMode;
	bool bUseCollisionChecksInPaintRemoveMode;

	float TimeDelayBCreateNextPGActorsGroup;

	TArray<FGenerationGridCell> ArrGridCells;

	TArray<FGenerationGridCell*> ArrGridCellsCurrentlyActivePtrs;
	TArray<FGenerationGridCell*> ArrGridCellsCurrentlyInactivePtrs;

	FVector CurrentCameraViewPos;
	FCriticalSection CameraViewPosTL;

	FGenericVariableTS<FVector> CurrentCameraViewPosTSVar;

	bool bPIESessionStarted = false;

	TUniquePtr<FGenerationHelperThread> pGenerationHelperThread;
};

static TAutoConsoleVariable<int32> CVarGenerationGridXMin(
	TEXT("ProcGen.GenerationGridXMin"),
	-300000,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarGenerationGridYMin(
	TEXT("ProcGen.GenerationGridYMin"),
	-300000,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarGenerationGridXMax(
	TEXT("ProcGen.GenerationGridXMax"),
	300000,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarGenerationGridYMax(
	TEXT("ProcGen.GenerationGridYMax"),
	300000,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarGenerationGridCellXYSize(
	TEXT("ProcGen.GenerationGridCellXYSize"),
	2500,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarGenerationGridCellZSize(
	TEXT("ProcGen.GenerationGridCellZSize"),
	150000,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<float> CVarGenerationGridGrabBBoxSize(
	TEXT("ProcGen.GenerationGridGrabBBoxSize"),
	15000.0f,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<float> CVarGenerationGridGrabDistanceAround(
	TEXT("ProcGen.GenerationGridGrabDistanceAround"),
	10000.0f,
	TEXT("")
	TEXT("")
);

static TAutoConsoleVariable<bool> CVarGenerationGridEnableCellsDebugDraw(
	TEXT("ProcGen.GenerationGridEnableCellsDebugDraw"),
	false,
	TEXT("")
	TEXT("")
);