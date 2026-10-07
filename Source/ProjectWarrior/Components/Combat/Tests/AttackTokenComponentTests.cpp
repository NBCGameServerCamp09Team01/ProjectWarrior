#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Components/Combat/AttackTokenComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorAttackTokenComponentTest, "ProjectWarrior.Combat.AttackToken.Pools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorAttackTokenComponentTest::RunTest(const FString& Parameters)
{
	// 별도 월드에서 시간을 흘리지 않고(월드 시간 0) 기본 풀 설정(근접 2, 원거리 1)으로 검증한다.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world"), World)) { return false; }

	AActor* Target = World->SpawnActor<AActor>();
	UAttackTokenComponent* Tokens = NewObject<UAttackTokenComponent>(Target);
	Tokens->RegisterComponent();

	AActor* MeleeA = World->SpawnActor<AActor>();
	AActor* MeleeB = World->SpawnActor<AActor>();
	AActor* MeleeC = World->SpawnActor<AActor>();
	AActor* RangeA = World->SpawnActor<AActor>();
	AActor* RangeB = World->SpawnActor<AActor>();

	const FGameplayTag Melee = WarriorGameplayTags::AI_AttackToken_Melee;
	const FGameplayTag Range = WarriorGameplayTags::AI_AttackToken_Range;

	// 풀 용량까지만 나간다.
	TestTrue(TEXT("Melee A acquires"), Tokens->TryAcquire(MeleeA, Melee));
	TestTrue(TEXT("Melee B acquires"), Tokens->TryAcquire(MeleeB, Melee));
	TestFalse(TEXT("Melee C is denied at capacity"), Tokens->TryAcquire(MeleeC, Melee));
	TestEqual(TEXT("No melee tokens left"), Tokens->GetAvailableTokens(Melee), 0);

	// 이미 들고 있으면 다시 요청해도 중복 차감하지 않고, 다른 풀은 받지 못한다.
	TestTrue(TEXT("Holder re-acquire is idempotent"), Tokens->TryAcquire(MeleeA, Melee));
	TestFalse(TEXT("Holder cannot take another pool"), Tokens->TryAcquire(MeleeA, Range));

	// 풀은 서로 독립이다.
	TestTrue(TEXT("Range A acquires"), Tokens->TryAcquire(RangeA, Range));
	TestFalse(TEXT("Range B is denied at capacity"), Tokens->TryAcquire(RangeB, Range));

	// 비용이 남은 수보다 크면 거절한다.
	Tokens->Release(RangeA);
	TestFalse(TEXT("Cost over capacity is denied"), Tokens->TryAcquire(RangeB, Range, 2));

	// 두 번 받았으면(BT 데코레이터 + 어빌리티) 두 번 반납해야 풀로 돌아간다.
	Tokens->Release(MeleeA);
	TestTrue(TEXT("Still held after first of two releases"), Tokens->IsHolding(MeleeA));
	TestFalse(TEXT("Pool still full after first release"), Tokens->CanAcquire(MeleeC, Melee));

	// 반납하면 다른 AI가 받을 수 있고, 반납한 AI는 재획득 대기 시간 동안 받지 못한다.
	Tokens->Release(MeleeA);
	TestFalse(TEXT("Released after second release"), Tokens->IsHolding(MeleeA));
	TestFalse(TEXT("Released holder is on cooldown"), Tokens->CanAcquire(MeleeA, Melee));
	TestTrue(TEXT("Melee C acquires after release"), Tokens->TryAcquire(MeleeC, Melee));

	// 보유자가 사라지면 다음 요청 때 토큰이 회수된다.
	MeleeB->Destroy();
	AActor* MeleeD = World->SpawnActor<AActor>();
	TestTrue(TEXT("Destroyed holder's token is reclaimed"), Tokens->TryAcquire(MeleeD, Melee));

	// 등록되지 않은 풀은 제한하지 않는다.
	TestTrue(TEXT("Unconfigured pool is unlimited"), Tokens->CanAcquire(RangeB, WarriorGameplayTags::AI_AttackToken));

	Tokens->ReleaseAll();
	TestEqual(TEXT("ReleaseAll restores melee"), Tokens->GetAvailableTokens(Melee), 2);
	TestEqual(TEXT("ReleaseAll restores range"), Tokens->GetAvailableTokens(Range), 1);
	TestTrue(TEXT("ReleaseAll clears cooldowns"), Tokens->CanAcquire(MeleeA, Melee));

	World->DestroyWorld(false);
	return true;
}

#endif
