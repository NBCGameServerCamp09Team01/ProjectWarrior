#include "WarriorAttackAreaTypes.h"
#include "DrawDebugHelpers.h"

float FWarriorAttackAreaData::GetBoundingRadius() const
{
    float PlanarRadius = Radius;

    if (Shape == EWarriorAttackAreaShape::Box)
    {
        PlanarRadius = FMath::Sqrt(FMath::Square(Length) + FMath::Square(Width * 0.5f));
    }

    return FMath::Max(PlanarRadius, Height * 0.5f);
}

bool FWarriorAttackAreaData::IsInside(const FTransform& AreaTransform, const FVector& WorldLocation, float InRadius, float InHalfHeight) const
{
    const FVector LocalLocation = AreaTransform.InverseTransformPositionNoScale(WorldLocation);

    if (FMath::Abs(LocalLocation.Z) > Height * 0.5f + InHalfHeight)
    {
        return false;
    }

    const float PlanarDistance = FVector2D(LocalLocation.X, LocalLocation.Y).Size();

    switch (Shape)
    {
    case EWarriorAttackAreaShape::Circle:
        return PlanarDistance <= Radius + InRadius;

    case EWarriorAttackAreaShape::Box:
        return LocalLocation.X >= -InRadius
            && LocalLocation.X <= Length + InRadius
            && FMath::Abs(LocalLocation.Y) <= Width * 0.5f + InRadius;

    case EWarriorAttackAreaShape::Cone:
    {
        if (PlanarDistance > Radius + InRadius)
        {
            return false;
        }

        // 원점에 걸쳐 있거나 전방위 부채꼴이면 각도 판정 불필요
        if (PlanarDistance <= InRadius || ConeAngle >= 360.f)
        {
            return true;
        }

        const float AngleToLocation = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(LocalLocation.Y), LocalLocation.X));

        // 캡슐 반지름만큼 부채꼴 가장자리 밖으로 걸친 경우도 포함
        const float RadiusSlackAngle = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(InRadius / PlanarDistance, 0.f, 1.f)));

        return AngleToLocation <= ConeAngle * 0.5f + RadiusSlackAngle;
    }

    default:
        return false;
    }
}

void FWarriorAttackAreaData::DrawDebug(const UWorld* World, const FTransform& AreaTransform, const FColor& Color, float Duration) const
{
#if ENABLE_DRAW_DEBUG
    if (!World)
    {
        return;
    }

    const FVector Origin = AreaTransform.GetLocation();
    const FVector Forward = AreaTransform.GetUnitAxis(EAxis::X);
    const FVector Right = AreaTransform.GetUnitAxis(EAxis::Y);

    switch (Shape)
    {
    case EWarriorAttackAreaShape::Circle:
        DrawDebugCircle(World, Origin, Radius, 32, Color, false, Duration, 0, 3.f, Forward, Right, false);
        break;

    case EWarriorAttackAreaShape::Box:
        DrawDebugBox(World, Origin + Forward * Length * 0.5f, FVector(Length * 0.5f, Width * 0.5f, Height * 0.5f), AreaTransform.GetRotation(), Color, false, Duration, 0, 3.f);
        break;

    case EWarriorAttackAreaShape::Cone:
    {
        const int32 NumSegments = 16;
        const float HalfAngle = FMath::Min(ConeAngle, 360.f) * 0.5f;

        FVector PrevPoint = Origin + Forward.RotateAngleAxis(-HalfAngle, FVector::UpVector) * Radius;
        DrawDebugLine(World, Origin, PrevPoint, Color, false, Duration, 0, 3.f);

        for (int32 Index = 1; Index <= NumSegments; ++Index)
        {
            const float Angle = -HalfAngle + (HalfAngle * 2.f) * Index / NumSegments;
            const FVector Point = Origin + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Radius;

            DrawDebugLine(World, PrevPoint, Point, Color, false, Duration, 0, 3.f);
            PrevPoint = Point;
        }

        DrawDebugLine(World, Origin, PrevPoint, Color, false, Duration, 0, 3.f);
        break;
    }

    default:
        break;
    }
#endif
}
