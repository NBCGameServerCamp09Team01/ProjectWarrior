#include "WarriorStatTags.h"

namespace WarriorStatTags
{
	// 공격
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Combat_Damage_Overkill, "Stat.Combat.Damage.Overkill", "남은 체력보다 초과로 들어간 데미지");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Combat_Balance_Dealt, "Stat.Combat.Balance.Dealt", "가한 자세(밸런스) 데미지");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Combat_Stagger_Caused, "Stat.Combat.Stagger.Caused", "그로기 유발 횟수");

	// 피격
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Defense_Health_MinRatio, "Stat.Defense.Health.MinRatio", "최저 체력 비율 (0~1)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Defense_Wave_NoHit, "Stat.Defense.Wave.NoHit", "피격 없이 끝난 웨이브 수");

	// 시간·진행
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Time_Rest, "Stat.Time.Rest", "휴식 상태로 보낸 시간(초)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Time_Shop, "Stat.Time.Shop", "상점을 연 시간(초)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Progress_Rest_Skipped, "Stat.Progress.Rest.Skipped", "휴식 건너뛰기 횟수");
}
