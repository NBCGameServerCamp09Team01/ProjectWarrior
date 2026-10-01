#pragma once

#include "CoreMinimal.h"
#include "WarriorAttackAreaTypes.generated.h"

UENUM(BlueprintType)
enum class EWarriorAttackAreaShape : uint8
{
    // 기준점 중심의 원
    Circle,
    // 기준점에서 정면으로 Length만큼 뻗는 사각형
    Box,
    // 기준점에서 정면으로 펼쳐지는 부채꼴
    Cone
};

UENUM(BlueprintType)
enum class EWarriorAttackAreaAnchor : uint8
{
    // 공격자를 따라 움직임 (제자리 회전베기 등)
    Owner,
    // 표시 시작 시점의 공격자 위치·방향에 고정 (돌진 경로 등)
    OwnerSnapshot,
    // 표시 시작 시점의 대상 위치에 고정, 방향은 공격자 -> 대상 (도약 내려찍기 등)
    TargetSnapshot
};

/**
 * 범위 공격 한 번의 판정 영역. 위험 범위 표시(AWarriorAttackIndicator)와 실제 판정이 같은 데이터를 사용한다.
 * 영역 좌표계: X = 정면, Y = 오른쪽, Z = 위. 원점은 Anchor 위치 + LocalOffset.
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAttackAreaData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea")
    EWarriorAttackAreaShape Shape = EWarriorAttackAreaShape::Circle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea")
    EWarriorAttackAreaAnchor Anchor = EWarriorAttackAreaAnchor::Owner;

    // Circle, Cone의 반지름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea", meta = (ClampMin = "0.0", EditCondition = "Shape != EWarriorAttackAreaShape::Box", EditConditionHides))
    float Radius = 300.f;

    // Box의 정면 방향 길이
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea", meta = (ClampMin = "0.0", EditCondition = "Shape == EWarriorAttackAreaShape::Box", EditConditionHides))
    float Length = 600.f;

    // Box의 폭
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea", meta = (ClampMin = "0.0", EditCondition = "Shape == EWarriorAttackAreaShape::Box", EditConditionHides))
    float Width = 200.f;

    // Cone의 전체 각도 (정면 기준 좌우 절반씩)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea", meta = (ClampMin = "0.0", ClampMax = "360.0", EditCondition = "Shape == EWarriorAttackAreaShape::Cone", EditConditionHides))
    float ConeAngle = 90.f;

    // 판정 높이. 원점 기준 위아래로 절반씩
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea", meta = (ClampMin = "0.0"))
    float Height = 300.f;

    // Anchor 기준 로컬 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea")
    FVector LocalOffset = FVector::ZeroVector;

    // true면 막기로 막을 수 없음 (회피는 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackArea")
    bool bUnblockable = true;

    // 판정 영역을 모두 감싸는 구의 반지름 (원점 기준, 오버랩 쿼리용)
    float GetBoundingRadius() const;

    // AreaTransform 기준으로 캡슐(반지름 InRadius, 반높이 InHalfHeight)이 영역에 걸치는지
    bool IsInside(const FTransform& AreaTransform, const FVector& WorldLocation, float InRadius = 0.f, float InHalfHeight = 0.f) const;

    void DrawDebug(const UWorld* World, const FTransform& AreaTransform, const FColor& Color, float Duration) const;
};
