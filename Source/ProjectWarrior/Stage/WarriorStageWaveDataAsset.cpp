#include "WarriorStageWaveDataAsset.h"

bool UWarriorStageWaveDataAsset::GetWaveData(const int32 InWaveIndex, FWarriorStageWaveData& OutWaveData) const
{
	if (!Waves.IsValidIndex(InWaveIndex))
	{
		return false;
	}

	OutWaveData = Waves[InWaveIndex];
	return true;
}

int32 UWarriorStageWaveDataAsset::GetWaveCount() const
{
	return Waves.Num();
}
