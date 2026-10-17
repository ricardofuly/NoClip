// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "NoclipMoverInputs.generated.h"

/**
 * Inputs extras do NOCLIP enviados junto com o FCharacterDefaultInputs a cada tick de simulacao.
 * Tudo que muda a velocidade precisa estar aqui (ou no sync state) para o rollback do Mover ficar consistente.
 */
USTRUCT(BlueprintType)
struct NOCLIP_API FNoclipMoverInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	/** Jogador segurando o botao de correr. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Noclip|Mover")
	bool bWantsSprint = false;

	/** Escala de velocidade imposta pelo peso carregado, quantizada em 0..255 (255 = sem penalidade). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Noclip|Mover")
	uint8 CarrySpeedScaleQuantized = 255;

	float GetCarrySpeedScale() const { return CarrySpeedScaleQuantized / 255.f; }
	void SetCarrySpeedScale(float Scale) { CarrySpeedScaleQuantized = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Scale, 0.f, 1.f) * 255.f)); }

	virtual FMoverDataStructBase* Clone() const override { return new FNoclipMoverInputs(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Super::NetSerialize(Ar, Map, bOutSuccess);
		Ar.SerializeBits(&bWantsSprint, 1);
		Ar << CarrySpeedScaleQuantized;
		bOutSuccess = true;
		return true;
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FNoclipMoverInputs& Auth = static_cast<const FNoclipMoverInputs&>(AuthorityState);
		return Auth.bWantsSprint != bWantsSprint || Auth.CarrySpeedScaleQuantized != CarrySpeedScaleQuantized;
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		const FNoclipMoverInputs& Source = static_cast<const FNoclipMoverInputs&>(Pct < 0.5f ? From : To);
		bWantsSprint = Source.bWantsSprint;
		CarrySpeedScaleQuantized = Source.CarrySpeedScaleQuantized;
	}

	virtual void Merge(const FMoverDataStructBase& From) override
	{
		const FNoclipMoverInputs& Typed = static_cast<const FNoclipMoverInputs&>(From);
		bWantsSprint |= Typed.bWantsSprint;
		CarrySpeedScaleQuantized = FMath::Min(CarrySpeedScaleQuantized, Typed.CarrySpeedScaleQuantized);
	}

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Super::ToString(Out);
		Out.Appendf("bWantsSprint: %i\n", bWantsSprint);
		Out.Appendf("CarrySpeedScale: %.2f\n", GetCarrySpeedScale());
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};

template<>
struct TStructOpsTypeTraits<FNoclipMoverInputs> : public TStructOpsTypeTraitsBase2<FNoclipMoverInputs>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
