// Copyright Fuly Games. All Rights Reserved.

#include "Enemy/NoclipNoiseSubsystem.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipNoiseSubsystem)

extern TAutoConsoleVariable<int32> CVarNoclipDebugEnemies;

bool UNoclipNoiseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNoclipNoiseSubsystem::Report(const UObject* WorldContext, const FVector& Location, float Radius, AActor* Source, bool bFromPlayer)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || Radius <= 1.f)
	{
		return;
	}
	if (UNoclipNoiseSubsystem* Noise = World->GetSubsystem<UNoclipNoiseSubsystem>())
	{
		Noise->ReportNoise(Location, Radius, Source, bFromPlayer);
	}
}

void UNoclipNoiseSubsystem::ReportNoise(const FVector& Location, float Radius, AActor* Source, bool bFromPlayer)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	Prune(Now, NoiseLifetime);

	// O mesmo autor repetindo (passos a cada 0,2 s): atualiza o ultimo em vez de empilhar.
	for (int32 i = Noises.Num() - 1; i >= 0; --i)
	{
		FNoclipNoise& N = Noises[i];
		if (Source && N.Source.Get() == Source && Now - N.Time < 0.25)
		{
			if (Radius >= N.Radius * 0.8f)
			{
				N.Location = Location;
				N.Radius = FMath::Max(N.Radius, Radius);
				N.Time = Now;
			}
			return;
		}
	}

	FNoclipNoise& N = Noises.AddDefaulted_GetRef();
	N.Location = Location;
	N.Radius = Radius;
	N.Time = Now;
	N.Source = Source;
	N.bFromPlayer = bFromPlayer;

	if (CVarNoclipDebugEnemies.GetValueOnGameThread() > 0)
	{
		DrawDebugCircle(World, Location + FVector(0, 0, 5), Radius, 32, bFromPlayer ? FColor::Yellow : FColor::Cyan, false, 0.6f, 0, 3.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
}

const TArray<FNoclipNoise>& UNoclipNoiseSubsystem::GetRecentNoises(double MaxAgeSeconds)
{
	if (UWorld* World = GetWorld())
	{
		Prune(World->GetTimeSeconds(), FMath::Max(MaxAgeSeconds, NoiseLifetime));
	}
	return Noises;
}

void UNoclipNoiseSubsystem::Prune(double Now, double MaxAge)
{
	Noises.RemoveAll([Now, MaxAge](const FNoclipNoise& N) { return Now - N.Time > MaxAge; });
}
