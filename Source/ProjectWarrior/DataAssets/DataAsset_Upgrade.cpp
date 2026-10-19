// DataAsset_Upgrade.cpp
#include "DataAsset_Upgrade.h"

int32 UDataAsset_Upgrade::GetNextCost(int32 InCurrentLevel) const
{
	if (InCurrentLevel < 0 || InCurrentLevel >= MaxLevel)
	{
		return -1;
	}

	switch (CostGrowth)
	{
	case EWarriorUpgradeCostGrowth::Exponential:
		return FMath::RoundToInt(BaseCost * FMath::Pow(CostMultiplier, static_cast<float>(InCurrentLevel)));
	case EWarriorUpgradeCostGrowth::Linear:
	default:
		return BaseCost + CostStep * InCurrentLevel;
	}
}

float UDataAsset_Upgrade::GetValueAtLevel(int32 InLevel) const
{
	return ValuePerLevel * FMath::Clamp(InLevel, 0, MaxLevel);
}