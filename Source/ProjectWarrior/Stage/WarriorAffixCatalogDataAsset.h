// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WarriorAffixCatalogDataAsset.generated.h"

class UGameplayEffect;
class UMaterialInterface;
class UNiagaraSystem;

//수식어 등급. 서버에는 등급 칸이 없어 붙은 수식어 수로 정한다(1개 = 챔피언, 2개 이상 = 희귀)
UENUM(BlueprintType)
enum class EWarriorAffixTier : uint8
{
	None,
	Champion,
	Rare
};

//수식어 하나의 효과·표시
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAffixDefinition
{
	GENERATED_BODY()

	//체력바 수식어 줄에 보이는 이름(예: 화염)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FText DisplayName;

	//수식어를 가진 적에게 붙는 태그(AI.Affix.*). 다른 코드가 "이 적이 화염인가"를 물을 때 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix", meta = (Categories = "AI.Affix"))
	FGameplayTag AffixTag;

	//적 자신에게 거는 GE(무한, 능력치 배율 등). 시작 GE·난이도 배율 뒤에 적용된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix|Self")
	TArray<TSubclassOf<UGameplayEffect>> SelfEffects;

	//이 적에게 맞은 플레이어에게 거는 GE(화상·둔화 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix|OnHit")
	TSubclassOf<UGameplayEffect> OnHitEffect;

	//준 피해 중 자기 체력으로 돌리는 비율(흡혈, 0~1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix|OnHit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LifeStealRatio = 0.f;

	//오라 색(NS_AffixAura의 User.AuraColor). 여러 수식어면 첫 번째 것만 보인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix|Display")
	FLinearColor AuraColor = FLinearColor::White;
};

/**
 * 수식어 카탈로그: 서버 수식어 키(affix.*) → 효과·표시(stage-api.md v1.6 "수식어").
 * 서버에는 수식어 목록이 없고 키 모양만 검사하므로, 어떤 수식어가 있는지는 이 표가 정한다. 표에 없는 키가 오면 그 수식어만 빼고 경고한다.
 * 경로는 UWarriorStageSettings::AffixCatalog.
 */
UCLASS(BlueprintType)
class PROJECTWARRIOR_API UWarriorAffixCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	//표에 없으면 nullptr
	const FWarriorAffixDefinition* FindAffix(FName InAffixId) const;

	//수식어 키 모양: affix.으로 시작, 영문 소문자·숫자·점, 1~64자(서버 기동 검사 W8과 같은 규칙)
	static bool IsValidAffixId(const FString& InAffixId);

	//붙은(카탈로그에 있는) 수식어 수 → 등급
	static EWarriorAffixTier GetTierForCount(int32 InAffixCount);

	FLinearColor GetTierColor(EWarriorAffixTier InTier) const;

	UMaterialInterface* GetOutlineMaterial() const { return OutlineMaterial; }
	UNiagaraSystem* GetAuraSystem() const { return AuraSystem; }
	float GetAuraIntensity() const { return AuraIntensity; }

	//설정(UWarriorStageSettings::AffixCatalog)의 카탈로그. 지정하지 않았거나 읽지 못하면 nullptr
	static const UWarriorAffixCatalogDataAsset* LoadFromSettings();

	//자동화 시험용: 에셋 없이 표를 채운다
	void SetAffixesForTest(const TMap<FName, FWarriorAffixDefinition>& InAffixes) { Affixes = InAffixes; }

#if WITH_EDITOR
	//키 모양, 태그 지정 여부를 검사한다(에디터 저장·Data Validation)
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	//수식어 키 → 정의. 키는 서버 stage-waves.json의 affixes와 같게 쓴다(예: affix.fire)
	UPROPERTY(EditAnywhere, Category = "Warrior|Affix", meta = (AllowPrivateAccess = "true"))
	TMap<FName, FWarriorAffixDefinition> Affixes;

	//등급 색(체력바 줄·외곽선). 챔피언 파랑, 희귀 금색
	UPROPERTY(EditAnywhere, Category = "Warrior|Affix|Tier", meta = (AllowPrivateAccess = "true"))
	FLinearColor ChampionColor = FLinearColor(0.3f, 0.6f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Warrior|Affix|Tier", meta = (AllowPrivateAccess = "true"))
	FLinearColor RareColor = FLinearColor(1.0f, 0.8f, 0.2f);

	//수식어 적의 보이는 메시에 거는 오버레이(등급 색 가장자리 발광, 벡터 파라미터 TierColor). 블레이드 모드 약점선이 잠깐 대신 쓴다
	UPROPERTY(EditAnywhere, Category = "Warrior|Affix|Display", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> OutlineMaterial;

	//보이는 메시를 따라 붙는 오라(NS_AffixAura, 메시는 붙은 부모에서 찾음). 색은 첫 수식어의 AuraColor × AuraIntensity → User.AuraColor
	UPROPERTY(EditAnywhere, Category = "Warrior|Affix|Display", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> AuraSystem;

	//불꽃 스프라이트가 밝은(HDR) 색이라 0~1 색에 곱한다
	UPROPERTY(EditAnywhere, Category = "Warrior|Affix|Display", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AuraIntensity = 3.f;
};
