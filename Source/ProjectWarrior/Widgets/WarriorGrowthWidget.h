#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Account/WarriorAccountTypes.h"
#include "WarriorGrowthWidget.generated.h"

/**
 * 성장 화면의 베이스. 스탯 포인트로 계정 스탯에 투자한다.
 * 값은 UWarriorAccountSubsystem에서 읽고, 바뀌면 BP_OnAccountRefreshed로 다시 그린다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGrowthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//스탯 하나에 1포인트 투자. 포인트 부족·최대치면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	bool RequestInvest(FGameplayTag StatTag);

	//메인메뉴로 돌아간다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	void RequestBack();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//화면이 열릴 때와 계정 값이 바뀔 때 호출. BP에서 포인트·각 스탯 줄을 갱신한다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Growth")
	void BP_OnAccountRefreshed(const FWarriorAccountData& AccountData);

	UFUNCTION()
	void HandleAccountChanged(const FWarriorAccountData& InAccountData);
};