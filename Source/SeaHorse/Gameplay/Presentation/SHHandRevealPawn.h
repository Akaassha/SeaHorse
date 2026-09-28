#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Presentation/HandRevealTypes.h"
#include "SHHandRevealPawn.generated.h"

class ASHPlayerController;
class UCameraComponent;
class USplineComponent;
struct FInputKeyEventArgs;

/** Private presentation stage. Its visual cards are local copies, never gameplay cards. */
UCLASS(Blueprintable)
class SEAHORSE_API ASHHandRevealPawn : public APawn
{
	GENERATED_BODY()
public:
	ASHHandRevealPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual const AActor* GetNetOwner() const override;

	void InitializePresentation(ASHPlayerController* PC, FGuid SessionId,
		const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder, bool bCanFinish);
	void ApplySnapshot(const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder);
	bool HandlePointerInput(const FInputKeyEventArgs& Params);
	void ClearPresentation();

	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	TArray<FSHRevealedHandCard> GetCards() const { return RevealedCards; }
	/** Local visual actors, useful for a SceneCapture2D Show Only list. */
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	TArray<ASHCard*> GetPresentationCards() const;
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	bool CanReorderCards() const { return bReorderingAllowed; }
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	bool CanFinishViewing() const { return bFinishingAllowed; }
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	FGuid GetRevealSessionId() const { return RevealSessionId; }
	UFUNCTION(BlueprintImplementableEvent, Category = "Hand Reveal")
	void OnPresentationChanged();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Reveal|Components")
	TObjectPtr<USceneComponent> ViewRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Reveal|Components")
	TObjectPtr<USceneComponent> CardsRoot;
	/** Describes the current fan; available for presentation and capture subclasses. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Reveal|Components")
	TObjectPtr<USplineComponent> CardLayoutSpline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Reveal|Components")
	TObjectPtr<UCameraComponent> RevealCamera;

	/** Distance in front of the unchanged table camera, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "20.0"))
	float CardsDistanceFromCamera = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ScreenWidthFraction = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ScreenHeightFraction = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "1.0"))
	float PreferredCardSpacing = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "1.0"))
	float MaximumFanWidth = 110.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "0.1"))
	float CardScale = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout")
	FRotator CardRotation = FRotator(0.f, 90.f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "0.0"))
	float FanCurvature = 0.002f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "0.0"))
	float FanAnglePerCard = 1.5f;
	/** Virtual layout width used to fit the cards; never changes the table camera's zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "1.0"))
	float MinimumViewWidth = 75.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Hover", meta = (ClampMin = "0.0"))
	float HoverForwardOffset = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Hover", meta = (ClampMin = "0.0"))
	float HoverLiftHeight = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Hover", meta = (ClampMin = "1.0"))
	float HoverScale = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Reveal|Layout", meta = (ClampMin = "0.1"))
	float AnimationSpeed = 14.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerController> PresentationController;
	// Deliberately not replicated: definitions arrive only through participant RPCs.
	UPROPERTY(Transient)
	TArray<FSHRevealedHandCard> RevealedCards;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHCard>> VisualCards;
	TMap<TObjectPtr<ASHCard>, FTransform> RestingTransforms;
	TMap<TObjectPtr<ASHCard>, FBox> VisualBounds;
	TWeakObjectPtr<ASHCard> HoveredVisual;
	TWeakObjectPtr<ASHCard> DraggedVisual;
	FGuid RevealSessionId;
	bool bReorderingAllowed = false;
	bool bFinishingAllowed = false;
	int32 LastRequestedDropIndex = INDEX_NONE;
	FVector DragCursorOffset = FVector::ZeroVector;
	UPROPERTY(Transient)
	FMinimalViewInfo PreservedView;
	FMatrix PreservedProjection = FMatrix::Identity;
	bool bHasPreservedView = false;

	void CaptureTableView(ASHPlayerController* PC);
	void FitCardsToView(const FBox& LayoutBounds);
	ASHCard* SpawnVisualCard(const FSHRevealedHandCard& Card);
	void RebuildLayout(bool bSnapNewCards);
	FTransform MakeHoveredTransform(const FTransform& Resting) const;
	bool GetCursorOnCardPlane(FVector& OutLocalPoint) const;
	ASHCard* FindVisualUnderCursor(const FVector& LocalPoint) const;
	bool ContainsPoint(const ASHCard* Card, const FTransform& Transform, const FVector& Point) const;
	void UpdateDrag(const FVector& Cursor);
};
