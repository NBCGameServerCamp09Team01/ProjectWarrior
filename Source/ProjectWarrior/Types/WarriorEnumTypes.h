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

// 막기 규칙. PerfectParryOnly는 막기 직후 짧은 구간(Player.Status.Blocking.Perfect)에만 막을 수 있음
UENUM(BlueprintType)
enum class EWarriorBlockRule : uint8
{
    Blockable,
    PerfectParryOnly,
    Unblockable
};

UENUM(BlueprintType)
enum class EWarriorHitResultType : uint8
{
    Hit,
    Blocked,
    Dodged,
    Invalid
};

UENUM(BlueprintType)
enum class EWarriorPurchaseResult : uint8
{
    Success,
    InvalidItem,
    NotEnoughGold,
    InventoryFull,
    MaxLevel
};
