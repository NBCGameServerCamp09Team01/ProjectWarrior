#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WarriorStageWaveTypes.h"
#include "WarriorStageWaveDataAsset.generated.h"

UCLASS(BlueprintType)
class PROJECTWARRIOR_API UWarriorStageWaveDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Warrior|Stage")
	bool GetWaveData(int32 InWaveIndex, FWarriorStageWaveData& OutWaveData) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Warrior|Stage")
	int32 GetWaveCount() const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (AllowPrivateAccess = "true"))
	TArray<FWarriorStageWaveData> Waves;
};
