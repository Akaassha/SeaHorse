// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHBlueprintFunctionLibrary.h"

bool USHBlueprintFunctionLibrary::IsEditorWorld(const UObject* WorldContextObject)
{
#if WITH_EDITOR
    if (!IsValid(WorldContextObject))
    {
        return false;
    }

    const UWorld* World = WorldContextObject->GetWorld();

    return IsValid(World) &&
        (World->WorldType == EWorldType::Editor ||
            World->WorldType == EWorldType::EditorPreview);
#else
    return false;
#endif
}
