// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipWaitingRoom.generated.h"

class ANoclipElevator;
class ANoclipWaitingAvatar;
class APlayerState;
class UMaterialInterface;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class ENoclipWaitingPhase : uint8
{
	/** Esperando a equipe entrar e marcar pronto. */
	Gathering,
	/** Portas abertas, estagiarios entrando no elevador. */
	Boarding,
	/** Portas fechadas, elevador descendo (viagem em instantes). */
	Departing
};

/**
 * Sala de espera do terreo: sala acarpetada com cadeiras, planta, relogio e o elevador B.
 * Uma camera de seguranca (SceneCapture) filma a sala; a UI mostra a imagem como no prototipo.
 * Os estagiarios aparecem como bonecos (ANoclipWaitingAvatar) que andam ate o elevador no embarque.
 *
 * Coordenadas locais: origem no centro do piso; o elevador fica na parede +X, de frente para a camera (parede -X).
 */
UCLASS()
class NOCLIP_API ANoclipWaitingRoom : public AActor
{
	GENERATED_BODY()
	friend struct FNoclipTestAccess; // testes de automacao (Tests/)

public:
	ANoclipWaitingRoom();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------- Servidor ----------
	ANoclipWaitingAvatar* AddAvatar(APlayerState* ForPlayer);
	void RemoveAvatar(APlayerState* ForPlayer);
	void SetPhase(ENoclipWaitingPhase NewPhase);

	/** Segundos (a partir do inicio do embarque) em que o ultimo boneco chega na cabine. */
	float GetBoardingFinishSeconds() const;

	// ---------- Todos ----------
	ENoclipWaitingPhase GetPhase() const { return Phase; }

	/** Segundos desde que a fase atual comecou (relogio do servidor). */
	float GetPhaseElapsed() const;

	ANoclipElevator* GetElevator() const { return Elevator; }

	/** Onde o boneco do lugar N esta agora (mundo) e para onde olha. */
	void EvaluateAvatar(int32 Slot, FVector& OutLocation, FRotator& OutRotation, bool& bOutWalking, bool& bOutInCabin, float* OutSpeed = nullptr) const;

	/** Quantas imagens a camera de seguranca ja tirou (as caixas de rastreio seguem a imagem, nao o mundo). */
	uint32 GetCaptureCount() const { return CaptureCount; }

	/** Imagem da camera de seguranca (so em maquinas com tela). */
	UTextureRenderTarget2D* GetCameraTarget() const { return CameraTarget; }

	/** Projeta um ponto do mundo na imagem da camera (0..1). false se estiver atras da camera. */
	bool ProjectToCamera(const FVector& World, FVector2D& OutUV) const;

	/** Quadros por segundo da camera de seguranca (CFTV barato). */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float CameraFPS = 30.f;

	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float CameraFOV = 72.f;

	/** Velocidade dos bonecos entrando no elevador (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float WalkSpeed = 240.f;

	/** Atraso entre um boneco e o proximo comecar a andar. */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float WalkStagger = 0.35f;

	/** Quando os bonecos comecam a andar (depois de as portas abrirem). */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float WalkStartDelay = 1.1f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UStaticMeshComponent* MakeBox(const TCHAR* Name, const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollide = false);

	FVector GetIdleSpot(int32 Slot) const;
	/** Rota fixa (spline) de cada lugar: do lugar de espera ate o lugar na cabine, sem cruzar as outras. */
	struct FBoardRoute
	{
		TArray<FVector> Points;   // spline amostrada (mundo), passo ~8 cm
		float Length = 0.f;
		float DoorDistance = 0.f; // distancia ao longo da rota ate a porta
		int32 Row = 0;            // 0 = fundo da cabine (entra primeiro), 1 = frente
	};
	const FBoardRoute& GetRoute(int32 Slot) const;
	/** Tempo de caminhada (com arranque e parada suaves) para uma rota deste tamanho. */
	float GetWalkDuration(float PathLength) const;
	/** Em quanto tempo (desde o arranque) chega a esta distancia da rota. */
	float GetTimeAtDistance(float Distance, float PathLength) const;
	/** Quem esta na sala (bit por lugar). */
	uint8 GetLiveSlotMask() const;
	/** Quando cada lugar comeca a andar: fundo primeiro, um passando a porta de cada vez. */
	void ComputeSchedule(uint8 Mask, float OutStart[6]) const;
	/** Relogio do embarque: anda liso a cada quadro e so se corrige devagar pelo relogio do servidor. */
	float GetBoardClock() const;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneCaptureComponent2D> SecurityCamera;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> LightA;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> LightB;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> ClockHand;

	UPROPERTY(Replicated)
	TObjectPtr<ANoclipElevator> Elevator;

	UPROPERTY(Replicated)
	ENoclipWaitingPhase Phase = ENoclipWaitingPhase::Gathering;

	UPROPERTY(Replicated)
	double PhaseServerTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> CameraTarget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANoclipWaitingAvatar>> Avatars;

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;

private:
	double GetServerNow() const;
	float CaptureAccumulator = 0.f;
	uint32 CaptureCount = 0;

	mutable float BoardClock = 0.f;
	mutable uint64 BoardClockFrame = MAX_uint64;
	mutable ENoclipWaitingPhase BoardClockPhase = ENoclipWaitingPhase::Gathering;
	mutable TArray<FBoardRoute> RouteCache;
	mutable float SlotStart[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	mutable bool bScheduleValid = false;
};
