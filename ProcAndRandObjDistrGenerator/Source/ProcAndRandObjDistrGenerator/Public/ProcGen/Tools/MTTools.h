// Free to copy, edit and use in any work/projects, commercial or free

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeLock.h"
#include "HAL/CriticalSection.h"
#include "MTTools.generated.h"

/**
 * 
 */

UCLASS(BlueprintType, Blueprintable)
class UMTTools : public UObject
{
	GENERATED_BODY()

public:
	UMTTools()
	{

	}
};

template<class T>
class TArrayTS
{
	TArray<T> MainContainer;
	FCriticalSection ContainerOperationsLock;
public:
	TArrayTS() : MainContainer(), ContainerOperationsLock()
	{

	}

	void Add(const T& ArrItem)
	{
		FScopeLock lock(&ContainerOperationsLock);
		MainContainer.Add(ArrItem);
	}

	int32 Num()
	{
		FScopeLock lock(&ContainerOperationsLock);
		return MainContainer.Num();
	}

	T& operator[](int32 Index)
	{
		//SetLock call before this operation
		return MainContainer[Index];
	}

	void SetLock(bool EnableLock, bool bTry = true)
	{
		if (EnableLock)
		{
			if (bTry)
			{
				ContainerOperationsLock.TryLock();
			}
			else
			{
				ContainerOperationsLock.Lock();
			}
		}
		else
		{
			ContainerOperationsLock.Unlock();
		}
	}

	bool Contains(const T& ArrItem)
	{
		FScopeLock lock(&ContainerOperationsLock);
		return MainContainer.Contains(ArrItem);
	}

	int32 Find(const T& ArrItem)
	{
		FScopeLock lock(&ContainerOperationsLock);
		return MainContainer.Find(ArrItem);
	}

	bool RemoveSwap(const T& ArrItem)
	{
		FScopeLock lock(&ContainerOperationsLock);
		return MainContainer.RemoveSwap(ArrItem);
	}

	void RemoveAt(int32 AtIndex)
	{
		FScopeLock lock(&ContainerOperationsLock);
		MainContainer.RemoveAt(AtIndex);
	}

	void RemoveAtSwap(int32 AtIndex)
	{
		FScopeLock lock(&ContainerOperationsLock);
		MainContainer.RemoveAtSwap(AtIndex);
	}

	void Remove(const T& ArrItem)
	{
		FScopeLock lock(&ContainerOperationsLock);
		MainContainer.Remove(ArrItem);
	}

	void ClearArr()
	{
		FScopeLock lock(&ContainerOperationsLock);
		MainContainer.Empty();
	}

	TArray<T>& GetMainContainerForOps()
	{
		return MainContainer;
	}
};

class FVectorTS
{
	FVector VectorVar;
	FCriticalSection OperationsLock;
public:
	FVectorTS() : VectorVar(FVector::ZeroVector), OperationsLock()
	{

	}

	FVectorTS(const FVector& VVarNew) : VectorVar(VVarNew), OperationsLock()
	{

	}

	void SetVectorVar(const FVector& VVarNew)
	{
		FScopeLock lock(&OperationsLock);
		VectorVar = VVarNew;
	}

	FVector GetVectorVar()
	{
		FScopeLock lock(&OperationsLock);
		return VectorVar;
	}
};

template<class T>
class FGenericVariableTS
{
	T GVar;
	FCriticalSection OperationsLock;
public:
	FGenericVariableTS() : GVar(), OperationsLock()
	{

	}

	FGenericVariableTS(const T& VarNew) : GVar(VarNew), OperationsLock()
	{

	}

	void SetVar(const T& VarNew)
	{
		FScopeLock lock(&OperationsLock);
		GVar = VarNew;
	}

	T GetVar()
	{
		FScopeLock lock(&OperationsLock);
		return GVar;
	}
};