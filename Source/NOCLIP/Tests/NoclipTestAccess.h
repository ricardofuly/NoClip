// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipToolTypes.h"
#include "World/NoclipLevelGenerator.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipPowerGenerator.h"
#include "World/NoclipWaitingRoom.h"

/**
 * Acesso dos testes (NOCLIP.Unit.* e noclip.SelfTest) ao estado interno das classes do jogo.
 * As classes declaram "friend struct FNoclipTestAccess"; nada aqui e usado pelo jogo em si.
 */
struct FNoclipTestAccess
{
	// ---------- Agarrar / carregar em equipe ----------
	/** Velocidade calculada (sem o "1 quando nao segura nada" do GetCarrySpeedScale). */
	static float RawCarrySpeed(const UNoclipGrabComponent* Grab) { return Grab->CarrySpeedScale; }
	static void SetRawCarrySpeed(UNoclipGrabComponent* Grab, float Value) { Grab->CarrySpeedScale = Value; }
	static bool IsLead(const UNoclipGrabComponent* Grab) { return Grab->IsLead(); }
	static void GrabFromHit(UNoclipGrabComponent* Grab, const FHitResult& Hit) { Grab->ServerGrabFromHit(Hit); }
	static void ServerThrow(UNoclipGrabComponent* Grab, float Charge) { Grab->Server_Throw_Implementation(Charge); }

	/** Mesma conta do UNoclipGrabComponent::ApplyCarryShare, escrita de novo (se alguem mudar a regra, os testes avisam). */
	static float ExpectedCarrySpeed(const UNoclipGrabComponent* Grab, int32 Carriers, int32 Required, float MassKg)
	{
		const int32 Count = FMath::Max(Carriers, 1);
		if (Required > 1 && Count < Required)
		{
			return Grab->DragSpeedScale;
		}
		const float Share = MassKg / Count;
		float Speed = 1.f - FMath::Clamp(Share / FMath::Max(Grab->MaxGrabMassKg, 1.f), 0.f, 1.f) * Grab->MaxCarryPenalty;
		if (Share >= NoclipTools::TwoHandMassKg || Required > 1)
		{
			Speed = FMath::Min(Speed * Grab->TwoHandedSpeedFactor, 0.72f);
		}
		return Speed;
	}

	// ---------- Personagem: QTE do gerador ----------
	/**
	 * Um toque no QTE pelo caminho real do dono (TryGeneratorQTE -> Server_GeneratorAttempt).
	 * Com o QTE ja aberto, poe o ponteiro dentro (bHit) ou fora da faixa verde antes do toque.
	 */
	static bool PressGeneratorQTE(ANoclipCharacter* Char, bool bHit)
	{
		if (Char->bQTEActive)
		{
			Char->QTENeedle = bHit ? Char->QTEZoneCenter : (Char->QTEZoneCenter > 0.5f ? 0.f : 1.f);
		}
		return Char->TryGeneratorQTE();
	}

	// ---------- Achados ----------
	static int32 CatalogIndex(const ANoclipLootActor* Loot) { return Loot->CatalogIndex; }

	// ---------- Gerador de nivel ----------
	static const TArray<TObjectPtr<AActor>>& SpawnedLoot(const ANoclipLevelGenerator* Gen) { return Gen->SpawnedLoot; }

	// ---------- Gerador de energia ----------
	static float HitGain(const ANoclipPowerGenerator* Power) { return Power->HitGain; }
	static float MissLoss(const ANoclipPowerGenerator* Power) { return Power->MissLoss; }

	// ---------- Sala de espera ----------
	static const TArray<FVector>& RoutePoints(const ANoclipWaitingRoom* Room, int32 Slot) { return Room->GetRoute(Slot).Points; }
	static float RouteLength(const ANoclipWaitingRoom* Room, int32 Slot) { return Room->GetRoute(Slot).Length; }
	static float RouteDoorDistance(const ANoclipWaitingRoom* Room, int32 Slot) { return Room->GetRoute(Slot).DoorDistance; }
	static int32 RouteRow(const ANoclipWaitingRoom* Room, int32 Slot) { return Room->GetRoute(Slot).Row; }
	static FVector IdleSpot(const ANoclipWaitingRoom* Room, int32 Slot) { return Room->GetIdleSpot(Slot); }
	static void ComputeSchedule(const ANoclipWaitingRoom* Room, uint8 Mask, float OutStart[6]) { Room->ComputeSchedule(Mask, OutStart); }
	static float WalkDuration(const ANoclipWaitingRoom* Room, float Length) { return Room->GetWalkDuration(Length); }
	static float TimeAtDistance(const ANoclipWaitingRoom* Room, float Distance, float Length) { return Room->GetTimeAtDistance(Distance, Length); }
};
