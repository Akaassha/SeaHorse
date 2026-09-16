#include "SHCreateMagicCircleCommandlet.h"
#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_SpriteFacingAndAlignment.h"
#include "Stateless/Modules/NiagaraStatelessModule_DynamicMaterialParameters.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#endif

USHCreateMagicCircleCommandlet::USHCreateMagicCircleCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 USHCreateMagicCircleCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    const FString Path = TEXT("/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle");
    if (FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite %s"), *Path); return 1; }
    auto* Source = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight.MinimalLightweight"));
    auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeaHorse/VFX/MagicCircle/MI_MagicCircle.MI_MagicCircle"));
    if (!Source || !Material) { UE_LOG(LogTemp, Error, TEXT("Missing template or material")); return 1; }
    UPackage* Package = CreatePackage(*Path);
    UNiagaraSystem* System = DuplicateObject<UNiagaraSystem>(Source, Package, TEXT("NS_MagicCircle"));
    System->SetFlags(RF_Public | RF_Standalone);
    System->SetFixedBounds(FBox(FVector(-64, -64, -32), FVector(64, 64, 64)));
    for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        UNiagaraStatelessEmitter* Emitter = Handle.GetStatelessEmitter();
        if (!Emitter) { UE_LOG(LogTemp, Error, TEXT("Expected lightweight emitter")); return 1; }
        auto* StateProperty = FindFProperty<FStructProperty>(Emitter->GetClass(), TEXT("EmitterState"));
        auto* State = StateProperty->ContainerPtrToValuePtr<FNiagaraEmitterStateData>(Emitter);
        State->LoopBehavior = ENiagaraLoopBehavior::Once;
        State->LoopDuration = FNiagaraDistributionRangeFloat(1.5f);
        for (int32 i = 0; i < Emitter->GetNumSpawnInfos(); ++i) Emitter->GetSpawnInfoByIndex(i)->bEnabled = false;
        auto& Spawn = Emitter->AddSpawnInfo();
        Spawn.Type = ENiagaraStatelessSpawnInfoType::Burst;
        Spawn.Amount = FNiagaraDistributionRangeInt(1);
        Spawn.SpawnTime = 0.0f;
        for (UNiagaraStatelessModule* Module : Emitter->GetModules())
        {
            if (Module->CanDisableModule()) Module->SetIsModuleEnabled(false);
        }
        auto* Init = Cast<UNiagaraStatelessModule_InitializeParticle>(Emitter->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()));
        if (!Init) { UE_LOG(LogTemp, Error, TEXT("Missing Initialize Particle")); return 1; }
        Init->SetIsModuleEnabled(true);
        Init->LifetimeDistribution = FNiagaraDistributionRangeFloat(1.5f);
        Init->SpriteSizeDistribution.InitConstant(FVector2f(32,32));
        Init->InitialPositionDistribution = FNiagaraDistributionPosition(FVector3f(0,0,12));
        Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor::White);
        // Lightweight emitters do not output Particles.NormalizedAge. Feed material age explicitly.
        auto* Dynamic = Cast<UNiagaraStatelessModule_DynamicMaterialParameters>(Emitter->GetModule(UNiagaraStatelessModule_DynamicMaterialParameters::StaticClass()));
        if (!Dynamic) { UE_LOG(LogTemp, Error, TEXT("Template lacks Dynamic Material Parameters module")); return 1; }
        Dynamic->SetIsModuleEnabled(true);
        Dynamic->bParameter0Enabled = true;
        Dynamic->Parameter0.XChannelDistribution.InitCurve({0.0f, 1.0f});
        auto* Facing = Cast<UNiagaraStatelessModule_SpriteFacingAndAlignment>(Emitter->GetModule(UNiagaraStatelessModule_SpriteFacingAndAlignment::StaticClass()));
        if (!Facing) { UE_LOG(LogTemp, Error, TEXT("Template lacks Sprite Facing module")); return 1; }
        Facing->SetIsModuleEnabled(true);
        Facing->bSpriteFacingEnabled = true;
        Facing->bSpriteAlignmentEnabled = true;
        Facing->SpriteFacing.InitConstant(FVector3f(0,0,1));
        Facing->SpriteAlignment.InitConstant(FVector3f(0,1,0));
        for (auto* Renderer : Emitter->GetRenderers())
        {
            if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
            {
                Sprite->Material = Material;
                Sprite->FacingMode = ENiagaraSpriteFacingMode::CustomFacingVector;
                Sprite->Alignment = ENiagaraSpriteAlignment::CustomAlignment;
                Sprite->PostEditChange();
            }
        }
        Emitter->PostEditChange();
    }
    System->PostEditChange();
    System->RequestCompile(true);
    System->WaitForCompilationComplete();
    FAssetRegistryModule::AssetCreated(System);
    Package->MarkPackageDirty();
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, System, *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()), SaveArgs);
    UE_LOG(LogTemp, Display, TEXT("MAGIC_CIRCLE_NIAGARA saved=%d valid=%d ready=%d emitters=%d"), bSaved, System->IsValid(), System->IsReadyToRun(), System->GetEmitterHandles().Num());
    return bSaved && System->IsValid() ? 0 : 1;
#else
    return 1;
#endif
}
