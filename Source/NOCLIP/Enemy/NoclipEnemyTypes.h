// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NoclipEnemyTypes.generated.h"

class USkeletalMesh;
class UAnimSequence;

/** As entidades do Level 0. Cada uma tem UMA regra que o grupo pode explorar (GDD, Entidades). */
UENUM(BlueprintType)
enum class ENoclipEnemyKind : uint8
{
	/** Cao: quase cego, caca pelo barulho mais alto. Silencio e iscas (arremessar achados) o despistam. */
	Hound,
	/** Funcionario que nunca saiu: lento e teimoso, caca pela visao (a lanterna te denuncia). Luz no rosto o faz recuar. */
	Shambler,
	Count UMETA(Hidden)
};

/** Estado da IA (servidor; replicado para animacao e depuracao). */
UENUM(BlueprintType)
enum class ENoclipEnemyState : uint8
{
	Idle,
	Wander,
	/** Indo ver um barulho (cao) ou o ultimo lugar onde viu alguem (shambler). */
	Investigate,
	/** Uivo / grito ao achar uma presa (parado). */
	Alert,
	Chase,
	Attack,
	/** Atingido por um achado arremessado ou pela luz no rosto. */
	Stunned,
	/** Fugindo da luz (shambler) / recuando depois de morder (cao). */
	Retreat,
	/** Cheirando em volta do ultimo ponto conhecido. */
	Search
};

/** Animacao de uma vez so, tocada por cima da locomocao. */
UENUM(BlueprintType)
enum class ENoclipEnemyAction : uint8
{
	None,
	Attack,
	Attack2,
	Hit,
	/** Uivo (cao) / grito (shambler). */
	Alert,
	Death
};

enum class ENoclipEnemyAnim : uint8
{
	Idle,
	Walk,
	Run,
	Attack,
	Attack2,
	Hit,
	Alert,
	Death,
	Count
};

/** Ficha de uma entidade: modelos, animacoes e numeros de jogo. */
struct NOCLIP_API FNoclipEnemyDef
{
	ENoclipEnemyKind Kind = ENoclipEnemyKind::Hound;
	FText Name;

	/** Variantes visuais (mesmo comportamento). Cada variante tem o proprio esqueleto e as proprias animacoes. */
	TArray<FString> MeshPaths;
	TArray<TArray<FString>> AnimPaths; // [variante][ENoclipEnemyAnim]

	// Corpo
	float CapsuleRadius = 40.f;
	float CapsuleHalfHeight = 50.f;
	/** Yaw do modelo para olhar para +X do ator. */
	float MeshYaw = -90.f;
	float MeshScale = 1.f;
	/** Folga das paredes ao planejar o caminho (cm). */
	float NavClearance = 45.f;
	float TurnRate = 360.f;
	float Acceleration = 1600.f;

	// Velocidades (cm/s)
	float WanderSpeed = 150.f;
	float InvestigateSpeed = 250.f;
	float ChaseSpeed = 500.f;
	/** Velocidade em que o ciclo de andar / correr toca a 1x. */
	float WalkAnimSpeed = 150.f;
	float RunAnimSpeed = 500.f;

	// Ataque
	float AttackRange = 170.f;
	float AttackDamage = 20.f;
	/** Momento do golpe dentro de cada animacao de ataque (s). */
	float AttackHitTime = 0.5f;
	float Attack2HitTime = 0.45f;
	float AttackCooldown = 0.6f;

	// Percepcao
	/** Alcance da visao com o alvo no escuro / com a lanterna dele acesa (cm). */
	float SightRange = 500.f;
	float SightRangeLit = 500.f;
	float SightHalfAngleDeg = 70.f;
	/** Abaixo disso percebe em qualquer direcao (cheiro / respiracao). */
	float SenseRadius = 300.f;
	/** Multiplica o raio dos barulhos que ouve (0 = surdo). */
	float HearingScale = 0.f;
	/** Desiste depois de tanto tempo sem ver / ouvir a presa (s). */
	float LoseTargetSeconds = 5.f;

	/** Duracao do uivo / grito (s) e a velocidade em que a animacao toca. */
	float AlertSeconds = 1.8f;
	float AlertRate = 1.f;
	/** Raio do barulho que o proprio grito faz (atrai os caes). 0 = nenhum. */
	float AlertNoiseRadius = 0.f;

	/** Atordoamento por um achado arremessado (s). */
	float StunSeconds = 1.2f;

	/** Luz da lanterna no rosto: exposicao para recuar, quanto recua e quanto tempo fica imune. 0 = ignora a luz. */
	float LightExposureToFlinch = 0.f;
	float RetreatSeconds = 3.f;
	float LightImmunitySeconds = 6.f;
};

namespace NoclipEnemies
{
	NOCLIP_API const FNoclipEnemyDef& Get(ENoclipEnemyKind Kind);
	NOCLIP_API int32 NumVariants(ENoclipEnemyKind Kind);
	NOCLIP_API USkeletalMesh* LoadMesh(ENoclipEnemyKind Kind, int32 Variant);
	NOCLIP_API UAnimSequence* LoadAnim(ENoclipEnemyKind Kind, int32 Variant, ENoclipEnemyAnim Anim);
	NOCLIP_API ENoclipEnemyAnim AnimForAction(ENoclipEnemyAction Action);
}
