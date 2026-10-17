// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "NoclipWaitingRoomScreen.generated.h"

class ANoclipWaitingRoom;
class UBorder;
class UCanvasPanel;
class UImage;
class UNoclipButton;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * Sala de espera (WaitingRoom.dc.html): codigo de convite, pronto / descer, e a camera de seguranca
 * da sala do elevador com caixas de rastreio sobre cada estagiario. Todos prontos (ou o host manda) ->
 * portas abrem, a equipe entra, portas fecham e o elevador desce para o Level 0.
 */
UCLASS()
class NOCLIP_API UNoclipWaitingRoomScreen : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool HandleBack() override;

private:
	void RefreshInfo();
	void RefreshCrew();
	void RefreshCamera(float DeltaSeconds);
	void Leave(bool bReopenHostOptions);
	void InviteViaSteam();
	/** Host (codigo, descer, fechar) ou colega (resumo, pronto, convidar, sair). */
	void ApplyRole(bool bLeader);
	ANoclipWaitingRoom* GetRoom();

	struct FTrack
	{
		UBorder* Box = nullptr;
		UBorder* Tag = nullptr;
		UTextBlock* TagText = nullptr;
	};
	TArray<FTrack> Tracks;

	UNoclipButton* BackLink = nullptr;
	UTextBlock* RoomLabel = nullptr;
	UTextBlock* Title = nullptr;
	UTextBlock* Description = nullptr;
	UTextBlock* PrivacyText = nullptr;
	UTextBlock* CodeText = nullptr;
	UTextBlock* CopyMsg = nullptr;
	UNoclipButton* DepartButton = nullptr;
	UNoclipButton* ReadyButton = nullptr;
	UNoclipButton* CloseButton = nullptr;
	UNoclipButton* SteamButton = nullptr;
	class USizeBox* ReadyBig = nullptr;
	class USizeBox* ReadySmall = nullptr;
	UWidget* InviteCard = nullptr;
	UWidget* GuestCard = nullptr;
	UTextBlock* GuestPrivacyText = nullptr;
	UTextBlock* GuestHostText = nullptr;
	UTextBlock* GuestReadyText = nullptr;
	UTextBlock* GuestYouText = nullptr;
	UTextBlock* NavNote = nullptr;

	UImage* CameraImage = nullptr;
	UCanvasPanel* TrackCanvas = nullptr;
	UWidget* RecDot = nullptr;
	UTextBlock* StampText = nullptr;
	UTextBlock* DoorText = nullptr;

	UTextBlock* CountText = nullptr;
	UVerticalBox* CrewGrid = nullptr;

	/** "Seu estagiario: Davi Moreira · Trocar ->" (abre a ficha de admissao). */
	UNoclipButton* CharacterLink = nullptr;
	UTextBlock* CharacterNameText = nullptr;

	UTextBlock* KeyUp = nullptr;
	UTextBlock* KeyDown = nullptr;
	UTextBlock* KeyConfirm = nullptr;
	UTextBlock* KeyBack = nullptr;

	TWeakObjectPtr<ANoclipWaitingRoom> CachedRoom;
	FString CrewSignature;
	float RefreshAccumulator = 1.f;
	float BlinkTime = 0.f;
	bool bCopied = false;
	bool bBoundImage = false;
	bool bBusyShown = false;
	bool bRoleApplied = false;
	bool bRoleIsLeader = false;
};
