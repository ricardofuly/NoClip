// Copyright Fuly Games. All Rights Reserved.

#include "Enemy/NoclipEnemyTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipEnemyTypes)

#define LOCTEXT_NAMESPACE "NoclipEnemies"

namespace
{
	/** Sobe quando os numeros mudam: estaticos de funcao nao reiniciam com o Live Coding. */
	constexpr int32 EnemyTableVersion = 1;

	void AddVariant(FNoclipEnemyDef& D, const FString& Folder, const FString& Name, const TCHAR* AlertSuffix)
	{
		const FString Base = Folder / Name + TEXT("/SkeletalMeshes/") + Name;
		D.MeshPaths.Add(FString::Printf(TEXT("%s.%s"), *Base, *Name));
		TArray<FString>& A = D.AnimPaths.AddDefaulted_GetRef();
		A.SetNum(static_cast<int32>(ENoclipEnemyAnim::Count));
		auto Path = [&](const TCHAR* Suffix) { return FString::Printf(TEXT("%s%s.%s%s"), *Base, Suffix, *Name, Suffix); };
		A[static_cast<int32>(ENoclipEnemyAnim::Idle)] = Path(TEXT("Idle"));
		A[static_cast<int32>(ENoclipEnemyAnim::Walk)] = Path(TEXT("Walk"));
		A[static_cast<int32>(ENoclipEnemyAnim::Run)] = Path(TEXT("Run"));
		A[static_cast<int32>(ENoclipEnemyAnim::Attack)] = Path(TEXT("Attack"));
		A[static_cast<int32>(ENoclipEnemyAnim::Attack2)] = Path(TEXT("Attack2"));
		A[static_cast<int32>(ENoclipEnemyAnim::Hit)] = Path(TEXT("Hit"));
		A[static_cast<int32>(ENoclipEnemyAnim::Alert)] = Path(AlertSuffix);
		A[static_cast<int32>(ENoclipEnemyAnim::Death)] = Path(TEXT("Death"));
	}

	TArray<FNoclipEnemyDef> BuildEnemyTable()
	{
		TArray<FNoclipEnemyDef> Defs;
		Defs.AddDefaulted(static_cast<int32>(ENoclipEnemyKind::Count));

		// ---------- Cao ----------
		// Bicho de ~1,9 m de comprimento e 1 m de altura. Mais lento que o sprint e mais rapido que andar:
		// quem corre faz barulho; quem anda escapa devagar (e quem carrega peso nao escapa).
		{
			FNoclipEnemyDef& D = Defs[static_cast<int32>(ENoclipEnemyKind::Hound)];
			D.Kind = ENoclipEnemyKind::Hound;
			D.Name = LOCTEXT("Hound", "Cão");
			AddVariant(D, TEXT("/Game/NOCLIP/Characters/Hound"), TEXT("hound_black"), TEXT("Howl"));
			AddVariant(D, TEXT("/Game/NOCLIP/Characters/Hound"), TEXT("hound_flayed"), TEXT("Howl"));
			D.CapsuleRadius = 42.f;
			D.CapsuleHalfHeight = 50.f;
			D.NavClearance = 55.f;
			D.TurnRate = 540.f;
			D.Acceleration = 2400.f;
			D.WanderSpeed = 170.f;
			D.InvestigateSpeed = 420.f;
			D.ChaseSpeed = 610.f;
			D.WalkAnimSpeed = 170.f;
			D.RunAnimSpeed = 610.f;
			D.AttackRange = 185.f;
			D.AttackDamage = 20.f;
			D.AttackHitTime = 0.5f;
			D.Attack2HitTime = 0.45f;
			D.AttackCooldown = 0.5f;
			D.SightRange = 450.f;      // quase cego
			D.SightRangeLit = 450.f;
			D.SightHalfAngleDeg = 60.f;
			D.SenseRadius = 260.f;     // farejo
			D.HearingScale = 1.f;
			D.LoseTargetSeconds = 2.5f;
			D.AlertSeconds = 1.6f;     // o uivo inteiro (3 s) e longo demais: toca acelerado
			D.AlertRate = 1.6f;
			D.AlertNoiseRadius = 0.f;
			D.StunSeconds = 1.4f;
			D.LightExposureToFlinch = 0.f; // cego: a lanterna nao adianta
		}

		// ---------- Shambler ----------
		// Humanoide de ~1,9 m. Mais lento que andar: assusta pelo cerco, nao pela corrida. Quem carrega peso
		// anda na mesma velocidade que ele.
		{
			FNoclipEnemyDef& D = Defs[static_cast<int32>(ENoclipEnemyKind::Shambler)];
			D.Kind = ENoclipEnemyKind::Shambler;
			D.Name = LOCTEXT("Shambler", "Funcionário");
			AddVariant(D, TEXT("/Game/NOCLIP/Characters/Shambler"), TEXT("shambler_patient"), TEXT("Scream"));
			AddVariant(D, TEXT("/Game/NOCLIP/Characters/Shambler"), TEXT("shambler_worker"), TEXT("Scream"));
			D.CapsuleRadius = 34.f;
			D.CapsuleHalfHeight = 92.f;
			D.NavClearance = 45.f;
			D.TurnRate = 220.f;
			D.Acceleration = 900.f;
			D.WanderSpeed = 110.f;
			D.InvestigateSpeed = 180.f;
			D.ChaseSpeed = 300.f;
			D.WalkAnimSpeed = 110.f;
			D.RunAnimSpeed = 300.f;
			D.AttackRange = 165.f;
			D.AttackDamage = 30.f;
			D.AttackHitTime = 0.55f;
			D.Attack2HitTime = 0.45f;
			D.AttackCooldown = 0.7f;
			D.SightRange = 1000.f;
			D.SightRangeLit = 2600.f;   // lanterna acesa denuncia de longe
			D.SightHalfAngleDeg = 65.f;
			D.SenseRadius = 220.f;
			D.HearingScale = 0.35f;     // so barulho alto e perto
			D.LoseTargetSeconds = 6.f;
			D.AlertSeconds = 1.8f;
			D.AlertRate = 1.f;
			D.AlertNoiseRadius = 3200.f; // o grito chama os caes
			D.StunSeconds = 1.f;
			D.LightExposureToFlinch = 0.5f;
			D.RetreatSeconds = 3.f;
			D.LightImmunitySeconds = 6.f;
		}
		return Defs;
	}

	const TArray<FNoclipEnemyDef>& EnemyTable()
	{
		static TArray<FNoclipEnemyDef> Defs;
		static int32 BuiltVersion = INDEX_NONE;
		if (BuiltVersion != EnemyTableVersion || Defs.Num() == 0)
		{
			Defs = BuildEnemyTable();
			BuiltVersion = EnemyTableVersion;
		}
		return Defs;
	}

	template <typename T>
	T* LoadEnemyAsset(const FString& Path)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UObject>* Found = Cache.Find(Path); Found && Found->IsValid())
		{
			return Cast<T>(Found->Get());
		}
		T* Obj = LoadObject<T>(nullptr, *Path);
		if (Obj)
		{
			Obj->AddToRoot();
			Cache.Add(Path, Obj);
		}
		return Obj;
	}
}

namespace NoclipEnemies
{
	const FNoclipEnemyDef& Get(ENoclipEnemyKind Kind)
	{
		const TArray<FNoclipEnemyDef>& Defs = EnemyTable();
		const int32 Index = FMath::Clamp(static_cast<int32>(Kind), 0, Defs.Num() - 1);
		return Defs[Index];
	}

	int32 NumVariants(ENoclipEnemyKind Kind)
	{
		return Get(Kind).MeshPaths.Num();
	}

	USkeletalMesh* LoadMesh(ENoclipEnemyKind Kind, int32 Variant)
	{
		const FNoclipEnemyDef& D = Get(Kind);
		return D.MeshPaths.IsValidIndex(Variant) ? LoadEnemyAsset<USkeletalMesh>(D.MeshPaths[Variant]) : nullptr;
	}

	UAnimSequence* LoadAnim(ENoclipEnemyKind Kind, int32 Variant, ENoclipEnemyAnim Anim)
	{
		const FNoclipEnemyDef& D = Get(Kind);
		if (!D.AnimPaths.IsValidIndex(Variant))
		{
			return nullptr;
		}
		const TArray<FString>& A = D.AnimPaths[Variant];
		const int32 Index = static_cast<int32>(Anim);
		return A.IsValidIndex(Index) ? LoadEnemyAsset<UAnimSequence>(A[Index]) : nullptr;
	}

	ENoclipEnemyAnim AnimForAction(ENoclipEnemyAction Action)
	{
		switch (Action)
		{
		case ENoclipEnemyAction::Attack:  return ENoclipEnemyAnim::Attack;
		case ENoclipEnemyAction::Attack2: return ENoclipEnemyAnim::Attack2;
		case ENoclipEnemyAction::Hit:     return ENoclipEnemyAnim::Hit;
		case ENoclipEnemyAction::Alert:   return ENoclipEnemyAnim::Alert;
		case ENoclipEnemyAction::Death:   return ENoclipEnemyAnim::Death;
		default:                          return ENoclipEnemyAnim::Count;
		}
	}
}

#undef LOCTEXT_NAMESPACE
