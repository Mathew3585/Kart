#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "KartPawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

// Kart arcade "fait maison" : pas de physique simulée, on calcule tout nous-mêmes dans Tick
UCLASS()
class KARTGAMES_API AKartPawn : public APawn
{
	GENERATED_BODY()

public:
	AKartPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	// Appelée quand un Controller prend possession du kart : on y ajoute le Mapping Context
	virtual void NotifyControllerChanged() override;

	// ---------- Composants ----------

	// Boîte de collision : c'est elle qui se déplace et qui bloque contre les murs
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	// Châssis du kart
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	// Les 4 roues (les roues avant braquent)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UStaticMeshComponent> WheelFrontLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UStaticMeshComponent> WheelFrontRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UStaticMeshComponent> WheelRearLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UStaticMeshComponent> WheelRearRight;

	// Bras de caméra (avec du retard pour un effet de poursuite)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kart|Components")
	TObjectPtr<UCameraComponent> Camera;

	// ---------- Input ----------

	UPROPERTY(EditAnywhere, Category = "Kart|Input")
	TObjectPtr<UInputMappingContext> KartMappingContext;

	// Axe 1D : +1 = accélérer, -1 = freiner / reculer
	UPROPERTY(EditAnywhere, Category = "Kart|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	// Axe 1D : +1 = droite, -1 = gauche
	UPROPERTY(EditAnywhere, Category = "Kart|Input")
	TObjectPtr<UInputAction> SteerAction;

	// Bouton : maintenir en tournant pour drifter
	UPROPERTY(EditAnywhere, Category = "Kart|Input")
	TObjectPtr<UInputAction> DriftAction;

	// Bouton : petit saut
	UPROPERTY(EditAnywhere, Category = "Kart|Input")
	TObjectPtr<UInputAction> JumpAction;

	// ---------- Réglages : déplacement (cm et secondes) ----------

	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float MaxForwardSpeed = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float MaxReverseSpeed = 800.f;

	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float Acceleration = 1500.f;

	// Décélération quand on appuie dans le sens opposé à la vitesse
	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float BrakeDeceleration = 3000.f;

	// Décélération quand on lâche les gaz
	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float CoastDeceleration = 800.f;

	// Vitesse de rotation en degrés par seconde
	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float TurnRate = 100.f;

	// En dessous de cette vitesse, le kart ne tourne pas
	UPROPERTY(EditAnywhere, Category = "Kart|Movement")
	float MinSpeedToTurn = 50.f;

	// Part de la vitesse conservée quand on touche un mur (0 = arrêt net)
	UPROPERTY(EditAnywhere, Category = "Kart|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float WallSpeedFactor = 0.3f;

	// ---------- Réglages : gravité et sol ----------

	UPROPERTY(EditAnywhere, Category = "Kart|Gravity")
	float GravityStrength = 2500.f;

	// Longueur du rayon vers le bas, depuis le centre de la boîte
	UPROPERTY(EditAnywhere, Category = "Kart|Gravity")
	float GroundTraceDistance = 80.f;

	// Vitesse à laquelle le kart s'incline pour suivre la pente
	UPROPERTY(EditAnywhere, Category = "Kart|Gravity")
	float GroundAlignSpeed = 8.f;

	// ---------- Réglages : saut ----------

	// Vitesse vers le haut au moment du saut (450 = environ 40 cm de haut)
	UPROPERTY(EditAnywhere, Category = "Kart|Jump")
	float JumpVelocity = 450.f;

	// ---------- Réglages : drift et boost ----------

	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float DriftMinSpeed = 1000.f;

	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float DriftTurnMultiplier = 1.4f;

	// Angle visuel du châssis pendant le drift (en degrés)
	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float DriftSlideAngle = 25.f;

	// Temps de drift pour obtenir le mini-turbo (bleu)
	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float MiniTurboChargeTime = 1.f;

	// Temps de drift pour obtenir le super-turbo (orange)
	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float SuperTurboChargeTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float MiniTurboDuration = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float SuperTurboDuration = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Kart|Drift")
	float BoostSpeed = 3500.f;

	// ---------- Réglages : visuel ----------

	UPROPERTY(EditAnywhere, Category = "Kart|Visual")
	float WheelRadius = 20.f;

	UPROPERTY(EditAnywhere, Category = "Kart|Visual")
	float MaxWheelSteerAngle = 30.f;

	// Champ de vision (FOV) à l'arrêt
	UPROPERTY(EditAnywhere, Category = "Kart|Camera")
	float BaseFieldOfView = 90.f;

	// FOV à vitesse max : plus large = sensation de vitesse
	UPROPERTY(EditAnywhere, Category = "Kart|Camera")
	float MaxSpeedFieldOfView = 100.f;

	// FOV pendant un boost
	UPROPERTY(EditAnywhere, Category = "Kart|Camera")
	float BoostFieldOfView = 110.f;

	// Part de l'angle de drift que la caméra suit (1 = toujours derrière l'arrière du kart, 0 = ne suit pas)
	UPROPERTY(EditAnywhere, Category = "Kart|Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float CameraDriftFollow = 1.f;

	// Vitesse à laquelle le FOV change
	UPROPERTY(EditAnywhere, Category = "Kart|Camera")
	float FieldOfViewInterpSpeed = 4.f;

	// Affiche la vitesse et l'état du drift à l'écran
	UPROPERTY(EditAnywhere, Category = "Kart|Visual")
	bool bShowDebug = true;

	// ---------- État (visible dans les Details pendant le Play pour déboguer) ----------

	// Vitesse vers l'avant (négative = marche arrière)
	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float CurrentSpeed = 0.f;

	// Vitesse verticale (négative = on tombe)
	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float VerticalSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float ThrottleInput = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float SteerInput = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	bool bIsGrounded = false;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	bool bIsDrifting = false;

	// -1 = drift à gauche, +1 = drift à droite
	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	int32 DriftDirection = 0;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float DriftChargeTime = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float BoostTimeRemaining = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	float WheelSpinAngle = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Kart|State")
	FVector GroundNormal = FVector::UpVector;

private:
	// ---------- Fonctions appelées par l'input ----------
	void OnThrottle(const FInputActionValue& Value);
	void OnThrottleReleased(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnSteerReleased(const FInputActionValue& Value);
	void OnDriftPressed(const FInputActionValue& Value);
	void OnDriftReleased(const FInputActionValue& Value);
	void OnJumpPressed(const FInputActionValue& Value);

	// ---------- Étapes du Tick (dans l'ordre) ----------
	void UpdateGround(float DeltaTime);
	void UpdateSpeed(float DeltaTime);
	void UpdateSteering(float DeltaTime);
	void UpdateDrift(float DeltaTime);
	void UpdateBoost(float DeltaTime);
	void MoveKart(float DeltaTime);
	void AlignToGround(float DeltaTime);
	void UpdateVisuals(float DeltaTime);
	void UpdateCameraFieldOfView(float DeltaTime);
	void ShowDebugInfo() const;

	// ---------- Saut ----------
	void Jump();

	// ---------- Drift et boost ----------
	void StartDrift();
	void EndDrift();
	void StartBoost(float Duration);
	float GetCurrentMaxSpeed() const;

	// ---------- Visuel des roues ----------
	void SetWheelRotation(UStaticMeshComponent* Wheel, const FRotator& BaseRotation, float SteerAngle) const;

	// Rotations de départ placées dans le Blueprint (on tourne "par-dessus")
	FRotator BodyBaseRotation;
	FRotator SpringArmBaseRotation;
	FRotator WheelFrontLeftBaseRotation;
	FRotator WheelFrontRightBaseRotation;
	FRotator WheelRearLeftBaseRotation;
	FRotator WheelRearRightBaseRotation;
};
