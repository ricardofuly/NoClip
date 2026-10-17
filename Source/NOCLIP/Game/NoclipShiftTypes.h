// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NoclipShiftTypes.generated.h"

/** Fases de um expediente (turno). */
UENUM(BlueprintType)
enum class ENoclipShiftPhase : uint8
{
	/** Entre turnos: grupo no saguao, loja aberta, esperando o host iniciar. */
	Lobby,
	/** Nivel novo gerado, jogadores reposicionados, contagem regressiva curta. */
	Preparing,
	/** Turno em andamento. */
	Active,
	/** Ultimo minuto: o nivel esta desligando. */
	Collapse,
	/** Turno encerrado: tela de relatorio. */
	Ended
};

/** Do que o estagiario morreu (texto da tela de morte). */
UENUM(BlueprintType)
enum class ENoclipDeathCause : uint8
{
	None,
	Hound,
	Shambler,
	Fall,
	Other
};

/** Resultado de um turno, replicado para todos montarem o relatorio. */
USTRUCT(BlueprintType)
struct FNoclipShiftResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 ShiftIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 Quota = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 Extracted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 ItemsExtracted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 MoneyEarned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 PlayersExtracted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 PlayersTotal = 0;

	/** Saiu pelo elevador antes do tempo acabar. */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	bool bExtractedEarly = false;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	bool bQuotaMet = false;

	/** Quota nao batida: o grupo foi demitido e a corrida recomeca. */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	bool bFired = false;

	/** Bonus fixo pago por bater a quota (ja incluso em MoneyEarned). */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 BonusPaid = 0;

	/** Quota do proximo turno (apos este resultado). */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 NextQuota = 0;

	/** Demissao: turnos completados nesta corrida e o caixa perdido. */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 ShiftsCompleted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 MoneyLost = 0;

	/** Colapso: quem nao chegou a Sala Manila ficou para tras e perdeu o achado que carregava. */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 PlayersLeftBehind = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 ValueLostCarried = 0;

	/** Quantos morreram no turno (voltam no proximo sem itens). */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	int32 PlayersDied = 0;

	/** A equipe inteira morreu: o turno acabou ali (conta so o que ja estava na Sala Manila). */
	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Shift")
	bool bTeamWiped = false;
};
