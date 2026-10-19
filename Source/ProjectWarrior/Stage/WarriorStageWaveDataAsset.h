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

	//실행 중에 웨이브를 채운다(M24: 서버 웨이브 구성을 담는 임시 에셋. NewObject로 만들고 저장하지 않는다)
	void SetWaves(const TArray<FWarriorStageWaveData>& InWaves) { Waves = InWaves; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (AllowPrivateAccess = "true"))
	TArray<FWarriorStageWaveData> Waves;
};
