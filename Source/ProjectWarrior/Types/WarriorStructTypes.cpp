#include "WarriorStructTypes.h"
#include "ProjectWarrior/AbilitySystem/Abilities/WarriorGameplayAbility.h"

bool FWarriorPlayerAbilitySet::IsValid() const
{
    return InputTag.IsValid() && AbilityToGrant;
}