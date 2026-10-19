// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WarriorMonsterCatalogDataAsset.generated.h"

class AWarriorAICharacter;

/**
 * 몬스터 카탈로그(M24): 서버 몬스터 키(monsterId) → 적 BP.
 * 서버가 판 응답의 waves로 몬스터 키를 보내면 이 표에서 BP를 찾는다(루트 docs/contracts/stage-api.md "키"·"서버 정의 파일").
 * 키는 서버 master/monsters.json과 같은 문자열이다. 서버에 있는데 여기 없는 키가 오면 그 판은 시작하지 않는다(STAGE_WAVE_MONSTER_UNKNOWN).
 * 경로는 UWarriorStageSettings::MonsterCatalog. 에셋(DA_MonsterCatalog)은 a가 만들고 채운다.
 */
UCLASS(BlueprintType)
class PROJECTWARRIOR_API UWarriorMonsterCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	//몬스터 키의 BP. 표에 없으면 빈 값
	TSoftClassPtr<AWarriorAICharacter> FindMonster(FName InMonsterId) const;

	//표에 있고 BP가 지정된 키인가
	bool HasMonster(FName InMonsterId) const;

	//몬스터 키 모양: monster.으로 시작, 영문 소문자·숫자·점, 1~64자(서버 기동 검사와 같은 규칙)
	static bool IsValidMonsterId(const FString& InMonsterId);

	//설정(UWarriorStageSettings::MonsterCatalog)의 카탈로그. 지정하지 않았거나 읽지 못하면 nullptr
	static const UWarriorMonsterCatalogDataAsset* LoadFromSettings();

	//자동화 시험용: 에셋 없이 표를 채운다
	void SetMonstersForTest(const TMap<FName, TSoftClassPtr<AWarriorAICharacter>>& InMonsters) { Monsters = InMonsters; }

#if WITH_EDITOR
	//키 모양, BP 지정 여부를 검사한다(에디터 저장·Data Validation)
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	//몬스터 키 → 적 BP. 키는 서버 monsters.json의 monsterId와 같게 쓴다(예: monster.samurai)
	UPROPERTY(EditAnywhere, Category = "Warrior|Stage", meta = (AllowPrivateAccess = "true"))
	TMap<FName, TSoftClassPtr<AWarriorAICharacter>> Monsters;
};
