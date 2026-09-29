#include "DataAsset_Item.h"

FPrimaryAssetId UDataAsset_Item::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Item"), GetFName());
}