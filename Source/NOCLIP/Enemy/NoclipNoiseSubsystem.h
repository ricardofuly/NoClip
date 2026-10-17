// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NoclipNoiseSubsystem.generated.h"

/** Um barulho no mundo. O raio e ate onde da para ouvir (cm). */
struct FNoclipNoise
{
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	double Time = 0.0;
	TWeakObjectPtr<AActor> Source;
	/** Feito por um estagiario (passo, pulo) — e nao por um achado batendo ou por outra entidade. */
	bool bFromPlayer = false;
};

/**
 * Barulhos recentes (so no servidor), que os caes escutam: passos, pousos, achados batendo e arremessados,
 * o grito do shambler. A voz por proximidade entra aqui quando o chat de voz existir.
 */
UCLASS()
class NOCLIP_API UNoclipNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Atalho: ignora no cliente e em mundos sem o subsystem. */
	static void Report(const UObject* WorldContext, const FVector& Location, float Radius, AActor* Source, bool bFromPlayer);

	void ReportNoise(const FVector& Location, float Radius, AActor* Source, bool bFromPlayer);

	/** Barulhos dos ultimos MaxAgeSeconds. */
	const TArray<FNoclipNoise>& GetRecentNoises(double MaxAgeSeconds = 1.0);

	/** Quanto tempo um barulho fica "no ar" (s). */
	static constexpr double NoiseLifetime = 1.5;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Prune(double Now, double MaxAge);

	TArray<FNoclipNoise> Noises;
};
