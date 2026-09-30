#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "CommonTextBlock.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Player/SHPlayerRepresentation.h"
#include "Gameplay/Presentation/SHPlayerFaceWidget.h"
#include "Gameplay/SHHand.h"
#include "ImageUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "TextureResource.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHPlayerFacePresentationTest, "SeaHorse.Gameplay.UI.PlayerFacePresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHPlayerFacePresentationTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Face = World->SpawnActor<ASHPlayerRepresentation>();
	Face->RefreshPlayerFace();
	TestNull(TEXT("Legacy actors do not create a face widget"), Face->GetPlayerFaceWidget());
	Face->PlayerFaceWidgetClass = USHPlayerFaceWidget::StaticClass();
	Face->bLoadSteamAvatar = false;
	Face->OfflineDisplayName = FText::FromString(TEXT("Offline player"));
	auto* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeaHorse/Board/Materials/M_PlayerFace.M_PlayerFace"));
	if (!TestNotNull(TEXT("Test surface material"), Base))
	{
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	if (FApp::CanEverRender())
	{
		GShaderCompilingManager->FinishAllCompilation();
		const FMaterialResource* Resource = Base->GetMaterialResource(GMaxRHIShaderPlatform);
		if (TestNotNull(TEXT("Material resource"), Resource))
		{
			TestEqual(TEXT("Face material compiles without shader errors"), Resource->GetCompileErrors().Num(), 0);
		}
	}
	auto* MeshAsset = NewObject<UStaticMesh>(Face);
	MeshAsset->GetStaticMaterials().Add(FStaticMaterial(Base, TEXT("Frame")));
	MeshAsset->GetStaticMaterials().Add(FStaticMaterial(Base, TEXT("PlayerFace")));
	auto* Mesh = NewObject<UStaticMeshComponent>(Face);
	Face->AddInstanceComponent(Mesh);
	Mesh->SetStaticMesh(MeshAsset);
	TestTrue(TEXT("Unique named slot binds automatically"), Face->BindFaceMaterial());
	TestEqual(TEXT("Frame material is untouched"), Mesh->GetMaterial(0), Base);
	TestEqual(TEXT("Only face slot receives a dynamic material"), Mesh->GetMaterial(1), static_cast<UMaterialInterface*>(Face->GetPlayerFaceMaterial()));

	auto MakeAvatar = [](FColor Color)
	{
		auto* Texture = UTexture2D::CreateTransient(32, 32, PF_B8G8R8A8);
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		auto* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Index = 0; Index < 32 * 32; ++Index) { Pixels[Index] = Color; }
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	};
	Face->FallbackAvatar = MakeAvatar(FColor::Red);
	Face->BindToHand(nullptr);
	auto* Widget = Face->GetPlayerFaceWidget();
	if (!TestNotNull(TEXT("Null PlayerState still initializes the offline face"), Widget))
	{
		Face->EndPlay(EEndPlayReason::Destroyed);
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	{
		TestEqual(TEXT("Offline name"), Widget->DisplayName.ToString(), FString(TEXT("Offline player")));
		TestEqual(TEXT("Fallback avatar"), Widget->AvatarTexture.Get(), Face->FallbackAvatar.Get());
		auto* Name = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PlayerNameText")));
		if (TestNotNull(TEXT("Native name control"), Name))
		{
			TestTrue(TEXT("Long names use ellipsis"), Name->GetTextOverflowPolicy() == ETextOverflowPolicy::Ellipsis);
			TestTrue(TEXT("Name cannot paint outside its field"), Name->GetClipping() == EWidgetClipping::ClipToBoundsAlways);
		}
	}
	auto* StateA = World->SpawnActor<ASHPlayerState>();
	auto* StateB = World->SpawnActor<ASHPlayerState>();
	auto* Hand = World->SpawnActor<ASHHand>();
	StateA->SetPlayerName(TEXT("A very long player name that must never expand the plaque beyond its frame"));
	Hand->SetRepresentedPlayerState(StateA);
	Face->BindToHand(Hand);
	TestEqual(TEXT("Represented player's name"), Widget->DisplayName.ToString(), StateA->GetPlayerName());
	StateA->SetPlayerName(TEXT("Changed\nname"));
	StateA->OnRep_PlayerName();
	TestEqual(TEXT("Replicated name refreshes presentation"), Widget->DisplayName.ToString(), StateA->GetPlayerName());
	TestEqual(TEXT("Line breaks do not create extra lines"), CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("PlayerNameText")))->GetText().ToString(), FString(TEXT("Changed name")));
	StateA->SetPlayerName(TEXT("A very long player name that must never expand the plaque beyond its frame"));
	StateA->OnRep_PlayerName();
	Face->SetSelectable(true);
	TestTrue(TEXT("Selection state reaches the widget"), Widget->bSelectable);
	const uint64 OldRequest = Face->AvatarRequestId;
	StateB->SetPlayerName(TEXT("Second player"));
	Hand->SetRepresentedPlayerState(StateB);
	Face->BindToHand(Hand);
	auto* GreenAvatar = MakeAvatar(FColor::Green);
	Face->CompleteAvatarRequest(OldRequest, StateA, GreenAvatar);
	TestEqual(TEXT("Old player's avatar cannot leak into a new seat"), Face->GetPlayerAvatar(), Face->FallbackAvatar.Get());
	Face->ReloadPlayerAvatar();
	Face->CompleteAvatarRequest(Face->AvatarRequestId - 1, StateB, GreenAvatar);
	TestEqual(TEXT("Superseded request for the same player is ignored"), Face->GetPlayerAvatar(), Face->FallbackAvatar.Get());

	UTextureRenderTarget2D* Target = Face->GetPlayerFaceRenderTarget();
	UMaterialInstanceDynamic* MID = Face->GetPlayerFaceMaterial();
	auto ReadAvatarPixel = [this, Face]()
	{
		FlushRenderingCommands();
		TArray<FColor> Pixels;
		TestTrue(TEXT("Render target can be read"), Face->GetPlayerFaceRenderTarget()->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
		return Pixels.IsValidIndex(184 * 512 + 256) ? Pixels[184 * 512 + 256] : FColor::Black;
	};
	if (FApp::CanEverRender() && TestNotNull(TEXT("Real RHI creates the face texture"), Target))
	{
		TestEqual(TEXT("Default render width"), Target->SizeX, 512);
		const FColor Pixel = ReadAvatarPixel();
		TestTrue(TEXT("Fallback avatar is actually rendered"), Pixel.R > 200 && Pixel.G < 20);
		UTexture* BoundTexture = nullptr;
		MID->GetTextureParameterValue(FMaterialParameterInfo(TEXT("PlayerFaceTexture")), BoundTexture);
		TestEqual(TEXT("Material samples the rendered widget"), BoundTexture, static_cast<UTexture*>(Target));
	}
	Face->CompleteAvatarRequest(Face->AvatarRequestId, StateB, GreenAvatar);
	TestEqual(TEXT("Current avatar updates the widget"), Widget->AvatarTexture.Get(), GreenAvatar);
	TestEqual(TEXT("Redrawing reuses the render target"), Face->GetPlayerFaceRenderTarget(), Target);
	TestEqual(TEXT("Redrawing reuses the material"), Face->GetPlayerFaceMaterial(), MID);
	if (FApp::CanEverRender() && Target)
	{
		const FColor Pixel = ReadAvatarPixel();
		TestTrue(TEXT("Downloaded avatar replaces fallback pixels"), Pixel.G > 200 && Pixel.R < 20);
		StateB->SetPlayerName(TEXT("A very long player name that must never expand the plaque beyond its frame"));
		StateB->OnRep_PlayerName();
		FlushRenderingCommands();
		FBufferArchive PNG;
		if (FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG))
		{
			FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / TEXT("PlayerFacePreview.png")));
		}
	}
	Hand->SetRepresentedPlayerState(nullptr);
	Face->BindToHand(Hand);
	TestEqual(TEXT("Disconnect restores fallback avatar"), Widget->AvatarTexture.Get(), Face->FallbackAvatar.Get());
	StateB->SetPlayerName(TEXT("Stale player"));
	StateB->OnRep_PlayerName();
	TestEqual(TEXT("Old name delegate is detached"), Widget->DisplayName.ToString(), FString(TEXT("Offline player")));
	Face->EndPlay(EEndPlayReason::Destroyed);
	Face->CompleteAvatarRequest(Face->AvatarRequestId, StateB, GreenAvatar);
	TestNull(TEXT("EndPlay releases the widget"), Face->GetPlayerFaceWidget());
	TestNull(TEXT("EndPlay releases the target"), Face->GetPlayerFaceRenderTarget());
	TestEqual(TEXT("EndPlay restores the original material"), Mesh->GetMaterial(1), Base);

	// Prove a Designer layout is used intact and optional BindWidget controls receive data.
	auto* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		USHPlayerFaceWidget::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("TestPlayerFace")),
		BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!BP->WidgetTree) { BP->WidgetTree = NewObject<UWidgetTree>(BP); }
	auto* CustomRoot = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CustomLayout"));
	BP->WidgetTree->RootWidget = CustomRoot;
	auto* Background = BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AuthoredBackground"));
	Background->SetColorAndOpacity(FLinearColor(0.18f, 0.09f, 0.04f, 1.f));
	CustomRoot->AddChildToCanvas(Background)->SetSize(FVector2D(512.f, 512.f));
	auto* CustomName = BP->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PlayerNameText"));
	CustomName->SetJustification(ETextJustify::Center);
	CustomName->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
	CustomName->SetClipping(EWidgetClipping::OnDemand);
	CustomRoot->AddChildToCanvas(CustomName)->SetSize(FVector2D(300.f, 60.f));
	auto* CustomAvatar = BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AvatarImage"));
	CustomRoot->AddChildToCanvas(CustomAvatar)->SetSize(FVector2D(128.f, 128.f));
	FKismetEditorUtilities::CompileBlueprint(BP);
	TestTrue(TEXT("Designer subclass compiles"), BP->Status == BS_UpToDate);
	auto* CustomWidget = CreateWidget<USHPlayerFaceWidget>(World, BP->GeneratedClass.Get());
	if (TestNotNull(TEXT("Designer widget is instantiable"), CustomWidget))
	{
		CustomWidget->TakeWidget();
		auto* StyledName = CastChecked<UCommonTextBlock>(CustomWidget->GetWidgetFromName(TEXT("PlayerNameText")));
		StyledName->SetStyle(UCommonTextStyle::StaticClass());
		FSlateFontInfo AuthoredFont = CustomName->GetFont();
		AuthoredFont.Size = 31.f;
		AuthoredFont.FontMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeaHorse/Materials/M_Horse_Gradient.M_Horse_Gradient"));
		StyledName->SetFont(AuthoredFont);
		const FLinearColor AuthoredColor(0.7f, 0.3f, 0.1f, 1.f);
		StyledName->SetColorAndOpacity(FSlateColor(AuthoredColor));
		CustomWidget->UpdatePresentation(Face);
		TestNotNull(TEXT("Designer root was preserved"), CustomWidget->GetWidgetFromName(TEXT("CustomLayout")));
		TestNull(TEXT("Default layout was not inserted"), CustomWidget->GetWidgetFromName(TEXT("FaceCanvas")));
		TestEqual(TEXT("Optional name binding is automatic"), CastChecked<UTextBlock>(CustomWidget->GetWidgetFromName(TEXT("PlayerNameText")))->GetText().ToString(), FString(TEXT("Offline player")));
		TestEqual(TEXT("Optional avatar binding is automatic"), CastChecked<UImage>(CustomWidget->GetWidgetFromName(TEXT("AvatarImage")))->GetBrush().GetResourceObject(), static_cast<UObject*>(Face->FallbackAvatar));
		TestEqual(TEXT("Designer alignment is preserved"),
			FindFProperty<FByteProperty>(UTextBlock::StaticClass(), TEXT("Justification"))->GetPropertyValue_InContainer(StyledName),
			static_cast<uint8>(ETextJustify::Center));
		TestTrue(TEXT("Designer overflow is preserved"), StyledName->GetTextOverflowPolicy() == ETextOverflowPolicy::Clip);
		TestTrue(TEXT("Designer clipping is preserved"), StyledName->GetClipping() == EWidgetClipping::OnDemand);
		TestEqual(TEXT("Refresh does not reapply the CommonUI font style"), StyledName->GetFont().Size, AuthoredFont.Size);
		TestEqual(TEXT("Refresh preserves the font material"), StyledName->GetFont().FontMaterial, AuthoredFont.FontMaterial);
		TestTrue(TEXT("Refresh preserves the authored text color"), StyledName->GetColorAndOpacity().GetSpecifiedColor().Equals(AuthoredColor));

		auto* CustomFace = World->SpawnActor<ASHPlayerRepresentation>();
		CustomFace->PlayerFaceWidgetClass = BP->GeneratedClass.Get();
		CustomFace->PlayerFaceWidget = CustomWidget;
		CustomFace->FaceDesignSize = FIntPoint(512, 512);
		CustomFace->FaceRenderSize = FIntPoint(1024, 1024);
		auto* SurfaceMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/SeaHorse/Board/Materials/M_PlayerWidgetSurface.M_PlayerWidgetSurface"));
		CustomFace->FaceBaseMaterial = SurfaceMaterial;
		TestNotNull(TEXT("Whole-widget surface material exists"), SurfaceMaterial);
		auto* CustomMesh = NewObject<UStaticMeshComponent>(CustomFace);
		CustomFace->AddInstanceComponent(CustomMesh);
		CustomMesh->SetStaticMesh(MeshAsset);
		CustomFace->RefreshPlayerFace();
		if (FApp::CanEverRender())
		{
			GShaderCompilingManager->FinishAllCompilation();
			const FMaterialResource* SurfaceResource = SurfaceMaterial ? SurfaceMaterial->GetMaterialResource(GMaxRHIShaderPlatform) : nullptr;
			if (TestNotNull(TEXT("Whole-widget surface resource"), SurfaceResource))
			{
				TestEqual(TEXT("Whole-widget surface compiles"), SurfaceResource->GetCompileErrors().Num(), 0);
			}
			if (TestNotNull(TEXT("Designer widget is rendered in full"), CustomFace->GetPlayerFaceRenderTarget()))
			{
				FlushRenderingCommands();
				TArray<FColor> Pixels;
				FReadSurfaceDataFlags Flags(RCM_MinMax);
				Flags.SetLinearToGamma(false);
				TestTrue(TEXT("Read raw linear widget pixels"), CustomFace->GetPlayerFaceRenderTarget()->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags));
				if (Pixels.IsValidIndex(900 * 1024 + 900))
				{
					// ReadLinearColorPixels treats BGRA8 bytes as sRGB. This target stores linear bytes.
					const FLinearColor Pixel = Pixels[900 * 1024 + 900].ReinterpretAsLinear();
					TestTrue(FString::Printf(TEXT("Authored background fills higher-resolution target without reflow or double gamma (actual %s)"), *Pixel.ToString()),
						Pixel.Equals(FLinearColor(0.18f, 0.09f, 0.04f, 1.f), 0.015f));
				}
			}
		}
		CustomFace->EndPlay(EEndPlayReason::Destroyed);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
