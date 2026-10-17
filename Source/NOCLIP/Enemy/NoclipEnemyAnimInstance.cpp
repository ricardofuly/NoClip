// Copyright Fuly Games. All Rights Reserved.

#include "Enemy/NoclipEnemyAnimInstance.h"
#include "Enemy/NoclipEnemy.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipEnemyAnimInstance)

/** Mistura as camadas por peso no worker de animacao (mesmo esquema do estagiario). */
struct FNoclipEnemyAnimInstanceProxy : public FAnimInstanceProxy
{
	explicit FNoclipEnemyAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		if (const UNoclipEnemyAnimInstance* Inst = Cast<UNoclipEnemyAnimInstance>(InAnimInstance))
		{
			Layers.Reset();
			for (const FNoclipEnemyAnimLayer& L : Inst->PendingLayers)
			{
				Layers.Add(L);
			}
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		float Accumulated = 0.f;
		for (const FNoclipEnemyAnimLayer& L : Layers)
		{
			if (!L.Sequence || L.Weight <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FAnimExtractContext Extract(static_cast<double>(L.Time), false, FDeltaTimeRecord(), L.bLoop);
			if (Accumulated <= 0.f)
			{
				FAnimationPoseData PoseData(Output);
				L.Sequence->GetAnimationPose(PoseData, Extract);
				Accumulated = L.Weight;
				continue;
			}
			FPoseContext Temp(Output);
			Temp.ResetToRefPose();
			FAnimationPoseData TempData(Temp);
			L.Sequence->GetAnimationPose(TempData, Extract);
			const float Alpha = L.Weight / (Accumulated + L.Weight);
			for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
			{
				Output.Pose[BoneIndex].BlendWith(Temp.Pose[BoneIndex], Alpha);
			}
			Accumulated += L.Weight;
		}
		if (Accumulated <= 0.f)
		{
			Output.ResetToRefPose();
		}
		else
		{
			Output.Pose.NormalizeRotations();
		}
		return true;
	}

	TArray<FNoclipEnemyAnimLayer, TInlineAllocator<6>> Layers;
};

FAnimInstanceProxy* UNoclipEnemyAnimInstance::CreateAnimInstanceProxy()
{
	return new FNoclipEnemyAnimInstanceProxy(this);
}

void UNoclipEnemyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FNoclipEnemyAnimInstanceProxy*>(InProxy);
}

void UNoclipEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	IdleTime = FMath::FRandRange(0.f, 2.f); // nao respiram todos juntos
}

void UNoclipEnemyAnimInstance::SetEnemy(ENoclipEnemyKind InKind, int32 InVariant)
{
	if (InKind == Kind && InVariant == Variant && Anims.Num() == static_cast<int32>(ENoclipEnemyAnim::Count))
	{
		return;
	}
	Kind = InKind;
	Variant = InVariant;
	Anims.Reset();
	for (int32 a = 0; a < static_cast<int32>(ENoclipEnemyAnim::Count); ++a)
	{
		Anims.Add(NoclipEnemies::LoadAnim(Kind, Variant, static_cast<ENoclipEnemyAnim>(a)));
	}
}

void UNoclipEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	PendingLayers.Reset();
	if (Variant == INDEX_NONE || Anims.Num() != static_cast<int32>(ENoclipEnemyAnim::Count))
	{
		return;
	}
	const FNoclipEnemyDef& Def = NoclipEnemies::Get(Kind);
	auto Anim = [this](ENoclipEnemyAnim A) -> UAnimSequence* { return Anims[static_cast<int32>(A)]; };
	auto Len = [&Anim](ENoclipEnemyAnim A) -> float
	{
		const UAnimSequence* S = Anim(A);
		return S ? FMath::Max(S->GetPlayLength(), 0.05f) : 1.f;
	};
	auto Push = [this, &Anim, &Len](ENoclipEnemyAnim A, float NormOrTime, float Weight, bool bNormalized, bool bLoop)
	{
		UAnimSequence* S = Anim(A);
		if (!S || Weight <= 0.001f)
		{
			return;
		}
		FNoclipEnemyAnimLayer& L = PendingLayers.AddDefaulted_GetRef();
		L.Sequence = S;
		L.Time = bNormalized ? NormOrTime * Len(A) : FMath::Min(NormOrTime, Len(A));
		L.Weight = Weight;
		L.bLoop = bLoop;
	};

	const ANoclipEnemy* Enemy = Cast<ANoclipEnemy>(GetOwningActor());
	const float RawSpeed = Enemy ? Enemy->GetVelocity().Size2D() : 0.f;
	Speed = FMath::FInterpTo(Speed, RawSpeed, DeltaSeconds, 8.f);

	// ---------- Acao de uma vez so ----------
	const ENoclipEnemyAction NewAction = Enemy ? Enemy->GetAction() : ENoclipEnemyAction::None;
	const uint8 NewSerial = Enemy ? Enemy->GetActionSerial() : 0;
	if (NewSerial != ActionSerial)
	{
		ActionSerial = NewSerial;
		Action = NewAction;
		ActionTime = Enemy ? Enemy->GetActionElapsed() : 0.f; // quem chega atrasado (rede) entra no meio
		ActionRate = Enemy ? Enemy->GetActionRate() : 1.f;
	}
	else
	{
		ActionTime += DeltaSeconds * ActionRate;
	}

	const ENoclipEnemyAnim ActionAnim = NoclipEnemies::AnimForAction(Action);
	const bool bHasAction = Action != ENoclipEnemyAction::None && ActionAnim != ENoclipEnemyAnim::Count && Anim(ActionAnim);
	const float ActionLen = bHasAction ? Len(ActionAnim) : 0.f;
	const bool bHold = Action == ENoclipEnemyAction::Death; // morte segura o ultimo quadro
	const bool bPlaying = bHasAction && (bHold || ActionTime < ActionLen - 0.05f) && NewAction == Action;
	const float BlendOutStart = ActionLen - 0.22f;
	float Target = 0.f;
	if (bPlaying)
	{
		Target = (!bHold && ActionTime > BlendOutStart) ? FMath::Clamp((ActionLen - ActionTime) / 0.22f, 0.f, 1.f) : 1.f;
	}
	ActionWeight = FMath::FInterpConstantTo(ActionWeight, Target, DeltaSeconds, Target > ActionWeight ? 9.f : 5.f);

	// ---------- Locomocao ----------
	const float Moving = FMath::Clamp((Speed - 12.f) / 50.f, 0.f, 1.f);
	const float Gait = FMath::Clamp((Speed - Def.WalkAnimSpeed) / FMath::Max(Def.RunAnimSpeed - Def.WalkAnimSpeed, 1.f), 0.f, 1.f);
	const float Nominal = FMath::Lerp(Def.WalkAnimSpeed, Def.RunAnimSpeed, Gait);
	const float Rate = FMath::Clamp(Speed / FMath::Max(Nominal, 1.f), 0.4f, 1.8f);
	const float CycleLen = FMath::Lerp(Len(ENoclipEnemyAnim::Walk), Len(ENoclipEnemyAnim::Run), Gait);
	if (Moving > 0.f)
	{
		Phase = FMath::Frac(Phase + DeltaSeconds * Rate / CycleLen);
	}
	IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, Len(ENoclipEnemyAnim::Idle));

	const float LocoW = 1.f - ActionWeight;
	Push(ENoclipEnemyAnim::Idle, IdleTime, LocoW * (1.f - Moving), false, true);
	Push(ENoclipEnemyAnim::Walk, Phase, LocoW * Moving * (1.f - Gait), true, true);
	Push(ENoclipEnemyAnim::Run, Phase, LocoW * Moving * Gait, true, true);
	if (bHasAction && ActionWeight > 0.f)
	{
		Push(ActionAnim, FMath::Min(ActionTime, ActionLen), ActionWeight, false, false);
	}
}
