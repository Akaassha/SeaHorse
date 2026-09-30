#include "Gameplay/Player/SHPlayerRepresentation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/SHPlayerFaceWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Online/SHSteamAvatarSubsystem.h"
#include "RenderDeferredCleanup.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHPlayerFace, Log, All);

void ASHPlayerRepresentation::BeginDestroy()
{
	// Covers actors destroyed without BeginPlay/EndPlay, including editor previews.
	if (FaceRenderer)
	{
		BeginCleanup(FaceRenderer);
		FaceRenderer = nullptr;
	}
	Super::BeginDestroy();
}

void ASHPlayerRepresentation::RestoreFaceMaterial()
{
	if (UStaticMeshComponent* Mesh = BoundFaceMesh.Get())
	{
		if (BoundFaceSlotIndex != INDEX_NONE && Mesh->GetMaterial(BoundFaceSlotIndex) == PlayerFaceMaterial)
		{
			Mesh->SetMaterial(BoundFaceSlotIndex, OriginalFaceMaterial);
		}
	}
	BoundFaceMesh.Reset();
	BoundFaceSlotIndex = INDEX_NONE;
	OriginalFaceMaterial = nullptr;
	PlayerFaceMaterial = nullptr;
}

void ASHPlayerRepresentation::ReleaseFaceResources()
{
	RestoreFaceMaterial();
	PlayerFaceWidget = nullptr;
	PlayerFaceRenderTarget = nullptr;
	LoadedAvatar = nullptr;
	if (FaceRenderer)
	{
		// Slate rendering queues work on the render thread. Defer destruction until it is safe.
		BeginCleanup(FaceRenderer);
		FaceRenderer = nullptr;
	}
}

bool ASHPlayerRepresentation::SetPlayerFaceMesh(UStaticMeshComponent* Mesh)
{
	if (!IsValid(Mesh) || Mesh->GetOwner() != this || bPresentationEnded)
	{
		return false;
	}
	RestoreFaceMaterial();
	ExplicitFaceMesh = Mesh;
	RefreshPlayerFace();
	return IsValid(PlayerFaceMaterial);
}

bool ASHPlayerRepresentation::BindFaceMaterial()
{
	UStaticMeshComponent* Mesh = ExplicitFaceMesh.Get();
	if (!Mesh)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(this);
		for (UStaticMeshComponent* Candidate : Meshes)
		{
			if (Candidate->GetMaterialIndex(FaceMaterialSlot) != INDEX_NONE)
			{
				if (Mesh)
				{
					UE_LOG(LogSHPlayerFace, Warning, TEXT("%s: several meshes have slot %s. Call SetPlayerFaceMesh."),
						*GetName(), *FaceMaterialSlot.ToString());
					return false;
				}
				Mesh = Candidate;
			}
		}
	}
	const int32 Slot = Mesh ? Mesh->GetMaterialIndex(FaceMaterialSlot) : INDEX_NONE;
	if (Slot == INDEX_NONE)
	{
		UE_LOG(LogSHPlayerFace, Warning, TEXT("%s: no mesh with material slot %s."), *GetName(), *FaceMaterialSlot.ToString());
		return false;
	}
	if (BoundFaceMesh == Mesh && BoundFaceSlotIndex == Slot && IsValid(PlayerFaceMaterial) &&
		Mesh->GetMaterial(Slot) == PlayerFaceMaterial)
	{
		return true;
	}
	RestoreFaceMaterial();
	UMaterialInterface* Base = FaceBaseMaterial ? FaceBaseMaterial.Get() : Mesh->GetMaterial(Slot);
	UTexture* ExistingTexture = nullptr;
	if (!IsValid(Base) || !Base->GetTextureParameterValue(FMaterialParameterInfo(FaceTextureParameter), ExistingTexture))
	{
		UE_LOG(LogSHPlayerFace, Warning, TEXT("%s: face material must contain texture parameter %s."),
			*GetName(), *FaceTextureParameter.ToString());
		return false;
	}
	OriginalFaceMaterial = Mesh->GetMaterial(Slot);
	PlayerFaceMaterial = UMaterialInstanceDynamic::Create(Base, this);
	BoundFaceMesh = Mesh;
	BoundFaceSlotIndex = Slot;
	Mesh->SetMaterial(Slot, PlayerFaceMaterial);
	return true;
}

void ASHPlayerRepresentation::RefreshPlayerFace()
{
	if (bPresentationEnded || bRenderingFace || !PlayerFaceWidgetClass || !GetWorld() ||
		GetNetMode() == NM_DedicatedServer || !FSlateApplication::IsInitialized())
	{
		return;
	}
	TGuardValue<bool> RenderingGuard(bRenderingFace, true);
	if (!IsValid(PlayerFaceWidget))
	{
		PlayerFaceWidget = CreateWidget<USHPlayerFaceWidget>(GetWorld(), PlayerFaceWidgetClass);
	}
	if (!IsValid(PlayerFaceWidget))
	{
		return;
	}
	// Build first: Blueprint Construct must finish before applying the represented player's data.
	const TSharedRef<SWidget> SlateWidget = PlayerFaceWidget->TakeWidget();
	PlayerFaceWidget->UpdatePresentation(this);
	if (bPresentationEnded || !FApp::CanEverRender() || !BindFaceMaterial())
	{
		return;
	}
	const FIntPoint Size(FMath::Clamp(FaceRenderSize.X, 64, 2048), FMath::Clamp(FaceRenderSize.Y, 64, 2048));
	if (!FaceRenderer)
	{
		// A mesh material needs linear UI color; the scene applies display gamma later.
		FaceRenderer = new FWidgetRenderer(false, true);
	}
	if (!PlayerFaceRenderTarget)
	{
		PlayerFaceRenderTarget = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, false);
	}
	else if (PlayerFaceRenderTarget->SizeX != Size.X || PlayerFaceRenderTarget->SizeY != Size.Y)
	{
		PlayerFaceRenderTarget->ResizeTarget(Size.X, Size.Y);
	}
	if (PlayerFaceRenderTarget)
	{
		const FIntPoint DesignSize(FMath::Clamp(FaceDesignSize.X, 1, 4096), FMath::Clamp(FaceDesignSize.Y, 1, 4096));
		// Render the complete authored widget at a stable logical size, including
		// its background and decorations. Resolution is only a sampling choice.
		const TSharedRef<SWidget> Surface = SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(DesignSize.X)
				.HeightOverride(DesignSize.Y)
				[SlateWidget]
			];
		FaceRenderer->DrawWidget(PlayerFaceRenderTarget, Surface, FVector2D(Size), 0.f);
		PlayerFaceMaterial->SetTextureParameterValue(FaceTextureParameter, PlayerFaceRenderTarget);
	}
}

void ASHPlayerRepresentation::ReloadPlayerAvatar()
{
	const uint64 RequestId = ++AvatarRequestId;
	LoadedAvatar = nullptr;
	RefreshPlayerFace();
	if (bPresentationEnded || !PlayerFaceWidgetClass || !bLoadSteamAvatar || !IsValid(RepresentedPlayerState) ||
		!GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UGameInstance* Instance = GetGameInstance();
	USHSteamAvatarSubsystem* Avatars = Instance ? Instance->GetSubsystem<USHSteamAvatarSubsystem>() : nullptr;
	if (!Avatars)
	{
		return;
	}
	TWeakObjectPtr<ASHPlayerRepresentation> WeakThis(this);
	TWeakObjectPtr<ASHPlayerState> ExpectedPlayer(RepresentedPlayerState);
	Avatars->RequestAvatar(RepresentedPlayerState, [WeakThis, ExpectedPlayer, RequestId](UTexture2D* Texture, const FString&)
	{
		if (ASHPlayerRepresentation* Self = WeakThis.Get())
		{
			Self->CompleteAvatarRequest(RequestId, ExpectedPlayer.Get(), Texture);
		}
	});
}

void ASHPlayerRepresentation::CompleteAvatarRequest(uint64 RequestId, ASHPlayerState* ExpectedPlayer, UTexture2D* Texture)
{
	// Seat rotation, disconnects and retries can finish out of order.
	if (bPresentationEnded || RequestId != AvatarRequestId || !IsValid(ExpectedPlayer) ||
		RepresentedPlayerState != ExpectedPlayer || !IsValid(Texture))
	{
		return;
	}
	LoadedAvatar = Texture;
	RefreshPlayerFace();
}
