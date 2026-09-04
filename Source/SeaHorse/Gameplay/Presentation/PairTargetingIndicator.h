#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PairTargetingIndicator.generated.h"

class UMaterialInterface;
class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FPairTargetingIndicatorStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	TObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	TObjectPtr<UStaticMesh> ArrowHeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "2", ClampMax = "32"))
	int32 SegmentCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.0", Units = "cm"))
	float ArcHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.0", Units = "cm"))
	float StartHeightOffset = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetClearance = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.001"))
	FVector2D BodyScale = FVector2D(0.12f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	FVector ArrowHeadScale = FVector(0.25f);

	/** These are written to Color and Intensity parameters when the material exposes them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FLinearColor ValidColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FLinearColor InvalidColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0"))
	float PulseSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0"))
	float PulseAmount = 0.0f;
};

/** Local-only reusable world-space presentation for card-effect targeting. */
UCLASS(Blueprintable, NotPlaceable)
class SEAHORSE_API APairTargetingIndicator : public AActor
{
	GENERATED_BODY()

public:
	APairTargetingIndicator();
	virtual void Tick(float DeltaSeconds) override;

	void InitializeIndicator(const FPairTargetingIndicatorStyle& InStyle, FName InEffectPresentationId);
	void UpdateIndicator(const FVector& Start, const FVector& End, bool bInValidTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "Targeting Indicator")
	void OnIndicatorInitialized(FName EffectPresentationId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Targeting Indicator")
	void OnTargetValidityChanged(bool bValidTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "Targeting Indicator")
	void OnIndicatorUpdated(FVector Start, FVector End, bool bValidTarget);

private:
	void RebuildSegments();
	void ApplyVisualState();
	void ApplyMaterialParameters(float DisplayIntensity);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ArrowHead;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Segments;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> DynamicMaterials;

	FPairTargetingIndicatorStyle Style;
	FName CurrentEffectPresentationId;
	bool bCurrentTargetValid = false;
	bool bHasVisualState = false;
	float AnimationTime = 0.0f;
};
