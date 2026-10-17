// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Character/NoclipToolTypes.h"
#include "Game/NoclipShiftTypes.h"
#include "NoclipPlayerState.generated.h"

/**
 * Estado replicado de cada estagiario: pronto no saguao e as estatisticas do turno
 * (usadas nas "mencoes honrosas" do relatorio).
 */
UCLASS()
class NOCLIP_API ANoclipPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	bool IsReady() const { return bReady; }
	void SetReady(bool bInReady) { bReady = bInReady; }

	/**
	 * Ficou fora da Sala Manila quando o turno acabou (GDD: perde o que carregava e volta no proximo
	 * turno sem itens). Vale ate o fim do turno seguinte.
	 */
	bool IsLeftBehind() const { return bLeftBehind; }
	void SetLeftBehind(bool bIn) { bLeftBehind = bIn; }

	/** Morreu neste turno (assiste aos colegas ate o turno acabar). */
	bool IsDead() const { return bDead; }
	ENoclipDeathCause GetDeathCause() const { return DeathCause; }
	/** Servidor. */
	void SetDead(bool bInDead, ENoclipDeathCause Cause = ENoclipDeathCause::None);

	/** Estagiario escolhido na ficha de admissao (indice em NoclipCharacters). So visual. */
	int32 GetCharacterIndex() const { return CharacterIndex; }
	void SetCharacterIndex(int32 InIndex);

	// ---------- Cinto de ferramentas (posse da sessao; servidor escreve) ----------
	bool HasTool(ENoclipTool Tool) const;
	int32 GetMedkitCount() const { return MedkitCount; }
	/** Lanterna e celular entram no cinto ao chegar; kits vem do almoxarifado. */
	void GiveTool(ENoclipTool Tool, int32 Count = 1);
	/** Tira uma unidade (kit usado ou ferramenta solta no chao). */
	void TakeTool(ENoclipTool Tool);
	/** Ficou para tras no colapso: perde lanterna e kits (o celular e pessoal). */
	void StripShopTools();

	/** Zera as estatisticas no comeco do turno (servidor). */
	void ResetShiftStats();

	// ---------- Estatisticas do turno (servidor escreve) ----------
	/** Valor perdido em batidas de achados que este jogador segurou por ultimo. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Noclip|Stats")
	int32 DamageCaused = 0;

	/** Valor dos achados na Sala Manila no fim que este jogador levou por ultimo. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Noclip|Stats")
	int32 ValueCarried = 0;

	/** Segundos a mais de 15 m do colega mais proximo. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Noclip|Stats")
	float TimeAlone = 0.f;

	/** Maior arremesso do turno (cm, no plano) e o que foi arremessado. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Noclip|Stats")
	float LongestThrowCm = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Noclip|Stats")
	FText LongestThrowItem;

protected:
	UPROPERTY(Replicated)
	bool bReady = false;

	UPROPERTY(Replicated)
	bool bLeftBehind = false;

	UPROPERTY(Replicated)
	bool bDead = false;

	UPROPERTY(Replicated)
	ENoclipDeathCause DeathCause = ENoclipDeathCause::None;

	UPROPERTY(Replicated)
	int32 CharacterIndex = 0;

	/** Bits de ENoclipTool (NoclipTools::Bit). Comeca com lanterna e celular. */
	UPROPERTY(Replicated)
	uint8 ToolMask = (1u << 1) | (1u << 3);

	UPROPERTY(Replicated)
	uint8 MedkitCount = 0;
};
