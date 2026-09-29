// Fill out your copyright notice in the Description page of Project Settings.


#include "BowBase.h"

ABowBase::ABowBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USkeletalMeshComponent>(AWeaponBase::WeaponMeshComponentName))
{
}
