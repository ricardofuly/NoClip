// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipDebugHUD.h"
#include "Game/NoclipGameState.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "World/NoclipLootActor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipDebugHUD)

static TAutoConsoleVariable<int32> CVarNoclipDebugHUD(
	TEXT("noclip.DebugHUD"), 0,
	TEXT("1 = desenha o HUD de texto antigo (canvas) por cima da UI de CommonUI."));

namespace
{
	FString FormatClock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60);
	}

	FString PhaseLabel(ENoclipShiftPhase Phase)
	{
		switch (Phase)
		{
		case ENoclipShiftPhase::Lobby:     return TEXT("SAGUAO");
		case ENoclipShiftPhase::Preparing: return TEXT("PREPARANDO");
		case ENoclipShiftPhase::Active:    return TEXT("EXPEDIENTE");
		case ENoclipShiftPhase::Collapse:  return TEXT("COLAPSO");
		case ENoclipShiftPhase::Ended:     return TEXT("FIM DO TURNO");
		}
		return TEXT("");
	}
}

void ANoclipDebugHUD::DrawLine(const FString& Text, float X, float& Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font || !Canvas)
	{
		return;
	}
	// Sombra para ler sobre o carpete claro.
	Canvas->SetDrawColor(FColor(0, 0, 0, 200));
	Canvas->DrawText(Font, Text, X + 2.f, Y + 2.f, Scale, Scale);
	Canvas->SetDrawColor(Color.ToFColor(true));
	Canvas->DrawText(Font, Text, X, Y, Scale, Scale);
	Y += 22.f * Scale;
}

void ANoclipDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (CVarNoclipDebugHUD.GetValueOnGameThread() == 0)
	{
		return;
	}

	const ANoclipGameState* GS = GetWorld() ? GetWorld()->GetGameState<ANoclipGameState>() : nullptr;
	if (!GS || !Canvas)
	{
		return;
	}

	const FLinearColor Amber(0.95f, 0.8f, 0.45f);
	const FLinearColor Dim(0.7f, 0.66f, 0.55f);
	const FLinearColor Good(0.55f, 0.9f, 0.5f);
	const FLinearColor Bad(1.f, 0.4f, 0.35f);

	const ENoclipShiftPhase Phase = GS->GetPhase();
	const float Margin = 24.f;
	float Y = Margin;

	// --- Canto superior esquerdo: turno e relogio ---
	DrawLine(FString::Printf(TEXT("TURNO %d  -  %s"), GS->GetShiftIndex(), *PhaseLabel(Phase)), Margin, Y, Phase == ENoclipShiftPhase::Collapse ? Bad : Amber, 1.2f);
	if (Phase != ENoclipShiftPhase::Lobby)
	{
		DrawLine(FormatClock(GS->GetPhaseTimeRemaining()), Margin, Y, Phase == ENoclipShiftPhase::Collapse ? Bad : Amber, 2.f);
	}
	else
	{
		DrawLine(TEXT("Host: digite NoclipStart no console para comecar"), Margin, Y, Dim);
	}

	// --- Canto superior direito: quota e caixa ---
	{
		float RY = Margin;
		const float RX = Canvas->ClipX - 340.f;
		const int32 InRoom = GS->GetValueInExtraction();
		const int32 Quota = GS->GetQuota();
		DrawLine(FString::Printf(TEXT("QUOTA   $%d"), Quota), RX, RY, Amber, 1.2f);
		DrawLine(FString::Printf(TEXT("NA SALA MANILA   $%d"), InRoom), RX, RY, InRoom >= Quota ? Good : Dim, 1.2f);
		DrawLine(FString::Printf(TEXT("CAIXA DA EQUIPE   $%d"), GS->GetMoney()), RX, RY, Dim);
	}

	// --- Barra do elevador ---
	if (GS->GetExtractionProgress() > 0.01f)
	{
		const float W = 360.f, H = 14.f;
		const float X = (Canvas->ClipX - W) * 0.5f;
		const float BY = Canvas->ClipY * 0.72f;
		Canvas->K2_DrawBox(FVector2D(X, BY), FVector2D(W, H), 2.f, Amber);
		FCanvasTileItem Fill(FVector2D(X + 2.f, BY + 2.f), FVector2D((W - 4.f) * GS->GetExtractionProgress(), H - 4.f), Amber);
		Canvas->DrawItem(Fill);
		float TY = BY - 26.f;
		DrawLine(TEXT("ELEVADOR SUBINDO - fiquem na sala"), X, TY, Amber);
	}

	// --- Objeto na mao ---
	if (const ANoclipCharacter* Char = Cast<ANoclipCharacter>(GetOwningPawn()))
	{
		if (const UNoclipGrabComponent* Grab = Char->GetGrabComponent())
		{
			if (const ANoclipLootActor* Loot = Cast<ANoclipLootActor>(Grab->GetHeldActor()))
			{
				float HY = Canvas->ClipY - 90.f;
				DrawLine(FString::Printf(TEXT("%s   $%d"), *Loot->GetDisplayName().ToString(), Loot->GetCurrentValue()), Margin, HY, Amber, 1.2f);
				DrawLine(TEXT("[E] soltar   [Botao esq.] segurar e soltar para arremessar"), Margin, HY, Dim);
			}
		}
	}

	// --- Mira ---
	Canvas->K2_DrawBox(FVector2D(Canvas->ClipX * 0.5f - 2.f, Canvas->ClipY * 0.5f - 2.f), FVector2D(4.f, 4.f), 2.f, Dim);

	// --- Relatorio ---
	if (Phase == ENoclipShiftPhase::Ended)
	{
		const FNoclipShiftResult& R = GS->GetLastResult();
		float CY = Canvas->ClipY * 0.3f;
		const float CX = Canvas->ClipX * 0.5f - 220.f;
		DrawLine(R.bQuotaMet ? TEXT("QUOTA BATIDA") : TEXT("VOCES FORAM DEMITIDOS"), CX, CY, R.bQuotaMet ? Good : Bad, 2.f);
		DrawLine(FString::Printf(TEXT("Extraido: $%d de $%d  (%d itens)"), R.Extracted, R.Quota, R.ItemsExtracted), CX, CY, Amber, 1.2f);
		DrawLine(FString::Printf(TEXT("Na sala no fim: %d de %d estagiarios"), R.PlayersExtracted, R.PlayersTotal), CX, CY, Dim, 1.2f);
		DrawLine(R.bQuotaMet ? FString::Printf(TEXT("Lucro para a loja: +$%d"), R.MoneyEarned) : FString(TEXT("A Baseline Recuperacoes agradece seus servicos.")), CX, CY, Dim, 1.2f);
	}
}
