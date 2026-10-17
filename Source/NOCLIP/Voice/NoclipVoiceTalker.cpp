// Copyright Fuly Games. All Rights Reserved.

#include "Voice/NoclipVoiceTalker.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipVoiceTalker)

namespace
{
	/** Faixa de telefone. */
	constexpr float PhoneLowPassHz = 3400.f;
	constexpr float PhoneHighPassHz = 320.f;
}

UNoclipVoiceTalker::UNoclipVoiceTalker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

FSoundAttenuationSettings UNoclipVoiceTalker::MakeProximitySettings()
{
	FSoundAttenuationSettings S;
	// Ate 2,5 m na voz cheia; some por volta de 20 m (GDD: voz por proximidade ~20 m).
	S.bAttenuate = true;
	S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	S.AttenuationShape = EAttenuationShape::Sphere;
	S.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
	S.FalloffDistance = 1800.f;
	S.dBAttenuationAtMax = -60.f;

	// Posicao: panning do motor; colado no colega vira quase mono (sem "pular" de um ouvido para o outro).
	S.bSpatialize = true;
	S.SpatializationAlgorithm = SPATIALIZATION_Default;
	S.NonSpatializedRadiusStart = 140.f;
	S.NonSpatializedRadiusEnd = 50.f;
	S.StereoSpread = 0.f;

	// Ar: de longe perde os agudos.
	S.bAttenuateWithLPF = true;
	S.LPFRadiusMin = 400.f;
	S.LPFRadiusMax = 2050.f;
	S.LPFFrequencyAtMin = 20000.f;
	S.LPFFrequencyAtMax = 2600.f;
	S.HPFFrequencyAtMin = 0.f;
	S.HPFFrequencyAtMax = 0.f;

	// Parede no meio: abafa e baixa.
	S.bEnableOcclusion = true;
	S.OcclusionTraceChannel = ECC_Visibility;
	S.OcclusionLowPassFilterFrequency = 1100.f;
	S.OcclusionVolumeAttenuation = 0.4f;
	S.OcclusionInterpolationTime = 0.25f;
	S.bUseComplexCollisionForOcclusion = false;

	// Eco do corredor (vai para o reverb global das Backrooms).
	S.bEnableReverbSend = true;
	S.ReverbWetLevelMin = 0.12f;
	S.ReverbWetLevelMax = 0.55f;
	S.ReverbDistanceMin = 200.f;
	S.ReverbDistanceMax = 2000.f;

	S.bEnableListenerFocus = false;
	S.bEnablePriorityAttenuation = false;
	return S;
}

FSoundAttenuationSettings UNoclipVoiceTalker::MakeFlatSettings()
{
	FSoundAttenuationSettings S;
	S.bAttenuate = false;
	S.bSpatialize = false;
	S.bAttenuateWithLPF = false;
	S.bEnableOcclusion = false;
	S.bEnableReverbSend = false;
	S.bEnableListenerFocus = false;
	S.bEnablePriorityAttenuation = false;
	return S;
}

void UNoclipVoiceTalker::OnTalkingBegin(UAudioComponent* InAudioComponent)
{
	Super::OnTalkingBegin(InAudioComponent);
	AudioComponent = InAudioComponent;
	AppliedRoute = ENoclipVoiceRoute::Silent;
	AppliedVolume = -1.f;
	ApplyToComponent(true);
}

void UNoclipVoiceTalker::OnTalkingEnd()
{
	Super::OnTalkingEnd();
	AudioComponent.Reset();
}

void UNoclipVoiceTalker::UpdateRoute(ENoclipVoiceRoute InRoute, USceneComponent* AttachTo, FName Bone, float Volume, float InPhoneQuality, float DeltaSeconds)
{
	Route = InRoute;
	AttachParent = AttachTo;
	AttachBone = Bone;
	PhoneQuality = FMath::Clamp(InPhoneQuality, 0.f, 1.f);

	// O motor aplica estes na proxima frase; o componente atual e ajustado aqui embaixo.
	Settings.ComponentToAttachTo = (Route == ENoclipVoiceRoute::Proximity) ? AttachTo : nullptr;

	// Celular com sinal fraco: cortes curtos e aleatorios (o "picotando" do sinal ruim).
	float Gate = 1.f;
	if (Route == ENoclipVoiceRoute::Phone)
	{
		DropTimer -= DeltaSeconds;
		if (DropTimer <= 0.f)
		{
			const float DropChance = FMath::Lerp(0.38f, 0.f, PhoneQuality);
			bDropped = FMath::FRand() < DropChance;
			DropTimer = bDropped ? FMath::FRandRange(0.06f, 0.22f) : FMath::FRandRange(0.15f, 0.6f);
		}
		Gate = bDropped ? 0.f : 1.f;
	}
	else
	{
		bDropped = false;
	}

	TargetVolume = (Route == ENoclipVoiceRoute::Silent) ? 0.f : Volume * Gate * (Route == ENoclipVoiceRoute::Phone ? 0.9f : 1.f);

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bForce = Now >= NextReapplyTime; // o motor pode reiniciar o som: reafirma de vez em quando
	if (bForce)
	{
		NextReapplyTime = Now + 1.0;
	}
	ApplyToComponent(bForce);
}

void UNoclipVoiceTalker::ApplyToComponent(bool bForce)
{
	UAudioComponent* AC = AudioComponent.Get();
	if (!AC)
	{
		return;
	}

	const bool bRouteChanged = AppliedRoute != Route;
	if (bRouteChanged || bForce)
	{
		AppliedRoute = Route;
		switch (Route)
		{
		case ENoclipVoiceRoute::Proximity:
		{
			if (USceneComponent* Parent = AttachParent.Get())
			{
				if (AC->GetAttachParent() != Parent || AC->GetAttachSocketName() != AttachBone)
				{
					AC->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachBone);
				}
			}
			AC->bAllowSpatialization = true;
			AC->AdjustAttenuation(MakeProximitySettings());
			AC->SetLowPassFilterEnabled(false);
			AC->SetHighPassFilterEnabled(false);
			break;
		}
		case ENoclipVoiceRoute::Phone:
		{
			AC->bAllowSpatialization = false;
			AC->AdjustAttenuation(MakeFlatSettings());
			const float Lpf = FMath::Lerp(2300.f, PhoneLowPassHz, PhoneQuality);
			AC->SetLowPassFilterEnabled(true);
			AC->SetLowPassFilterFrequency(Lpf);
			AC->SetHighPassFilterEnabled(true);
			AC->SetHighPassFilterFrequency(PhoneHighPassHz);
			break;
		}
		default:
			AC->bAllowSpatialization = false;
			AC->AdjustAttenuation(MakeFlatSettings());
			AC->SetLowPassFilterEnabled(false);
			AC->SetHighPassFilterEnabled(false);
			break;
		}
	}

	if (bRouteChanged || bForce || !FMath::IsNearlyEqual(AppliedVolume, TargetVolume, 0.01f))
	{
		AppliedVolume = TargetVolume;
		AC->SetVolumeMultiplier(FMath::Max(TargetVolume, 0.f));
	}
}
