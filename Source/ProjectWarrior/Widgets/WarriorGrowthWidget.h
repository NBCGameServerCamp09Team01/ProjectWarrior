#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Account/WarriorAccountTypes.h"
#include "WarriorGrowthWidget.generated.h"

/**
 * 성장 화면의 베이스. 스탯 포인트로 계정 스탯을 올리고 내린다.
 * 값은 UWarriorAccountSubsystem에서 읽고, 바뀌면 BP_OnAccountRefreshed로 다시 그린다.
 *
 * S3(서버 저장): + / −는 미리보기만 바꾸고, "적용"을 누르면 미리보기 묶음을 한 번에 서버로 보낸다(UWarriorStatInvestSubsystem).
 * - BP_OnAccountRefreshed에는 **미리보기를 반영한 값**(포인트·스탯)을 넘긴다. 그래서 기존 BP의 숫자 표시가 그대로 미리보기를 보여 준다.
 * - 서버 응답이 오면 서버 값으로 다시 그린다(미리보기와 다르면 서버 값). 성공하면 미리보기를 비운다. 거절이면 미리보기를 두고 BP_OnInvestFinished로 알린다.
 * - 응답을 기다리는 동안(IsApplying) + / − / 적용을 막는다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGrowthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorGrowthWidget(const FObjectInitializer& ObjectInitializer);

	//스탯 하나 +1(미리보기). 남은 포인트가 없거나 최대치면 false. 기존 + 버튼 바인딩을 그대로 쓰려고 이름을 유지한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	bool RequestInvest(FGameplayTag StatTag);

	//스탯 하나 −1(미리보기). 저장된 값 아래로도 내릴 수 있다(0까지, 포인트를 돌려받음). 0이면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	bool RemovePending(FGameplayTag StatTag);

	//미리보기 묶음을 서버에 보낸다. 보낼 것이 없거나 응답을 기다리는 중이면 false. 결과는 BP_OnInvestFinished
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	bool ApplyPending();

	//미리보기를 비우고 지금 값으로 다시 그린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	void CancelPending();

	//미리보기에서 바꾼 양(양수 올림, 음수 내림)
	UFUNCTION(BlueprintPure, Category = "Warrior|Growth")
	int32 GetPendingPoints(FGameplayTag StatTag) const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Growth")
	bool HasPending() const { return PendingPoints.Num() > 0; }

	//적용을 보내고 응답을 기다리는 중
	UFUNCTION(BlueprintPure, Category = "Warrior|Growth")
	bool IsApplying() const;

	//메인메뉴로 돌아간다. 미리보기는 버린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Growth")
	void RequestBack();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//화면이 열릴 때, 계정 값이 바뀔 때, 미리보기가 바뀔 때 호출. AccountData는 미리보기를 반영한 값이다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Growth")
	void BP_OnAccountRefreshed(const FWarriorAccountData& AccountData);

	//적용 결과. 실패면 Message를 화면에 띄운다(성공이면 빈 문구)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Growth")
	void BP_OnInvestFinished(bool bSuccess, const FText& Message);

	UFUNCTION()
	void HandleAccountChanged(const FWarriorAccountData& InAccountData);

	UFUNCTION()
	void HandleInvestCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

private:
	//지금 계정 값에 미리보기를 더해 BP에 넘긴다
	void RefreshPreview();

	//스탯 태그 → 미리보기에서 바꾼 양. 0이 되면 지운다
	TMap<FGameplayTag, int32> PendingPoints;
};
