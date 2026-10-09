#include "KartPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

// Marge (en cm) sous la boîte pour considérer que le kart touche le sol
static constexpr float GroundSnapTolerance = 5.f;

// Petit espace (en cm) entre la boîte et le sol pour qu'elle ne frotte pas dessus
static constexpr float GroundClearance = 2.f;

// Une surface dont la normale a un Z plus grand que ça est un sol, sinon c'est un mur
static constexpr float WalkableFloorZ = 0.7f;

AKartPawn::AKartPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// La boîte de collision est la racine : c'est elle qu'on déplace
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(70.f, 45.f, 25.f));
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetSimulatePhysics(false);
	RootComponent = CollisionBox;

	// Les meshes sont juste visuels : pas de collision, c'est la boîte qui s'en occupe
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Les roues sont attachées au châssis pour suivre son décalage pendant le drift
	WheelFrontLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFrontLeft"));
	WheelFrontLeft->SetupAttachment(BodyMesh);
	WheelFrontLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WheelFrontRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFrontRight"));
	WheelFrontRight->SetupAttachment(BodyMesh);
	WheelFrontRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WheelRearLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRearLeft"));
	WheelRearLeft->SetupAttachment(BodyMesh);
	WheelRearLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WheelRearRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRearRight"));
	WheelRearRight->SetupAttachment(BodyMesh);
	WheelRearRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Caméra derrière et au-dessus du kart
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 600.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 100.f);

	// Pas de retard en position : à grande vitesse il fait trembler le kart d'avant en arrière à l'écran
	SpringArm->bEnableCameraLag = false;

	// Retard en rotation seulement : la caméra suit les virages en douceur
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 6.f;
	SpringArm->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void AKartPawn::BeginPlay()
{
	Super::BeginPlay();

	// On retient les rotations placées dans le Blueprint pour ne pas les écraser
	BodyBaseRotation = BodyMesh->GetRelativeRotation();
	SpringArmBaseRotation = SpringArm->GetRelativeRotation();
	WheelFrontLeftBaseRotation = WheelFrontLeft->GetRelativeRotation();
	WheelFrontRightBaseRotation = WheelFrontRight->GetRelativeRotation();
	WheelRearLeftBaseRotation = WheelRearLeft->GetRelativeRotation();
	WheelRearRightBaseRotation = WheelRearRight->GetRelativeRotation();
}

void AKartPawn::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// On donne au joueur les touches du kart
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(KartMappingContext, 0);
		}
	}
}

void AKartPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInput == nullptr)
	{
		return;
	}

	// Triggered = tant qu'on appuie, Completed = quand on lâche
	EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AKartPawn::OnThrottle);
	EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AKartPawn::OnThrottleReleased);

	EnhancedInput->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AKartPawn::OnSteer);
	EnhancedInput->BindAction(SteerAction, ETriggerEvent::Completed, this, &AKartPawn::OnSteerReleased);

	// Started = une seule fois au moment où on appuie
	EnhancedInput->BindAction(DriftAction, ETriggerEvent::Started, this, &AKartPawn::OnDriftPressed);
	EnhancedInput->BindAction(DriftAction, ETriggerEvent::Completed, this, &AKartPawn::OnDriftReleased);

	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AKartPawn::OnJumpPressed);
}

void AKartPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Chaque étape est une petite fonction, appelée dans l'ordre
	UpdateGround(DeltaTime);
	UpdateBoost(DeltaTime);
	UpdateSpeed(DeltaTime);
	UpdateSteering(DeltaTime);
	UpdateDrift(DeltaTime);
	MoveKart(DeltaTime);
	AlignToGround(DeltaTime);
	UpdateVisuals(DeltaTime);
	UpdateCameraFieldOfView(DeltaTime);

	if (bShowDebug)
	{
		ShowDebugInfo();
	}
}

// ==================== Input ====================

void AKartPawn::OnThrottle(const FInputActionValue& Value)
{
	ThrottleInput = Value.Get<float>();
}

void AKartPawn::OnThrottleReleased(const FInputActionValue& Value)
{
	ThrottleInput = 0.f;
}

void AKartPawn::OnSteer(const FInputActionValue& Value)
{
	SteerInput = Value.Get<float>();
}

void AKartPawn::OnSteerReleased(const FInputActionValue& Value)
{
	SteerInput = 0.f;
}

void AKartPawn::OnDriftPressed(const FInputActionValue& Value)
{
	// Le drift ne démarre que si on est au sol (StartDrift le vérifie)
	StartDrift();
}

void AKartPawn::OnDriftReleased(const FInputActionValue& Value)
{
	EndDrift();
}

void AKartPawn::OnJumpPressed(const FInputActionValue& Value)
{
	Jump();
}

// ==================== Étapes du Tick ====================

void AKartPawn::UpdateGround(float DeltaTime)
{
	// On lance un rayon vers le bas du kart pour trouver le sol
	const FVector Start = GetActorLocation();
	const FVector End = Start - GetActorUpVector() * GroundTraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// Le rayon part du centre de la boîte : le sol est "touché" à une demi-hauteur de boîte
	const float BoxHalfHeight = CollisionBox->GetScaledBoxExtent().Z;
	const float RideHeight = BoxHalfHeight + GroundClearance;
	bIsGrounded = bHitSomething && Hit.Distance <= RideHeight + GroundSnapTolerance && VerticalSpeed <= 0.f;

	// Si on voit un sol (même un peu loin), on s'aligne dessus, sinon on se remet droit
	GroundNormal = bHitSomething ? Hit.ImpactNormal : FVector::UpVector;

	if (bIsGrounded)
	{
		// Au sol on ne tombe plus
		VerticalSpeed = 0.f;

		// On recolle le kart à la bonne hauteur au-dessus du sol (vers le haut ou vers le bas)
		const float HeightError = Hit.Distance - RideHeight;
		AddActorWorldOffset(-GetActorUpVector() * HeightError, true);
	}
	else
	{
		// En l'air la gravité nous fait accélérer vers le bas
		VerticalSpeed -= GravityStrength * DeltaTime;
	}
}

void AKartPawn::UpdateSpeed(float DeltaTime)
{
	// Pendant un boost, on fonce à la vitesse du boost, même sans accélérer
	if (BoostTimeRemaining > 0.f)
	{
		CurrentSpeed = BoostSpeed;
		return;
	}

	// En l'air on garde sa vitesse
	if (!bIsGrounded)
	{
		return;
	}

	float TargetSpeed = 0.f;
	float Rate = CoastDeceleration;

	if (ThrottleInput > 0.f)
	{
		// On accélère vers l'avant (on freine d'abord si on reculait)
		TargetSpeed = GetCurrentMaxSpeed() * ThrottleInput;
		Rate = CurrentSpeed < 0.f ? BrakeDeceleration : Acceleration;
	}
	else if (ThrottleInput < 0.f)
	{
		// On freine, puis on recule
		TargetSpeed = MaxReverseSpeed * ThrottleInput;
		Rate = CurrentSpeed > 0.f ? BrakeDeceleration : Acceleration;
	}

	// On rapproche la vitesse de la cible, au maximum de "Rate" par seconde
	CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Rate);
}

void AKartPawn::UpdateSteering(float DeltaTime)
{
	// Pas de virage en l'air ou à l'arrêt
	if (!bIsGrounded || FMath::Abs(CurrentSpeed) < MinSpeedToTurn)
	{
		return;
	}

	float Steer = SteerInput;
	float Rate = TurnRate;

	if (bIsDrifting)
	{
		// En drift on tourne toujours du côté du drift :
		// le joystick dans le même sens serre le virage, dans l'autre sens il l'élargit
		const float SteerAlongDrift = SteerInput * DriftDirection;
		Steer = DriftDirection * FMath::GetMappedRangeValueClamped(FVector2D(-1.f, 1.f), FVector2D(0.5f, 1.f), SteerAlongDrift);
		Rate *= DriftTurnMultiplier;
	}

	// En marche arrière la direction est inversée, comme une vraie voiture
	const float MoveDirection = CurrentSpeed >= 0.f ? 1.f : -1.f;

	const float YawDelta = Steer * Rate * MoveDirection * DeltaTime;
	AddActorLocalRotation(FRotator(0.f, YawDelta, 0.f));
}

void AKartPawn::UpdateDrift(float DeltaTime)
{
	if (!bIsDrifting)
	{
		return;
	}

	// Trop lent : le drift s'arrête sans boost
	if (CurrentSpeed < DriftMinSpeed * 0.5f)
	{
		bIsDrifting = false;
		DriftDirection = 0;
		DriftChargeTime = 0.f;
		return;
	}

	// Le drift se charge seulement au sol
	if (bIsGrounded)
	{
		DriftChargeTime += DeltaTime;
	}
}

void AKartPawn::UpdateBoost(float DeltaTime)
{
	BoostTimeRemaining = FMath::Max(0.f, BoostTimeRemaining - DeltaTime);
}

void AKartPawn::MoveKart(float DeltaTime)
{
	// Déplacement = vers l'avant du kart + vers le haut/bas (gravité)
	const FVector Delta = GetActorForwardVector() * CurrentSpeed * DeltaTime
		+ FVector::UpVector * VerticalSpeed * DeltaTime;

	// "Sweep" = la boîte s'arrête si elle touche quelque chose au lieu de traverser
	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);

	if (!Hit.bBlockingHit)
	{
		return;
	}

	// Si la boîte était déjà collée à quelque chose au départ, ce n'est pas un vrai choc
	const bool bHitWall = Hit.Normal.Z < WalkableFloorZ && !Hit.bStartPenetrating;
	if (bHitWall)
	{
		// 0 = on frôle le mur, 1 = on le prend de face
		const FVector MoveDirection = GetActorForwardVector() * FMath::Sign(CurrentSpeed);
		const float ImpactStrength = FMath::Clamp(-FVector::DotProduct(MoveDirection, Hit.Normal), 0.f, 1.f);

		// On perd plus de vitesse quand on tape le mur de face
		CurrentSpeed *= FMath::Lerp(1.f, WallSpeedFactor, ImpactStrength);
	}
	else if (VerticalSpeed < 0.f)
	{
		// On vient d'atterrir
		VerticalSpeed = 0.f;
	}

	// Le reste du mouvement glisse le long de la surface touchée
	const FVector RemainingDelta = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
	AddActorWorldOffset(RemainingDelta, true);
}

void AKartPawn::AlignToGround(float DeltaTime)
{
	// Rotation cible : le haut du kart = la normale du sol, en gardant la même direction avant
	const FQuat TargetRotation = FRotationMatrix::MakeFromZX(GroundNormal, GetActorForwardVector()).ToQuat();

	// On tourne petit à petit vers la cible (Slerp = interpolation de rotation)
	const float Alpha = FMath::Clamp(GroundAlignSpeed * DeltaTime, 0.f, 1.f);
	const FQuat NewRotation = FQuat::Slerp(GetActorQuat(), TargetRotation, Alpha);

	SetActorRotation(NewRotation);
}

void AKartPawn::UpdateVisuals(float DeltaTime)
{
	// Angle parcouru par la roue = distance / rayon (en radians)
	WheelSpinAngle += FMath::RadiansToDegrees(CurrentSpeed * DeltaTime / WheelRadius);
	WheelSpinAngle = FMath::Fmod(WheelSpinAngle, 360.f);

	// Seules les roues avant braquent
	const float WheelSteerAngle = SteerInput * MaxWheelSteerAngle;
	SetWheelRotation(WheelFrontLeft, WheelFrontLeftBaseRotation, WheelSteerAngle);
	SetWheelRotation(WheelFrontRight, WheelFrontRightBaseRotation, WheelSteerAngle);
	SetWheelRotation(WheelRearLeft, WheelRearLeftBaseRotation, 0.f);
	SetWheelRotation(WheelRearRight, WheelRearRightBaseRotation, 0.f);

	// En drift, le châssis se met de travers
	const float TargetSlideYaw = bIsDrifting ? DriftDirection * DriftSlideAngle : 0.f;
	const float CurrentSlideYaw = BodyMesh->GetRelativeRotation().Yaw - BodyBaseRotation.Yaw;
	const float NewSlideYaw = FMath::FInterpTo(CurrentSlideYaw, TargetSlideYaw, DeltaTime, 6.f);

	FRotator BodyRotation = BodyBaseRotation;
	BodyRotation.Yaw += NewSlideYaw;
	BodyMesh->SetRelativeRotation(BodyRotation);

	// La caméra tourne avec le châssis pour rester derrière l'arrière du kart pendant le drift
	FRotator ArmRotation = SpringArmBaseRotation;
	ArmRotation.Yaw += NewSlideYaw * CameraDriftFollow;
	SpringArm->SetRelativeRotation(ArmRotation);
}

void AKartPawn::UpdateCameraFieldOfView(float DeltaTime)
{
	float TargetFieldOfView = BaseFieldOfView;

	if (BoostTimeRemaining > 0.f)
	{
		TargetFieldOfView = BoostFieldOfView;
	}
	else
	{
		// 0 à l'arrêt, 1 à vitesse max (seulement en marche avant)
		const float SpeedRatio = FMath::Clamp(CurrentSpeed / MaxForwardSpeed, 0.f, 1.f);
		TargetFieldOfView = FMath::Lerp(BaseFieldOfView, MaxSpeedFieldOfView, SpeedRatio);
	}

	// On change le FOV en douceur pour éviter les à-coups
	const float NewFieldOfView = FMath::FInterpTo(Camera->FieldOfView, TargetFieldOfView, DeltaTime, FieldOfViewInterpSpeed);
	Camera->SetFieldOfView(NewFieldOfView);
}

void AKartPawn::SetWheelRotation(UStaticMeshComponent* Wheel, const FRotator& BaseRotation, float SteerAngle) const
{
	// La roue tourne sur elle-même autour de X (Roll) puis braque autour de Z (Yaw), par-dessus sa rotation de départ
	// (X et pas Y car le modèle est tourné de -90° : son axe de roue est sur X)
	// Si les roues tournent à l'envers, mettre -WheelSpinAngle
	const FQuat SpinAndSteer = FQuat(FRotator(0.f, SteerAngle, WheelSpinAngle));
	Wheel->SetRelativeRotation(SpinAndSteer * FQuat(BaseRotation));
}

void AKartPawn::ShowDebugInfo() const
{
	if (GEngine == nullptr)
	{
		return;
	}

	// La couleur indique le niveau de charge du drift (comme dans Mario Kart)
	FColor DriftColor = FColor::White;
	if (DriftChargeTime >= SuperTurboChargeTime)
	{
		DriftColor = FColor::Orange;
	}
	else if (DriftChargeTime >= MiniTurboChargeTime)
	{
		DriftColor = FColor::Cyan;
	}

	// Une clé différente par ligne pour que les messages se remplacent au lieu de s'empiler
	GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, FString::Printf(TEXT("Speed: %.0f  Grounded: %s"), CurrentSpeed, bIsGrounded ? TEXT("yes") : TEXT("no")));
	GEngine->AddOnScreenDebugMessage(2, 0.f, DriftColor, FString::Printf(TEXT("Drift: %s  Charge: %.2f"), bIsDrifting ? TEXT("yes") : TEXT("no"), DriftChargeTime));
	GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::Yellow, FString::Printf(TEXT("Boost: %.2f"), BoostTimeRemaining));

	// Distance caméra-kart : si elle change pendant qu'on roule, c'est la caméra qui bouge
	const float CameraDistance = FVector::Dist(Camera->GetComponentLocation(), GetActorLocation());
	GEngine->AddOnScreenDebugMessage(4, 0.f, FColor::Green, FString::Printf(TEXT("Camera distance: %.1f  Camera lag: %s"), CameraDistance, SpringArm->bEnableCameraLag ? TEXT("on") : TEXT("off")));
}

// ==================== Saut ====================

void AKartPawn::Jump()
{
	// On ne peut sauter que depuis le sol
	if (!bIsGrounded)
	{
		return;
	}

	// On donne une vitesse vers le haut, la gravité fera redescendre le kart
	VerticalSpeed = JumpVelocity;
	bIsGrounded = false;
}

// ==================== Drift et boost ====================

void AKartPawn::StartDrift()
{
	// Il faut être au sol, assez rapide, et tourner d'un côté
	if (!bIsGrounded || CurrentSpeed < DriftMinSpeed || FMath::IsNearlyZero(SteerInput))
	{
		return;
	}

	bIsDrifting = true;
	DriftDirection = SteerInput > 0.f ? 1 : -1;
	DriftChargeTime = 0.f;
}

void AKartPawn::EndDrift()
{
	if (!bIsDrifting)
	{
		return;
	}

	// Plus on a drifté longtemps, plus le boost est long
	if (DriftChargeTime >= SuperTurboChargeTime)
	{
		StartBoost(SuperTurboDuration);
	}
	else if (DriftChargeTime >= MiniTurboChargeTime)
	{
		StartBoost(MiniTurboDuration);
	}

	bIsDrifting = false;
	DriftDirection = 0;
	DriftChargeTime = 0.f;
}

void AKartPawn::StartBoost(float Duration)
{
	BoostTimeRemaining = Duration;
	CurrentSpeed = BoostSpeed;
}

float AKartPawn::GetCurrentMaxSpeed() const
{
	return BoostTimeRemaining > 0.f ? BoostSpeed : MaxForwardSpeed;
}
