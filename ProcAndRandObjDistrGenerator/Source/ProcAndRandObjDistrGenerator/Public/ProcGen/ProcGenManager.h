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
enum class EProcGenPaintMode : uint8
{
	Default = 0,
	Paint,
	Clear,
	All
};


UCLASS(BlueprintType, Blueprintable, MinimalAPI)
class UProcGenManager : public UObject, public FTickableGameObject
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

	// paint by default
	void RequestPaint(bool bPaintOrClearBrush = true);

	void CreateActorByParams(const FActorToDelayedCreateParams& DelayedCreateParams, UWorld* CurWorldPtr);

	static UProcGenManager* GetCurManager();

	void SetCurManagerAsThisObject();

	bool IsActorAreGenerator(AActor* pActor);
	bool IsActorCreatedByGenerator(AActor* pActor);
	void AddProceduralTagToActor(AActor* pActor);

	void RemoveAllProcGeneratedActors();
	//UFUNCTION(BlueprintCallable, Category = Generation)

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GenerationParams)


	TQueue<FActorToDelayedCreateParams> DelayedCreateActorsQueue;

	TArray<TWeakObjectPtr<class AProcGenActor>> ProcGenActorsPtrs;
	TArray<TWeakObjectPtr<class AProcGenActor>> SelectedToPaintProcGenActorsPtrs;

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
};
