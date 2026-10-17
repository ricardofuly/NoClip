// Copyright Fuly Games. All Rights Reserved.

#include "Character/NoclipCharacterAnimInstance.h"
#include "Character/NoclipCharacter.h"
#include "Movement/NoclipMoverComponent.h"
#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipToolComponent.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/ScopeLock.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipCharacterAnimInstance)

/**
 * Proxy nativo: no worker de animacao, extrai cada camada e mistura por peso (sem grafo de nos).
 */
struct FNoclipAnimInstanceProxy : public FAnimInstanceProxy
{
	explicit FNoclipAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		if (const UNoclipCharacterAnimInstance* Inst = Cast<UNoclipCharacterAnimInstance>(InAnimInstance))
		{
			Layers.Reset();
			for (const FNoclipAnimLayer& L : Inst->PendingLayers)
			{
				Layers.Add(L);
			}
			RigInput = Inst->PendingRig;
			if (Rig.Get() != Inst->HandsRig)
			{
				Rig = Inst->HandsRig;
				BoneMapSerial = 0xFFFF; // remapeia
			}
			RigDelta = DeltaSeconds;
		}
	}

	/** Passa a pose da locomocao pelo Control Rig das maos (IK dos bracos + cabeca). */
	void RunHandsRig(FPoseContext& Output)
	{
		UControlRig* ControlRig = Rig.Get();
		if (!ControlRig || !RigInput.bActive)
		{
			return;
		}
		UE::TScopeLock LockRig(ControlRig->GetEvaluateMutex());
		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		if (!Hierarchy)
		{
			return;
		}

		// Osso do pose compacto <-> osso do rig (mesmo nome; o rig foi importado do mesmo esqueleto).
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		if (BoneMapSerial != Bones.GetSerialNumber())
		{
			BoneMapSerial = Bones.GetSerialNumber();
			BoneMap.Reset();
			const FReferenceSkeleton& RefSkel = Bones.GetReferenceSkeleton();
			for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
			{
				const int32 MeshIndex = Bones.MakeMeshPoseIndex(BoneIndex).GetInt();
				const FName Name = RefSkel.IsValidIndex(MeshIndex) ? RefSkel.GetBoneName(MeshIndex) : NAME_None;
				const int32 RigIndex = Hierarchy->GetIndex(FRigElementKey(Name, ERigElementType::Bone));
				if (RigIndex != INDEX_NONE)
				{
					BoneMap.Emplace(BoneIndex.GetInt(), RigIndex);
				}
			}
			NullR = Hierarchy->GetIndex(FRigElementKey(TEXT("ik_hand_r"), ERigElementType::Null));
			NullL = Hierarchy->GetIndex(FRigElementKey(TEXT("ik_hand_l"), ERigElementType::Null));
			NullPoleR = Hierarchy->GetIndex(FRigElementKey(TEXT("pole_r"), ERigElementType::Null));
			NullPoleL = Hierarchy->GetIndex(FRigElementKey(TEXT("pole_l"), ERigElementType::Null));
			NullHead = Hierarchy->GetIndex(FRigElementKey(TEXT("head_offset"), ERigElementType::Null));
			ControlWR = Hierarchy->GetIndex(FRigElementKey(TEXT("w_hand_r"), ERigElementType::Control));
			ControlWL = Hierarchy->GetIndex(FRigElementKey(TEXT("w_hand_l"), ERigElementType::Control));
		}
		if (BoneMap.Num() == 0)
		{
			return;
		}

		for (const TPair<int32, int32>& Pair : BoneMap)
		{
			Hierarchy->SetLocalTransform(Pair.Value, Output.Pose[FCompactPoseBoneIndex(Pair.Key)], false);
		}
		auto SetNull = [Hierarchy](int32 Index, const FTransform& T)
		{
			if (Index != INDEX_NONE)
			{
				Hierarchy->SetGlobalTransform(Index, T, false);
			}
		};
		SetNull(NullR, FTransform(RigInput.TargetR));
		SetNull(NullL, FTransform(RigInput.TargetL));
		SetNull(NullPoleR, FTransform(RigInput.PoleR));
		SetNull(NullPoleL, FTransform(RigInput.PoleL));
		SetNull(NullHead, FTransform(RigInput.HeadOffset));
		if (ControlWR != INDEX_NONE)
		{
			Hierarchy->SetControlValue(ControlWR, FRigControlValue::Make<float>(RigInput.WeightR));
		}
		if (ControlWL != INDEX_NONE)
		{
			Hierarchy->SetControlValue(ControlWL, FRigControlValue::Make<float>(RigInput.WeightL));
		}

		ControlRig->SetDeltaTime(RigDelta);
		ControlRig->Evaluate_AnyThread();

		for (const TPair<int32, int32>& Pair : BoneMap)
		{
			Output.Pose[FCompactPoseBoneIndex(Pair.Key)] = Hierarchy->GetLocalTransform(Pair.Value);
		}
		Output.Pose.NormalizeRotations();
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		float Accumulated = 0.f;
		for (const FNoclipAnimLayer& L : Layers)
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
			// Media ponderada incremental: cada camada nova entra com peso / (soma ate aqui + peso).
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
		RunHandsRig(Output);
		return true;
	}

	TArray<FNoclipAnimLayer, TInlineAllocator<8>> Layers;
	FNoclipHandsRigInput RigInput;
	TWeakObjectPtr<UControlRig> Rig;
	float RigDelta = 0.f;
	uint16 BoneMapSerial = 0xFFFF;
	TArray<TPair<int32, int32>> BoneMap;
	int32 NullR = INDEX_NONE, NullL = INDEX_NONE, NullPoleR = INDEX_NONE, NullPoleL = INDEX_NONE, NullHead = INDEX_NONE;
	int32 ControlWR = INDEX_NONE, ControlWL = INDEX_NONE;
};

FAnimInstanceProxy* UNoclipCharacterAnimInstance::CreateAnimInstanceProxy()
{
	return new FNoclipAnimInstanceProxy(this);
}

void UNoclipCharacterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FNoclipAnimInstanceProxy*>(InProxy);
}

void UNoclipCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	bHasLastLocation = false;
}

void UNoclipCharacterAnimInstance::SetCharacter(int32 Index)
{
	if (Index == CharacterIndex && Anims.Num() == static_cast<int32>(ENoclipCharacterAnim::Count))
	{
		return;
	}
	CharacterIndex = Index;
	Anims.Reset();
	for (int32 a = 0; a < static_cast<int32>(ENoclipCharacterAnim::Count); ++a)
	{
		Anims.Add(NoclipCharacters::LoadAnim(Index, static_cast<ENoclipCharacterAnim>(a)));
	}
	CreateHandsRig();
}

void UNoclipCharacterAnimInstance::CreateHandsRig()
{
	HandsRig = nullptr;
	// So o estagiario jogavel usa as maos (boneco da sala de espera e carrossel nao seguram nada).
	if (!Cast<ANoclipCharacter>(GetOwningActor()))
	{
		return;
	}
	UClass* RigClass = NoclipCharacters::LoadHandsRigClass(CharacterIndex);
	if (!RigClass || !RigClass->IsChildOf(UControlRig::StaticClass()))
	{
		return;
	}
	UControlRig* NewRig = NewObject<UControlRig>(this, RigClass);
	NewRig->Initialize(true);
	NewRig->RequestInit();
	HandsRig = NewRig;

	// Eixos da cabeca no espaco do proprio osso (o "nao" gira em volta do "cima" do personagem).
	if (URigHierarchy* Hierarchy = NewRig->GetHierarchy())
	{
		const FNoclipCharacterDef& Def = NoclipCharacters::Get(CharacterIndex);
		const FQuat HeadRot = Hierarchy->GetInitialGlobalTransform(FRigElementKey(Def.Head, ERigElementType::Bone)).GetRotation();
		HeadUpLocal = HeadRot.Inverse().RotateVector(FVector::UpVector).GetSafeNormal();
		HeadRightLocal = HeadRot.Inverse().RotateVector(-FVector::ForwardVector).GetSafeNormal(); // direita do personagem = -X do componente
	}
}

void UNoclipCharacterAnimInstance::UpdateHandsRig(float DeltaSeconds)
{
	PendingRig = FNoclipHandsRigInput();
	const ANoclipCharacter* Character = Cast<ANoclipCharacter>(GetOwningActor());
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!HandsRig || !Character || !Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		return;
	}
	const FNoclipCharacterDef& Def = NoclipCharacters::Get(CharacterIndex);
	const int32 UpperR = Mesh->GetBoneIndex(Def.UpperArmR);
	const int32 UpperL = Mesh->GetBoneIndex(Def.UpperArmL);
	const int32 HeadIdx = Mesh->GetBoneIndex(Def.Head);
	if (UpperR == INDEX_NONE || UpperL == INDEX_NONE || HeadIdx == INDEX_NONE)
	{
		return;
	}

	// Espaco do componente do mesh (yaw -90 no ator): frente = +Y, esquerda = +X, cima = +Z.
	const FVector Fwd(0.f, 1.f, 0.f);
	const FVector Left(1.f, 0.f, 0.f);
	const FVector Up(0.f, 0.f, 1.f);
	const FVector SR = Mesh->GetBoneTransform(UpperR, FTransform::Identity).GetLocation();
	const FVector SL = Mesh->GetBoneTransform(UpperL, FTransform::Identity).GetLocation();
	const FVector Head = Mesh->GetBoneTransform(HeadIdx, FTransform::Identity).GetLocation();
	const FVector Chest = (SR + SL) * 0.5f;
	const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(Character->GetAimRotation().Pitch), -65.f, 65.f);
	const FVector Aim = (Fwd * FMath::Cos(FMath::DegreesToRadians(Pitch)) + Up * FMath::Sin(FMath::DegreesToRadians(Pitch))).GetSafeNormal();

	const UNoclipToolComponent* Tools = Character->GetToolComponent();
	const UNoclipGrabComponent* Grab = Character->GetGrabComponent();
	const ENoclipTool Tool = Tools ? Tools->GetEquipped() : ENoclipTool::None;
	const ENoclipToolAction Action = Tools ? Tools->GetAction() : ENoclipToolAction::Hold;
	const bool bHolding = Grab && Grab->IsHolding();
	const bool bHeavy = Grab && Grab->IsHoldingTwoHanded();

	float WantR = 0.f, WantL = 0.f, HeadPitch = 0.f;
	FVector TR = SR + Fwd * 30.f - Up * 30.f;
	FVector TL = SL + Fwd * 30.f - Up * 30.f;
	PatTime += DeltaSeconds;

	if (bHeavy)
	{
		// Achado pesado: as duas maos na frente do peito, abertas na largura do objeto.
		WantR = WantL = 1.f;
		TR = Chest + Fwd * 40.f - Up * 14.f - Left * 20.f;
		TL = Chest + Fwd * 40.f - Up * 14.f + Left * 20.f;
	}
	else
	{
		switch (Tool)
		{
		case ENoclipTool::Flashlight:
			// Braco estendido apontando a lanterna para onde olha.
			WantR = 1.f;
			TR = SR + Aim * 38.f + Left * 7.f - Up * 4.f;
			break;
		case ENoclipTool::Phone:
			WantR = 1.f;
			if (Action == ENoclipToolAction::Looking)
			{
				TR = Head + Fwd * 27.f - Up * 15.f - Left * 5.f;
				HeadPitch = 12.f;
			}
			else
			{
				TR = SR + Fwd * 24.f - Up * 30.f + Left * 6.f;
			}
			break;
		case ENoclipTool::Medkit:
			WantR = WantL = 1.f;
			if (Action == ENoclipToolAction::Using)
			{
				// Curativo: kit no peito, maos batendo de leve e cabeca baixa.
				const float Pat = FMath::Abs(FMath::Sin(PatTime * 7.5f)) * 4.f;
				TR = Chest + Fwd * 22.f - Up * (26.f - Pat) - Left * 7.f;
				TL = Chest + Fwd * 22.f - Up * (24.f + Pat * 0.5f) + Left * 7.f;
				HeadPitch = 26.f;
			}
			else
			{
				TR = Chest + Fwd * 30.f - Up * 22.f - Left * 9.f;
				TL = Chest + Fwd * 30.f - Up * 22.f + Left * 9.f;
			}
			break;
		default:
			break;
		}
		// Achado leve: mao livre na frente (a esquerda se a direita estiver com ferramenta).
		if (bHolding)
		{
			if (Tool == ENoclipTool::None)
			{
				WantR = 1.f;
				TR = SR + Fwd * 40.f - Up * 10.f + Left * 6.f;
			}
			else if (Tool != ENoclipTool::Medkit)
			{
				WantL = 1.f;
				TL = SL + Fwd * 40.f - Up * 10.f - Left * 6.f;
			}
		}
	}

	// Suaviza: entra/sai do IK sem estalo e os alvos nao tremem com o idle.
	if (SmoothWeightR < 0.01f) { SmoothTargetR = TR; }
	if (SmoothWeightL < 0.01f) { SmoothTargetL = TL; }
	SmoothWeightR = FMath::FInterpTo(SmoothWeightR, WantR, DeltaSeconds, 9.f);
	SmoothWeightL = FMath::FInterpTo(SmoothWeightL, WantL, DeltaSeconds, 9.f);
	SmoothTargetR = FMath::VInterpTo(SmoothTargetR, TR, DeltaSeconds, 16.f);
	SmoothTargetL = FMath::VInterpTo(SmoothTargetL, TL, DeltaSeconds, 16.f);
	SmoothHeadPitch = FMath::FInterpTo(SmoothHeadPitch, HeadPitch, DeltaSeconds, 6.f);

	// "Nao" com a cabeca: mesma curva da camera do dono, mais amplo no corpo para ler de longe.
	float ShakeYaw = 0.f;
	if (Tools)
	{
		const float Age = Tools->GetHeadShakeAge();
		if (Age >= 0.f && Age < 0.6f)
		{
			ShakeYaw = FMath::Sin(Age * UE_TWO_PI * 3.3f) * 24.f * (1.f - Age / 0.6f);
		}
	}

	PendingRig.WeightR = SmoothWeightR;
	PendingRig.WeightL = SmoothWeightL;
	PendingRig.TargetR = SmoothTargetR;
	PendingRig.TargetL = SmoothTargetL;
	// Cotovelos para fora, para baixo e um pouco para tras.
	PendingRig.PoleR = SR - Left * 35.f - Up * 40.f - Fwd * 15.f;
	PendingRig.PoleL = SL + Left * 35.f - Up * 40.f - Fwd * 15.f;
	PendingRig.HeadOffset = FQuat(HeadUpLocal, FMath::DegreesToRadians(ShakeYaw)) * FQuat(HeadRightLocal, FMath::DegreesToRadians(SmoothHeadPitch));
	PendingRig.bActive = SmoothWeightR > 0.001f || SmoothWeightL > 0.001f || FMath::Abs(ShakeYaw) > 0.05f || FMath::Abs(SmoothHeadPitch) > 0.05f;
}

void UNoclipCharacterAnimInstance::SetVelocityOverride(bool bOverride, const FVector& LocalVelocity)
{
	bOverrideVelocity = bOverride;
	OverrideLocal = LocalVelocity;
}

void UNoclipCharacterAnimInstance::MeasureVelocity(float DeltaSeconds, FVector& OutLocal, bool& bOutGrounded)
{
	OutLocal = FVector::ZeroVector;
	bOutGrounded = true;

	const AActor* Owner = GetOwningActor();
	if (!Owner)
	{
		return;
	}
	if (bOverrideVelocity)
	{
		OutLocal = OverrideLocal;
		return;
	}

	FVector World = FVector::ZeroVector;
	if (const ANoclipCharacter* Character = Cast<ANoclipCharacter>(Owner))
	{
		if (const UNoclipMoverComponent* Mover = Character->GetNoclipMover())
		{
			World = Mover->GetVelocity();
			bOutGrounded = Mover->IsOnGround();
		}
	}
	else
	{
		const FVector Loc = Owner->GetActorLocation();
		if (bHasLastLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			World = (Loc - LastLocation) / DeltaSeconds;
			// Teleporte (troca de sala, respawn): ignora.
			if (World.SizeSquared() > FMath::Square(2000.f))
			{
				World = FVector::ZeroVector;
			}
		}
		LastLocation = Loc;
		bHasLastLocation = true;
	}
	OutLocal = Owner->GetActorQuat().UnrotateVector(World);
}

void UNoclipCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	PendingLayers.Reset();

	if (CharacterIndex < 0 || Anims.Num() != static_cast<int32>(ENoclipCharacterAnim::Count))
	{
		return;
	}
	const FNoclipCharacterDef& Def = NoclipCharacters::Get(CharacterIndex);
	auto Anim = [this](ENoclipCharacterAnim A) -> UAnimSequence* { return Anims[static_cast<int32>(A)]; };
	auto Len = [&Anim](ENoclipCharacterAnim A) -> float
	{
		const UAnimSequence* S = Anim(A);
		return S ? FMath::Max(S->GetPlayLength(), 0.05f) : 1.f;
	};

	FVector Local;
	bool bGrounded;
	MeasureVelocity(DeltaSeconds, Local, bGrounded);
	Local.Z = 0.f;
	SmoothedLocal = FMath::VInterpTo(SmoothedLocal, Local, DeltaSeconds, 10.f);
	Speed = SmoothedLocal.Size2D();

	// ---------- Ar ----------
	AirTime = bGrounded ? 0.f : AirTime + DeltaSeconds;
	const float AirTarget = (!bGrounded && AirTime > 0.08f) ? 1.f : 0.f;
	AirAlpha = FMath::FInterpConstantTo(AirAlpha, AirTarget, DeltaSeconds, AirTarget > AirAlpha ? 8.f : 5.f);
	const float GroundW = 1.f - AirAlpha;

	// ---------- Chao: parado / andar / correr, frente ou de lado ----------
	const float Moving = FMath::Clamp((Speed - 15.f) / 60.f, 0.f, 1.f);
	const float AbsF = FMath::Abs(SmoothedLocal.X);
	const float AbsR = FMath::Abs(SmoothedLocal.Y);
	const float Side = (AbsF + AbsR) > 1.f ? AbsR / (AbsF + AbsR) : 0.f;
	const float SideW = FMath::SmoothStep(0.25f, 0.75f, Side);
	const bool bBack = SmoothedLocal.X < -1.f;
	const bool bRight = SmoothedLocal.Y >= 0.f;

	const float Gait = FMath::Clamp((Speed - Def.WalkAnimSpeed) / FMath::Max(Def.RunAnimSpeed - Def.WalkAnimSpeed, 1.f), 0.f, 1.f);
	const float Nominal = FMath::Lerp(Def.WalkAnimSpeed, Def.RunAnimSpeed, Gait);
	const float Rate = FMath::Clamp(Speed / FMath::Max(Nominal, 1.f), 0.5f, 1.8f);
	const float CycleLen = FMath::Lerp(Len(ENoclipCharacterAnim::Walk), Len(ENoclipCharacterAnim::Run), Gait);
	if (Moving > 0.f)
	{
		Phase = FMath::Frac(Phase + DeltaSeconds * Rate / CycleLen);
	}
	IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, Len(ENoclipCharacterAnim::Idle));

	// Para tras: o ciclo de frente tocado ao contrario.
	const float FwdPhase = bBack ? 1.f - Phase : Phase;

	auto Push = [this, &Anim, &Len](ENoclipCharacterAnim A, float NormOrTime, float Weight, bool bNormalized, bool bLoop)
	{
		UAnimSequence* S = Anim(A);
		if (!S || Weight <= 0.001f)
		{
			return;
		}
		FNoclipAnimLayer& L = PendingLayers.AddDefaulted_GetRef();
		L.Sequence = S;
		L.Time = bNormalized ? NormOrTime * Len(A) : FMath::Min(NormOrTime, Len(A));
		L.Weight = Weight;
		L.bLoop = bLoop;
	};

	Push(ENoclipCharacterAnim::Idle, IdleTime, GroundW * (1.f - Moving), false, true);
	Push(ENoclipCharacterAnim::Walk, FwdPhase, GroundW * Moving * (1.f - SideW) * (1.f - Gait), true, true);
	Push(ENoclipCharacterAnim::Run, FwdPhase, GroundW * Moving * (1.f - SideW) * Gait, true, true);
	Push(bRight ? ENoclipCharacterAnim::StrafeRightWalk : ENoclipCharacterAnim::StrafeLeftWalk, Phase, GroundW * Moving * SideW * (1.f - Gait), true, true);
	Push(bRight ? ENoclipCharacterAnim::StrafeRightRun : ENoclipCharacterAnim::StrafeLeftRun, Phase, GroundW * Moving * SideW * Gait, true, true);

	// ---------- Pulo: comeca no impulso, segura a pose de voo ate pousar ----------
	if (AirAlpha > 0.f)
	{
		const float JumpLen = Len(ENoclipCharacterAnim::Jump);
		if (!bGrounded)
		{
			JumpPoseTime = FMath::Min(JumpLen * 0.3f + AirTime, JumpLen * 0.62f);
		}
		Push(ENoclipCharacterAnim::Jump, JumpPoseTime, AirAlpha, false, false);
	}

	UpdateHandsRig(DeltaSeconds);
}
