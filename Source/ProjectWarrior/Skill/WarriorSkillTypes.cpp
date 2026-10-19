#include "WarriorSkillTypes.h"

#define LOCTEXT_NAMESPACE "WarriorSkill"

FText FWarriorSkillRequirement::GetDisplayText() const
{
	if (!DisplayText.IsEmpty())
	{
		return DisplayText;
	}

	const FText Value = FText::AsNumber(FMath::RoundToInt(TargetValue));
	const FText KeyText = FText::FromName(Key);

	switch (Type)
	{
	case EWarriorSkillRequirementType::AccountLevel:
		return FText::Format(LOCTEXT("Req_AccountLevel", "계정 레벨 {0} 이상"), Value);
	case EWarriorSkillRequirementType::TotalKills:
		return Key.IsNone()
			? FText::Format(LOCTEXT("Req_TotalKills", "누적 처치 {0} 이상"), Value)
			: FText::Format(LOCTEXT("Req_TotalKillsByType", "{1} 처치 {0} 이상"), Value, KeyText);
	case EWarriorSkillRequirementType::KillsByDeathType:
		return FText::Format(LOCTEXT("Req_KillsByDeathType", "{1} 처치 {0} 이상"), Value, KeyText);
	case EWarriorSkillRequirementType::StagesCleared:
		return FText::Format(LOCTEXT("Req_StagesCleared", "스테이지 클리어 {0}회 이상"), Value);
	case EWarriorSkillRequirementType::PlayTimeSeconds:
		return FText::Format(LOCTEXT("Req_PlayTime", "누적 플레이 {0}분 이상"), FText::AsNumber(FMath::CeilToInt(TargetValue / 60.0)));
	case EWarriorSkillRequirementType::MaxWaveReached:
		return FText::Format(LOCTEXT("Req_MaxWave", "웨이브 {0} 도달"), Value);
	case EWarriorSkillRequirementType::BestClearTime:
		return FText::Format(LOCTEXT("Req_BestClearTime", "{0}초 이내 클리어"), Value);
	case EWarriorSkillRequirementType::TotalDamage:
		return FText::Format(LOCTEXT("Req_TotalDamage", "누적 피해 {0} 이상"), Value);
	case EWarriorSkillRequirementType::MaxDamage:
		return FText::Format(LOCTEXT("Req_MaxDamage", "한 번에 피해 {0} 이상"), Value);
	case EWarriorSkillRequirementType::ExtraStat:
		return FText::Format(LOCTEXT("Req_ExtraStat", "{1} {0} 이상"), Value, FText::FromString(StatTag.ToString()));
	}
	return Value;
}

double FWarriorSkillRequirementProgress::GetRatio() const
{
	if (bUpperBound || Target <= 0.0)
	{
		return bMet ? 1.0 : 0.0;
	}
	return FMath::Clamp(Current / Target, 0.0, 1.0);
}

#undef LOCTEXT_NAMESPACE
