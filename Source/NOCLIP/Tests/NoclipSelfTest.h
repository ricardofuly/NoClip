// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#if !UE_BUILD_SHIPPING

class UWorld;

/** Resultado de uma etapa do teste do jogo. */
struct FNoclipSelfTestStepResult
{
	FString Name;
	bool bPassed = true;
	bool bSkipped = false;
	float Seconds = 0.f;
	TArray<FString> Failures;
	TArray<FString> Notes;
};

/**
 * Teste do jogo de verdade, rodando no mapa do expediente (L_Level0_Test), no servidor/host:
 * turno, conteudo do nivel, noclip (paredes e pisos falsos, saidas de servico, noclip.GoDown), salas que mudam,
 * apagao + QTE do gerador (com mais de um estagiario), carregar em equipe (arrastar, levantar, dividir o peso,
 * troca de lider, sem arremesso), agarrar e arremessar pela mira, extracao, morte/espectador, equipe inteira
 * morta e volta ao saguao. As entidades ficam desligadas durante o teste (resultado sempre igual).
 *
 * Console (PIE ou jogo, no host):  noclip.SelfTest        noclip.SelfTest quit (fecha o jogo no fim)
 *                                  noclip.SelfTest stop
 * Automacao:                       NOCLIP.Game.FullLoop
 * Relatorio:                       na tela, no log (LogNoclipTest) e em Saved/Logs/NoclipSelfTest.txt
 */
class NOCLIP_API FNoclipSelfTest : public TSharedFromThis<FNoclipSelfTest>
{
public:
	/** Comeca no mundo dado (precisa ser o servidor/host). nullptr se ja houver um rodando. */
	static TSharedPtr<FNoclipSelfTest> Start(UWorld* World, bool bQuitWhenDone = false);
	static TSharedPtr<FNoclipSelfTest> GetRunning();

	bool IsFinished() const { return bFinished; }
	bool HasFailures() const;
	const TArray<FNoclipSelfTestStepResult>& GetResults() const { return Results; }
	FString GetReport() const;
	void Abort(const FString& Why);

	~FNoclipSelfTest();

	struct FImpl;

private:
	FNoclipSelfTest() = default;
	bool Tick(float DeltaTime);
	void Finish();

	TUniquePtr<FImpl> Impl;
	TArray<FNoclipSelfTestStepResult> Results;
	bool bFinished = false;
	bool bQuitWhenDone = false;
	FTSTicker::FDelegateHandle TickerHandle;
};

#endif // !UE_BUILD_SHIPPING
