// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Enemy/NoclipEnemyTypes.h"
#include "NoclipEnemy.generated.h"

class ANoclipCharacter;
class ANoclipEnemyDirector;

/** Acao de uma vez so replicada (o Serial muda a cada disparo, mesmo repetindo a mesma acao). */
USTRUCT()
struct FNoclipEnemyActionRep
{
	GENERATED_BODY()

	UPROPERTY()
	ENoclipEnemyAction Action = ENoclipEnemyAction::None;

	UPROPERTY()
	uint8 Serial = 0;

	/** Hora do servidor (GameState) em que comecou. */
	UPROPERTY()
	float StartServerTime = 0.f;

	UPROPERTY()
	float Rate = 1.f;
};

/**
 * Entidade do Level 0 (cao ou shambler). A IA roda so no servidor, em C++ simples (sem Behavior Tree):
 * percepcao a 10 Hz (visao, cheiro, barulho, luz no rosto), estados e caminho na grade do diretor.
 * O movimento e o do CharacterMovement (replicado e suavizado nos clientes); a animacao vem do estado
 * replicado (velocidade + acao de uma vez so).
 */
UCLASS(NotBlueprintable)
class NOCLIP_API ANoclipEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ANoclipEnemy(const FObjectInitializer& ObjectInitializer);

	/** Servidor, entre o SpawnActorDeferred e o FinishSpawning. */
	void InitEnemy(ENoclipEnemyKind InKind, int32 InVariant, ANoclipEnemyDirector* InDirector, int32 Seed);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	ENoclipEnemyKind GetKind() const { return Kind; }
	ENoclipEnemyState GetState() const { return State; }
	ENoclipEnemyAction GetAction() const { return ActionRep.Action; }
	uint8 GetActionSerial() const { return ActionRep.Serial; }
	float GetActionRate() const { return ActionRep.Rate; }
	/** Tempo de animacao ja tocado da acao atual (ja multiplicado pela velocidade). */
	float GetActionElapsed() const;

	/** Servidor: um achado arremessado (ou caindo) bateu nesta entidade. */
	void NotifyStruck(AActor* Object, float ImpactSpeed);

	/** Servidor: colapso (escuro) — mais rapidos e mais agressivos. */
	void SetFrenzy(bool bInFrenzy) { bFrenzy = bInFrenzy; }

	/** Servidor: forca ir atras de alguem (depuracao / eventos). */
	void ForceTarget(ANoclipCharacter* NewTarget);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	ENoclipEnemyKind Kind = ENoclipEnemyKind::Hound;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	uint8 Variant = 0;

	UPROPERTY(Replicated)
	ENoclipEnemyState State = ENoclipEnemyState::Idle;

	UPROPERTY(Replicated)
	FNoclipEnemyActionRep ActionRep;

	UFUNCTION()
	void OnRep_Visual();

private:
	void ApplyVisual();
	const FNoclipEnemyDef& Def() const { return NoclipEnemies::Get(Kind); }
	double Now() const;
	float ServerTime() const;

	// ---------- Servidor: cerebro ----------
	void Think(float DeltaSeconds);
	void ThinkHound(float DeltaSeconds);
	void ThinkShambler(float DeltaSeconds);
	void UpdateLight(float DeltaSeconds);
	ANoclipCharacter* FindVisibleTarget(float& OutDistance) const;
	bool CanSee(const ANoclipCharacter* Player, float& OutDistance) const;
	bool HasLineOfSight(const FVector& From, const ANoclipCharacter* Player) const;
	bool IsTargetable(const ANoclipCharacter* Player) const;
	/** Barulho mais "forte" que alcanca este bicho (0..1) e onde foi. */
	float HearBestNoise(FVector& OutLocation, TWeakObjectPtr<AActor>& OutSource) const;

	void SetState(ENoclipEnemyState NewState, float Duration = 0.f);
	void PlayAction(ENoclipEnemyAction NewAction, float Duration, float Rate = 1.f);
	void FinishAction();
	void StartAttack();
	void ResolveAttackHit();
	void PickWanderGoal();
	void TickAction(float DeltaSeconds);

	// ---------- Servidor: movimento ----------
	void SetGoal(const FVector& Goal, bool bForceRepath = false);
	void ClearGoal();
	/** Segue o caminho; verdadeiro ao chegar. */
	bool FollowPath(float DeltaSeconds);
	void FaceTowards(const FVector& Location, float DeltaSeconds);
	float SpeedForState() const;
	void DebugDraw() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ANoclipEnemyDirector> Director;

	TWeakObjectPtr<ANoclipCharacter> Target;
	FVector LastKnownLocation = FVector::ZeroVector;
	double LastSeenTime = -100.0;
	double LastHeardTime = -100.0;
	double StateEndTime = 0.0;
	double ThinkAccumulator = 0.0;

	// Acao
	double ActionEndTime = -1.0;
	double PendingHitTime = -1.0;
	ENoclipEnemyAction CurrentAttack = ENoclipEnemyAction::None;
	double NextAttackTime = 0.0;
	double NextAlertTime = 0.0;

	// Luz (shambler)
	float LightExposure = 0.f;
	double LightImmuneUntil = 0.0;
	FVector LightFrom = FVector::ZeroVector;
	bool bFlinchRetreat = false;
	void StartRetreat(const FVector& AwayFrom, float Seconds);
	float AnimLength(ENoclipEnemyAnim Anim) const;
	float FacingDot(const AActor* Other) const;

	// Caminho
	bool bHasGoal = false;
	FVector Goal = FVector::ZeroVector;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	double NextRepathTime = 0.0;
	double StuckTime = 0.0;
	int32 StuckCount = 0;

	bool bFrenzy = false;
	FRandomStream Stream;
};
