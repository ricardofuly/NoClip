// Copyright Fuly Games. All Rights Reserved.

#include "Enemy/NoclipEnemy.h"
#include "Enemy/NoclipEnemyAnimInstance.h"
#include "Enemy/NoclipEnemyDirector.h"
#include "Enemy/NoclipNoiseSubsystem.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipToolComponent.h"
#include "Character/NoclipToolTypes.h"
#include "NOCLIP.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipEnemy)

extern TAutoConsoleVariable<int32> CVarNoclipDebugEnemies;

namespace
{
	const TCHAR* StateName(ENoclipEnemyState S)
	{
		switch (S)
		{
		case ENoclipEnemyState::Idle:        return TEXT("parado");
		case ENoclipEnemyState::Wander:      return TEXT("vagando");
		case ENoclipEnemyState::Investigate: return TEXT("investigando");
		case ENoclipEnemyState::Alert:       return TEXT("alerta");
		case ENoclipEnemyState::Chase:       return TEXT("CACANDO");
		case ENoclipEnemyState::Attack:      return TEXT("ATACANDO");
		case ENoclipEnemyState::Stunned:     return TEXT("atordoado");
		case ENoclipEnemyState::Retreat:     return TEXT("recuando");
		case ENoclipEnemyState::Search:      return TEXT("procurando");
		default:                             return TEXT("?");
		}
	}

	bool IsFlashlightLit(const ANoclipCharacter* P)
	{
		const UNoclipToolComponent* T = P ? P->GetToolComponent() : nullptr;
		return T && T->GetEquipped() == ENoclipTool::Flashlight && T->IsFlashlightOn() && T->GetBattery() > 0.f;
	}
}

ANoclipEnemy::ANoclipEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.f);

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->bUseControllerDesiredRotation = false;
		Move->RotationRate = FRotator(0.f, 360.f, 0.f);
		Move->MaxWalkSpeed = 150.f;
		Move->MaxAcceleration = 1600.f;
		Move->BrakingDecelerationWalking = 1800.f;
		Move->GroundFriction = 8.f;
		Move->bUseRVOAvoidance = false;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(40.f, 50.f);
	}
	if (USkeletalMeshComponent* M = GetMesh())
	{
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetGenerateOverlapEvents(false);
		M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
}

void ANoclipEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipEnemy, Kind);
	DOREPLIFETIME(ANoclipEnemy, Variant);
	DOREPLIFETIME(ANoclipEnemy, State);
	DOREPLIFETIME(ANoclipEnemy, ActionRep);
}

void ANoclipEnemy::InitEnemy(ENoclipEnemyKind InKind, int32 InVariant, ANoclipEnemyDirector* InDirector, int32 Seed)
{
	Kind = InKind;
	Variant = static_cast<uint8>(FMath::Clamp(InVariant, 0, FMath::Max(NoclipEnemies::NumVariants(InKind) - 1, 0)));
	Director = InDirector;
	Stream.Initialize(Seed);
	ApplyVisual();
}

void ANoclipEnemy::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisual();
	if (HasAuthority())
	{
		SetState(ENoclipEnemyState::Idle, Stream.FRandRange(0.5f, 2.f));
		NextAlertTime = 0.0;
	}
}

void ANoclipEnemy::OnRep_Visual()
{
	ApplyVisual();
}

void ANoclipEnemy::ApplyVisual()
{
	const FNoclipEnemyDef& D = Def();
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(D.CapsuleRadius, D.CapsuleHalfHeight);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->RotationRate = FRotator(0.f, D.TurnRate, 0.f);
		Move->MaxAcceleration = D.Acceleration;
	}
	USkeletalMeshComponent* M = GetMesh();
	if (!M)
	{
		return;
	}
	if (USkeletalMesh* EnemyMesh = NoclipEnemies::LoadMesh(Kind, Variant))
	{
		if (M->GetSkeletalMeshAsset() != EnemyMesh)
		{
			M->SetSkeletalMesh(EnemyMesh);
		}
	}
	M->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -D.CapsuleHalfHeight), FRotator(0.f, D.MeshYaw, 0.f));
	M->SetRelativeScale3D(FVector(D.MeshScale));
	if (M->GetAnimClass() != UNoclipEnemyAnimInstance::StaticClass())
	{
		M->SetAnimInstanceClass(UNoclipEnemyAnimInstance::StaticClass());
	}
	if (UNoclipEnemyAnimInstance* Anim = Cast<UNoclipEnemyAnimInstance>(M->GetAnimInstance()))
	{
		Anim->SetEnemy(Kind, Variant);
	}
}

double ANoclipEnemy::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float ANoclipEnemy::ServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : static_cast<float>(Now());
}

float ANoclipEnemy::GetActionElapsed() const
{
	if (ActionRep.Action == ENoclipEnemyAction::None)
	{
		return 0.f;
	}
	return FMath::Max(0.f, (ServerTime() - ActionRep.StartServerTime) * ActionRep.Rate);
}

float ANoclipEnemy::AnimLength(ENoclipEnemyAnim Anim) const
{
	const UAnimSequence* S = NoclipEnemies::LoadAnim(Kind, Variant, Anim);
	return S ? FMath::Max(S->GetPlayLength(), 0.1f) : 1.f;
}

float ANoclipEnemy::FacingDot(const AActor* Other) const
{
	if (!Other)
	{
		return -1.f;
	}
	const FVector To = (Other->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	return FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), To);
}

// ===================================================================== Tick

void ANoclipEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	TickAction(DeltaSeconds);

	ThinkAccumulator += DeltaSeconds;
	if (ThinkAccumulator >= 0.1)
	{
		Think(static_cast<float>(ThinkAccumulator));
		ThinkAccumulator = 0.0;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = SpeedForState();
	}

	const bool bBusy = ActionRep.Action != ENoclipEnemyAction::None
		|| State == ENoclipEnemyState::Attack || State == ENoclipEnemyState::Alert || State == ENoclipEnemyState::Stunned
		|| State == ENoclipEnemyState::Idle || State == ENoclipEnemyState::Search;
	if (!bBusy && bHasGoal)
	{
		FollowPath(DeltaSeconds);
	}

	// Encara a presa enquanto uiva / morde (o CharacterMovement so gira andando).
	if (State == ENoclipEnemyState::Attack || State == ENoclipEnemyState::Alert)
	{
		if (const ANoclipCharacter* T = Target.Get())
		{
			FaceTowards(T->GetActorLocation(), DeltaSeconds);
		}
		else if (State == ENoclipEnemyState::Alert)
		{
			FaceTowards(LastKnownLocation, DeltaSeconds);
		}
	}

	if (CVarNoclipDebugEnemies.GetValueOnGameThread() > 0)
	{
		DebugDraw();
	}
}

void ANoclipEnemy::TickAction(float DeltaSeconds)
{
	const double T = Now();
	if (PendingHitTime > 0.0 && T >= PendingHitTime)
	{
		PendingHitTime = -1.0;
		ResolveAttackHit();
	}
	if (ActionEndTime > 0.0 && T >= ActionEndTime)
	{
		FinishAction();
	}
}

void ANoclipEnemy::SetState(ENoclipEnemyState NewState, float Duration)
{
	State = NewState;
	StateEndTime = Duration > 0.f ? Now() + Duration : 0.0;
	StuckTime = 0.0;
	StuckCount = 0;
}

void ANoclipEnemy::PlayAction(ENoclipEnemyAction NewAction, float Duration, float Rate)
{
	ActionRep.Action = NewAction;
	ActionRep.Serial = static_cast<uint8>(ActionRep.Serial + 1);
	ActionRep.StartServerTime = ServerTime();
	ActionRep.Rate = Rate;
	ActionEndTime = Now() + FMath::Max(Duration, 0.1f);
}

void ANoclipEnemy::FinishAction()
{
	ActionEndTime = -1.0;
	ActionRep.Action = ENoclipEnemyAction::None;

	switch (State)
	{
	case ENoclipEnemyState::Alert:
		SetState(ENoclipEnemyState::Chase);
		break;
	case ENoclipEnemyState::Attack:
		NextAttackTime = Now() + Def().AttackCooldown;
		CurrentAttack = ENoclipEnemyAction::None;
		if (Kind == ENoclipEnemyKind::Hound)
		{
			// Morde e recua um pouco: da uma chance de fugir (e evita morder em cadeia).
			const ANoclipCharacter* T = Target.Get();
			StartRetreat(T ? T->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 100.f, 1.1f);
		}
		else
		{
			SetState(ENoclipEnemyState::Chase);
		}
		break;
	default:
		break;
	}
}

// ===================================================================== Percepcao

bool ANoclipEnemy::IsTargetable(const ANoclipCharacter* Player) const
{
	if (!IsValid(Player) || !Player->GetController())
	{
		return false;
	}
	const ANoclipEnemyDirector* Dir = Director.Get();
	return !(Dir && Dir->IsSafeLocation(Player->GetActorLocation()));
}

bool ANoclipEnemy::HasLineOfSight(const FVector& From, const ANoclipCharacter* Player) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipEnemySight), false, this);
	Params.AddIgnoredActor(Player);
	const FVector Points[2] = { Player->GetEyeLocation(), Player->GetActorLocation() };
	for (const FVector& P : Points)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, From, P, ECC_Visibility, Params))
		{
			return true;
		}
		// Outra entidade na frente nao tapa a visao.
		if (Cast<ANoclipEnemy>(Hit.GetActor()))
		{
			return true;
		}
	}
	return false;
}

bool ANoclipEnemy::CanSee(const ANoclipCharacter* Player, float& OutDistance) const
{
	const FNoclipEnemyDef& D = Def();
	const FVector Me = GetActorLocation();
	const FVector Eye = Me + FVector(0.f, 0.f, D.CapsuleHalfHeight * 0.7f);
	OutDistance = FVector::Dist(Me, Player->GetActorLocation());

	const bool bLit = IsFlashlightLit(Player);
	float Range = bLit ? D.SightRangeLit : D.SightRange;
	if (bFrenzy && !bLit)
	{
		Range *= 0.75f; // colapso: escuro
	}
	if (OutDistance > D.SenseRadius)
	{
		if (OutDistance > Range)
		{
			return false;
		}
		const FVector To = (Player->GetActorLocation() - Me).GetSafeNormal2D();
		if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), To) < FMath::Cos(FMath::DegreesToRadians(D.SightHalfAngleDeg)))
		{
			return false;
		}
	}
	return HasLineOfSight(Eye, Player);
}

ANoclipCharacter* ANoclipEnemy::FindVisibleTarget(float& OutDistance) const
{
	ANoclipCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		ANoclipCharacter* P = *It;
		if (!IsTargetable(P))
		{
			continue;
		}
		float Dist = 0.f;
		// A presa atual conta um pouco mais perto (nao fica trocando de alvo).
		if (CanSee(P, Dist))
		{
			const float Score = P == Target.Get() ? Dist * 0.7f : Dist;
			if (Score < BestDist)
			{
				BestDist = Score;
				Best = P;
				OutDistance = Dist;
			}
		}
	}
	return Best;
}

float ANoclipEnemy::HearBestNoise(FVector& OutLocation, TWeakObjectPtr<AActor>& OutSource) const
{
	const FNoclipEnemyDef& D = Def();
	UNoclipNoiseSubsystem* Noise = GetWorld() ? GetWorld()->GetSubsystem<UNoclipNoiseSubsystem>() : nullptr;
	if (!Noise || D.HearingScale <= 0.f)
	{
		return 0.f;
	}
	const double T = Now();
	const FVector Me = GetActorLocation();
	const FVector Ear = Me + FVector(0.f, 0.f, D.CapsuleHalfHeight * 0.6f);
	const ANoclipEnemyDirector* Dir = Director.Get();
	float Best = 0.f;
	for (const FNoclipNoise& N : Noise->GetRecentNoises(0.5))
	{
		if (T - N.Time > 0.5 || N.Source.Get() == this || (Dir && Dir->IsSafeLocation(N.Location)))
		{
			continue;
		}
		float Radius = N.Radius * D.HearingScale * (bFrenzy ? 1.25f : 1.f);
		const float Dist = FVector::Dist(N.Location, Me);
		if (Dist >= Radius)
		{
			continue;
		}
		// Parede no meio abafa.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipEnemyHear), false, this);
		if (AActor* Src = N.Source.Get())
		{
			Params.AddIgnoredActor(Src);
		}
		if (GetWorld()->LineTraceTestByChannel(Ear, N.Location + FVector(0.f, 0.f, 40.f), ECC_Visibility, Params))
		{
			Radius *= 0.6f;
			if (Dist >= Radius)
			{
				continue;
			}
		}
		const float Score = 1.f - Dist / Radius;
		if (Score > Best)
		{
			Best = Score;
			OutLocation = N.Location;
			OutSource = N.Source;
		}
	}
	return Best;
}

void ANoclipEnemy::UpdateLight(float DeltaSeconds)
{
	const FNoclipEnemyDef& D = Def();
	if (D.LightExposureToFlinch <= 0.f || Now() < LightImmuneUntil
		|| State == ENoclipEnemyState::Stunned || State == ENoclipEnemyState::Retreat)
	{
		LightExposure = 0.f;
		return;
	}
	const FVector Head = GetActorLocation() + FVector(0.f, 0.f, D.CapsuleHalfHeight * 0.75f);
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(14.f));
	bool bLit = false;
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It && !bLit; ++It)
	{
		const ANoclipCharacter* P = *It;
		if (!IsValid(P) || !IsFlashlightLit(P))
		{
			continue;
		}
		const FVector From = P->GetEyeLocation();
		const FVector ToMe = Head - From;
		const float Dist = ToMe.Size();
		if (Dist > 1200.f || Dist < 1.f)
		{
			continue;
		}
		if (FVector::DotProduct(P->GetAimRotation().Vector(), ToMe / Dist) < MinDot)
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipEnemyLight), false, P);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, Head, ECC_Visibility, Params) && Hit.GetActor() != this)
		{
			continue;
		}
		bLit = true;
		LightFrom = From;
	}

	LightExposure = bLit ? LightExposure + DeltaSeconds : FMath::Max(0.f, LightExposure - DeltaSeconds * 0.5f);
	if (LightExposure >= D.LightExposureToFlinch)
	{
		// Luz no rosto: cobre a cara e recua (GDD: lanterna como contra-jogo).
		LightExposure = 0.f;
		PendingHitTime = -1.0;
		CurrentAttack = ENoclipEnemyAction::None;
		ClearGoal();
		const float StunTime = 0.8f;
		PlayAction(ENoclipEnemyAction::Hit, AnimLength(ENoclipEnemyAnim::Hit) / 0.8f, 0.8f);
		SetState(ENoclipEnemyState::Stunned, StunTime);
		bFlinchRetreat = true;
		LightImmuneUntil = Now() + StunTime + D.RetreatSeconds + D.LightImmunitySeconds;
		UE_LOG(LogNoclip, Verbose, TEXT("[Entidades] %s recuou da lanterna."), *GetName());
	}
}

// ===================================================================== Cerebro

void ANoclipEnemy::Think(float DeltaSeconds)
{
	if (Kind == ENoclipEnemyKind::Shambler)
	{
		ThinkShambler(DeltaSeconds);
	}
	else
	{
		ThinkHound(DeltaSeconds);
	}
}

void ANoclipEnemy::PickWanderGoal()
{
	const ANoclipEnemyDirector* Dir = Director.Get();
	if (!Dir || !Dir->GetNavGrid().IsValid())
	{
		SetState(ENoclipEnemyState::Idle, 3.f);
		return;
	}
	const FNoclipEnemyDef& D = Def();
	FVector Around = GetActorLocation();
	float MinD = 600.f, MaxD = 2200.f;
	// Colapso: vagam na direcao de alguem (pressao do fim de turno).
	if (bFrenzy)
	{
		float Closest = TNumericLimits<float>::Max();
		for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
		{
			if (IsTargetable(*It))
			{
				const float Dd = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
				if (Dd < Closest)
				{
					Closest = Dd;
					Around = It->GetActorLocation();
				}
			}
		}
		MinD = 0.f;
		MaxD = 900.f;
	}
	FVector P;
	if (Dir->GetNavGrid().RandomWalkable(Stream, D.NavClearance, Around, MinD, MaxD, P))
	{
		SetState(ENoclipEnemyState::Wander);
		SetGoal(P, true);
		if (!bHasGoal)
		{
			SetState(ENoclipEnemyState::Idle, 1.f);
		}
	}
	else
	{
		SetState(ENoclipEnemyState::Idle, 2.f);
	}
}

void ANoclipEnemy::StartRetreat(const FVector& AwayFrom, float Seconds)
{
	SetState(ENoclipEnemyState::Retreat, Seconds);
	ClearGoal();
	const ANoclipEnemyDirector* Dir = Director.Get();
	if (!Dir)
	{
		return;
	}
	const FVector Me = GetActorLocation();
	FVector Away = (Me - AwayFrom).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -GetActorForwardVector().GetSafeNormal2D();
	}
	const float Dist = SpeedForState() * Seconds;
	for (const float Angle : { 0.f, 35.f, -35.f, 70.f, -70.f, 110.f, -110.f })
	{
		const FVector Candidate = Me + Away.RotateAngleAxis(Angle, FVector::UpVector) * Dist;
		if (Dir->GetNavGrid().HasStraightPath(Me, Candidate, Def().NavClearance))
		{
			SetGoal(Candidate, true);
			return;
		}
	}
}

void ANoclipEnemy::StartAttack()
{
	const FNoclipEnemyDef& D = Def();
	CurrentAttack = Stream.FRand() < 0.5f ? ENoclipEnemyAction::Attack : ENoclipEnemyAction::Attack2;
	const bool bFirst = CurrentAttack == ENoclipEnemyAction::Attack;
	const float Rate = bFrenzy ? 1.15f : 1.f;
	const float Len = AnimLength(bFirst ? ENoclipEnemyAnim::Attack : ENoclipEnemyAnim::Attack2);
	const float HitAt = bFirst ? D.AttackHitTime : D.Attack2HitTime;
	ClearGoal();
	SetState(ENoclipEnemyState::Attack);
	PlayAction(CurrentAttack, FMath::Max(Len / Rate - 0.1f, HitAt / Rate + 0.2f), Rate);
	PendingHitTime = Now() + HitAt / Rate;
}

void ANoclipEnemy::ResolveAttackHit()
{
	ANoclipCharacter* T = Target.Get();
	if (!T || !IsTargetable(T))
	{
		return;
	}
	const FNoclipEnemyDef& D = Def();
	const FVector To = T->GetActorLocation() - GetActorLocation();
	if (To.Size2D() > D.AttackRange + 60.f || FacingDot(T) < FMath::Cos(FMath::DegreesToRadians(75.f)))
	{
		return; // esquivou
	}
	if (!HasLineOfSight(GetActorLocation() + FVector(0.f, 0.f, D.CapsuleHalfHeight * 0.5f), T))
	{
		return;
	}
	T->ApplyHurt(D.AttackDamage * (bFrenzy ? 1.2f : 1.f), GetActorLocation(), Kind == ENoclipEnemyKind::Hound ? ENoclipDeathCause::Hound : ENoclipDeathCause::Shambler);
	// Levou o golpe: larga o que estava carregando (o achado sai rolando).
	if (UNoclipGrabComponent* Grab = T->GetGrabComponent())
	{
		Grab->ForceDrop();
	}
	UNoclipNoiseSubsystem::Report(this, T->GetActorLocation(), 900.f, this, false);
	UE_LOG(LogNoclip, Log, TEXT("[Entidades] %s atingiu %s (vida %.0f)"), *GetName(), *T->GetName(), T->GetHealth());
}

void ANoclipEnemy::NotifyStruck(AActor* Object, float ImpactSpeed)
{
	if (!HasAuthority() || ImpactSpeed < 380.f || State == ENoclipEnemyState::Stunned)
	{
		return;
	}
	const FNoclipEnemyDef& D = Def();
	PendingHitTime = -1.0;
	CurrentAttack = ENoclipEnemyAction::None;
	ClearGoal();
	bFlinchRetreat = false;
	const float Stun = D.StunSeconds * FMath::Clamp(ImpactSpeed / 700.f, 0.8f, 1.5f);
	PlayAction(ENoclipEnemyAction::Hit, FMath::Min(AnimLength(ENoclipEnemyAnim::Hit), Stun), 1.f);
	SetState(ENoclipEnemyState::Stunned, Stun);
	if (Object)
	{
		LastKnownLocation = Object->GetActorLocation();
	}
	UE_LOG(LogNoclip, Log, TEXT("[Entidades] %s atordoado por %s (%.0f cm/s)"), *GetName(), Object ? *Object->GetName() : TEXT("?"), ImpactSpeed);
}

void ANoclipEnemy::ForceTarget(ANoclipCharacter* NewTarget)
{
	if (!NewTarget)
	{
		return;
	}
	Target = NewTarget;
	LastKnownLocation = NewTarget->GetActorLocation();
	LastSeenTime = LastHeardTime = Now();
	SetState(ENoclipEnemyState::Chase);
}

void ANoclipEnemy::ThinkShambler(float DeltaSeconds)
{
	const FNoclipEnemyDef& D = Def();
	const double T = Now();

	UpdateLight(DeltaSeconds);

	if (State == ENoclipEnemyState::Stunned)
	{
		if (T >= StateEndTime)
		{
			if (bFlinchRetreat)
			{
				bFlinchRetreat = false;
				StartRetreat(LightFrom, D.RetreatSeconds);
			}
			else
			{
				SetState(Target.IsValid() ? ENoclipEnemyState::Chase : ENoclipEnemyState::Search, 2.f);
			}
		}
		return;
	}
	if (State == ENoclipEnemyState::Retreat)
	{
		if (T >= StateEndTime || !bHasGoal)
		{
			SetState(ENoclipEnemyState::Search, 2.f);
			ClearGoal();
		}
		return;
	}
	if (State == ENoclipEnemyState::Alert || State == ENoclipEnemyState::Attack)
	{
		return;
	}

	float Dist = 0.f;
	ANoclipCharacter* Seen = FindVisibleTarget(Dist);
	if (Seen)
	{
		Target = Seen;
		LastKnownLocation = Seen->GetActorLocation();
		LastSeenTime = T;
		if (State != ENoclipEnemyState::Chase)
		{
			ClearGoal();
			if (T >= NextAlertTime)
			{
				// Grita ao ver alguem: o grito chama os caes.
				NextAlertTime = T + 18.0;
				SetState(ENoclipEnemyState::Alert);
				PlayAction(ENoclipEnemyAction::Alert, D.AlertSeconds, D.AlertRate);
				if (D.AlertNoiseRadius > 0.f)
				{
					UNoclipNoiseSubsystem::Report(this, GetActorLocation(), D.AlertNoiseRadius, this, false);
				}
				return;
			}
			SetState(ENoclipEnemyState::Chase);
		}
	}

	FVector NoiseLoc = FVector::ZeroVector;
	TWeakObjectPtr<AActor> NoiseSrc;
	const float Heard = HearBestNoise(NoiseLoc, NoiseSrc);

	switch (State)
	{
	case ENoclipEnemyState::Chase:
	{
		ANoclipCharacter* P = Target.Get();
		if (!P || !IsTargetable(P))
		{
			Target.Reset();
			ClearGoal();
			SetState(ENoclipEnemyState::Search, 3.f);
			break;
		}
		// Logo depois de perder de vista ainda "sabe" para onde a presa foi.
		const bool bFresh = T - LastSeenTime < 1.5;
		if (bFresh)
		{
			LastKnownLocation = P->GetActorLocation();
		}
		if (T - LastSeenTime > D.LoseTargetSeconds)
		{
			SetState(ENoclipEnemyState::Investigate);
			SetGoal(LastKnownLocation, true);
			break;
		}
		if (Seen == P && Dist <= D.AttackRange && T >= NextAttackTime && FacingDot(P) > 0.5f)
		{
			StartAttack();
			break;
		}
		SetGoal(LastKnownLocation);
		if (!bHasGoal && !bFresh && FVector::Dist2D(GetActorLocation(), LastKnownLocation) < 150.f)
		{
			SetState(ENoclipEnemyState::Search, 3.f);
		}
		break;
	}
	case ENoclipEnemyState::Investigate:
		if (Heard > 0.f)
		{
			SetGoal(NoiseLoc);
		}
		if (!bHasGoal)
		{
			SetState(ENoclipEnemyState::Search, 3.f);
		}
		break;
	case ENoclipEnemyState::Search:
		if (Heard > 0.f)
		{
			SetState(ENoclipEnemyState::Investigate);
			SetGoal(NoiseLoc, true);
		}
		else if (T >= StateEndTime)
		{
			PickWanderGoal();
		}
		break;
	case ENoclipEnemyState::Idle:
	case ENoclipEnemyState::Wander:
		if (Heard > 0.f)
		{
			SetState(ENoclipEnemyState::Investigate);
			SetGoal(NoiseLoc, true);
		}
		else if (State == ENoclipEnemyState::Idle && T >= StateEndTime)
		{
			PickWanderGoal();
		}
		else if (State == ENoclipEnemyState::Wander && !bHasGoal)
		{
			SetState(ENoclipEnemyState::Idle, Stream.FRandRange(2.f, 5.f));
		}
		break;
	default:
		break;
	}
}

void ANoclipEnemy::ThinkHound(float DeltaSeconds)
{
	const FNoclipEnemyDef& D = Def();
	const double T = Now();

	if (State == ENoclipEnemyState::Stunned)
	{
		if (T >= StateEndTime)
		{
			// Volta furioso para onde veio a pancada.
			LastHeardTime = T;
			SetState(ENoclipEnemyState::Chase);
			SetGoal(LastKnownLocation, true);
		}
		return;
	}
	if (State == ENoclipEnemyState::Retreat)
	{
		if (T >= StateEndTime || !bHasGoal)
		{
			ClearGoal();
			LastHeardTime = T;
			SetState(ENoclipEnemyState::Chase);
		}
		return;
	}
	if (State == ENoclipEnemyState::Alert || State == ENoclipEnemyState::Attack)
	{
		return;
	}

	float Dist = 0.f;
	ANoclipCharacter* Seen = FindVisibleTarget(Dist);
	FVector NoiseLoc = FVector::ZeroVector;
	TWeakObjectPtr<AActor> NoiseSrc;
	const float Heard = HearBestNoise(NoiseLoc, NoiseSrc);

	if (Seen)
	{
		Target = Seen;
		LastKnownLocation = Seen->GetActorLocation();
		LastSeenTime = LastHeardTime = T;
	}
	else if (Heard > 0.f)
	{
		// Vai atras do barulho mais forte, seja de quem for (achado arremessado vira isca).
		LastKnownLocation = NoiseLoc;
		LastHeardTime = T;
		if (ANoclipCharacter* P = Cast<ANoclipCharacter>(NoiseSrc.Get()))
		{
			Target = P;
		}
	}

	switch (State)
	{
	case ENoclipEnemyState::Idle:
	case ENoclipEnemyState::Wander:
	case ENoclipEnemyState::Search:
	case ENoclipEnemyState::Investigate:
		if (Seen || Heard >= 0.3f)
		{
			ClearGoal();
			if (T >= NextAlertTime)
			{
				// Uiva ao pegar o rastro.
				NextAlertTime = T + 25.0;
				SetState(ENoclipEnemyState::Alert);
				PlayAction(ENoclipEnemyAction::Alert, D.AlertSeconds, D.AlertRate);
			}
			else
			{
				SetState(ENoclipEnemyState::Chase);
			}
		}
		else if (Heard > 0.f)
		{
			if (State != ENoclipEnemyState::Investigate)
			{
				SetState(ENoclipEnemyState::Investigate);
			}
			SetGoal(NoiseLoc);
		}
		else if (State == ENoclipEnemyState::Investigate && !bHasGoal)
		{
			SetState(ENoclipEnemyState::Search, 2.5f);
		}
		else if ((State == ENoclipEnemyState::Search || State == ENoclipEnemyState::Idle) && T >= StateEndTime)
		{
			PickWanderGoal();
		}
		else if (State == ENoclipEnemyState::Wander && !bHasGoal)
		{
			SetState(ENoclipEnemyState::Idle, Stream.FRandRange(1.5f, 4.f));
		}
		break;
	case ENoclipEnemyState::Chase:
	{
		if (Seen && Dist <= D.AttackRange && T >= NextAttackTime && FacingDot(Seen) > 0.4f)
		{
			StartAttack();
			break;
		}
		SetGoal(Seen ? Seen->GetActorLocation() : LastKnownLocation);
		if (!Seen && T - LastHeardTime > D.LoseTargetSeconds && (!bHasGoal || FVector::Dist2D(GetActorLocation(), LastKnownLocation) < 150.f))
		{
			// Perdeu o rastro: fareja em volta.
			ClearGoal();
			SetState(ENoclipEnemyState::Search, 2.5f);
		}
		break;
	}
	default:
		break;
	}
}

// ===================================================================== Movimento

float ANoclipEnemy::SpeedForState() const
{
	const FNoclipEnemyDef& D = Def();
	float Speed = D.WanderSpeed;
	switch (State)
	{
	case ENoclipEnemyState::Investigate:
	case ENoclipEnemyState::Search:      Speed = D.InvestigateSpeed; break;
	case ENoclipEnemyState::Chase:       Speed = D.ChaseSpeed; break;
	case ENoclipEnemyState::Retreat:     Speed = D.InvestigateSpeed * 1.1f; break;
	default: break;
	}
	return Speed * (bFrenzy ? 1.15f : 1.f);
}

void ANoclipEnemy::ClearGoal()
{
	bHasGoal = false;
	Path.Reset();
	PathIndex = 0;
}

void ANoclipEnemy::SetGoal(const FVector& NewGoal, bool bForceRepath)
{
	const double T = Now();
	if (bHasGoal && !bForceRepath)
	{
		if (FVector::Dist2D(NewGoal, Goal) < 120.f || T < NextRepathTime)
		{
			return;
		}
	}
	const ANoclipEnemyDirector* Dir = Director.Get();
	Goal = NewGoal;
	NextRepathTime = T + 0.35;
	Path.Reset();
	PathIndex = 0;
	bHasGoal = Dir && Dir->GetNavGrid().FindPath(GetActorLocation(), NewGoal, Def().NavClearance, Path);
}

bool ANoclipEnemy::FollowPath(float DeltaSeconds)
{
	if (!bHasGoal || PathIndex >= Path.Num())
	{
		ClearGoal();
		return true;
	}
	const FVector Me = GetActorLocation();
	auto Accept = [this]() { return PathIndex == Path.Num() - 1 ? 70.f : 50.f; };
	while (FVector::Dist2D(Me, Path[PathIndex]) < Accept())
	{
		if (++PathIndex >= Path.Num())
		{
			ClearGoal();
			return true;
		}
	}

	FVector Dir = Path[PathIndex] - Me;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	// Nao se empilham: afasta das outras entidades proximas.
	if (const ANoclipEnemyDirector* D = Director.Get())
	{
		for (const ANoclipEnemy* Other : D->GetEnemies())
		{
			if (!IsValid(Other) || Other == this)
			{
				continue;
			}
			const FVector Away = Me - Other->GetActorLocation();
			const float Dd = Away.Size2D();
			if (Dd < 120.f && Dd > 1.f)
			{
				Dir += Away.GetSafeNormal2D() * 0.6f * (1.f - Dd / 120.f);
			}
		}
		Dir = Dir.GetSafeNormal2D();
	}
	AddMovementInput(Dir, 1.f);

	// Preso (quina, achado no caminho, colega): replaneja; depois de algumas vezes desiste do destino.
	if (GetVelocity().Size2D() < 25.f)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 1.2)
		{
			StuckTime = 0.0;
			if (++StuckCount >= 3)
			{
				StuckCount = 0;
				ClearGoal();
				return true;
			}
			SetGoal(Goal, true);
		}
	}
	else
	{
		StuckTime = FMath::Max(0.0, StuckTime - DeltaSeconds);
	}
	return false;
}

void ANoclipEnemy::FaceTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector To = Location - GetActorLocation();
	if (To.SizeSquared2D() < 1.f)
	{
		return;
	}
	const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Want, DeltaSeconds, Def().TurnRate));
}

void ANoclipEnemy::DebugDraw() const
{
	UWorld* World = GetWorld();
	const FNoclipEnemyDef& D = Def();
	const FVector Me = GetActorLocation();
	const FColor Color = State == ENoclipEnemyState::Chase || State == ENoclipEnemyState::Attack ? FColor::Red
		: (State == ENoclipEnemyState::Investigate || State == ENoclipEnemyState::Alert ? FColor::Orange : FColor::Green);
	DrawDebugString(World, Me + FVector(0.f, 0.f, D.CapsuleHalfHeight + 40.f),
		FString::Printf(TEXT("%s: %s%s"), *D.Name.ToString(), StateName(State), bFrenzy ? TEXT(" (frenesi)") : TEXT("")), nullptr, Color, -1.f, true, 1.2f);
	FVector Prev = Me;
	for (int32 i = PathIndex; i < Path.Num(); ++i)
	{
		const FVector P(Path[i].X, Path[i].Y, Me.Z);
		DrawDebugLine(World, Prev, P, Color, false, -1.f, 0, 2.f);
		Prev = P;
	}
	if (CVarNoclipDebugEnemies.GetValueOnGameThread() > 1)
	{
		DrawDebugCircle(World, Me, D.SightRange, 32, FColor(80, 80, 255), false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		DrawDebugCircle(World, Me, D.SenseRadius, 24, FColor(255, 80, 255), false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
}
