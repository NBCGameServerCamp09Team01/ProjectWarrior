#pragma once

UENUM()
enum class EWarriorConfirmType : uint8
{
    Yes,
    No
};

UENUM()
enum class EWarriorValidType : uint8
{
    Valid,
    Invalid
};

UENUM()
enum class EWarriorSuccessType : uint8
{
    Successful,
    Failed
};

UENUM(BlueprintType)
enum class EWarriorHitResultType : uint8
{
    Hit,
    Blocked,
    Dodged,
    Invalid
};