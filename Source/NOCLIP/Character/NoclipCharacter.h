// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "Game/NoclipShiftTypes.h"
#include "NoclipCharacter.generated.h"

class UCapsuleComponent;
class UCameraComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UNoclipMoverComponent;
class UNoclipGrabComponent;
class UNoclipToolComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Estagiario da Baseline Recuperacoes. Pawn em primeira pessoa movido pelo Mover (Network Prediction).
 * Gera o input do Mover (ProduceInput) e cuida do game feel local da camera.
 */
UCLASS(Blueprintable)
class NOCLIP_API ANoclipCharacter : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()
	friend struct FNoclipTestAccess; // testes de automacao (Tests/)

public:
	ANoclipCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual FRotator GetViewRotation() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Noclip")
	UNoclipMoverComponent* GetNoclipMover() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "Noclip")
	UNoclipGrabComponent* GetGrabComponent() const { return GrabComponent; }

	UFUNCTION(BlueprintPure, Category = "Noclip")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Noclip")
	USkeletalMeshComponent* GetCharacterMesh() const { return CharacterMesh; }

	UFUNCTION(BlueprintPure, Category = "Noclip")
	UNoclipToolComponent* GetToolComponent() const { return ToolComponent; }

	/** Indice do estagiario vestido agora (NoclipCharacters). */
	int32 GetAppliedCharacter() const { return FMath::Max(AppliedCharacter, 0); }

	/** Para onde o estagiario mira, em qualquer maquina (dono: controle; servidor: input do Mover; outros: pitch replicado). */
	FRotator GetAimRotation() const;

	// ---------- Vida (kit medico cura; queda forte machuca) ----------
	UFUNCTION(BlueprintPure, Category = "Noclip")
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	/** Servidor. Tambem devolve o folego todo ao dono. */
	void Heal(float Amount);
	/**
	 * Servidor. FromLocation (opcional): de onde veio o golpe, para o tranco da camera do dono.
	 * Durante o turno a vida pode zerar: o estagiario morre (no quadro seguinte) e vira espectador.
	 */
	void ApplyHurt(float Amount, FVector FromLocation = FVector::ZeroVector, ENoclipDeathCause Cause = ENoclipDeathCause::Other);

	/** Vida zerou e a morte esta sendo processada. */
	bool IsDying() const { return bPendingDeath; }

	// ---------- Noclip ----------
	/** Servidor: atravessou uma falha (teleporta pelo Mover, leva junto o achado da mao, avisa o dono). */
	void ServerNoclipTo(const FVector& Destination, uint8 GlitchKind);
	/** Dono: segundos desde o ultimo noclip e para onde foi (ENoclipGlitchKind). */
	float GetNoclipAge() const;
	uint8 GetLastNoclipKind() const { return LastNoclipKind; }

	// ---------- Gerador (apagao) ----------
	/** Dono: gerador ao alcance agora (com a energia caida). */
	class ANoclipPowerGenerator* GetGeneratorInReach() const;
	bool IsGeneratorQTEActive() const { return bQTEActive; }
	/** Ponteiro e faixa verde do QTE (0..1 na barra). */
	float GetQTENeedle() const { return QTENeedle; }
	float GetQTEZoneCenter() const { return QTEZoneCenter; }
	float GetQTEZoneHalf() const { return QTEZoneHalf; }
	/** Resultado do ultimo toque (acertou?) e ha quanto tempo. */
	bool GetQTELastHit() const { return bQTELastHit; }
	float GetQTEResultAge() const;

	/** Dono: segundos desde o ultimo dano (HUD pisca vermelho) e a forca dele (0..1). */
	float GetHurtAge() const;
	float GetHurtStrength() const { return HurtStrength; }

	/** Veste o modelo de um estagiario (indice em NoclipCharacters). Chamado sozinho pelo PlayerState. */
	void ApplyCharacter(int32 Index);

	/** Rotacao de visao valida no servidor (vem do ultimo input do Mover). */
	FRotator GetAuthoritativeViewRotation() const;

	/** Ponto de visao (camera) no mundo. */
	FVector GetEyeLocation() const;

	/** Folego 0..1 (so no dono). Correr gasta; parado/andando recupera. */
	UFUNCTION(BlueprintPure, Category = "Noclip")
	float GetStamina() const { return Stamina; }

	/** Vibracao curta no controle do dono (nao faz nada com teclado+mouse). Intensidade 0..1. */
	void PlayRumble(float Intensity, float Duration) const;

protected:
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// ---------- Componentes ----------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	/** Corpo provisorio antigo (cilindro). Fica escondido quando o modelo do estagiario carrega. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Modelo do estagiario escolhido (visto pelos outros; o dono so ve a sombra). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNoclipMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNoclipGrabComponent> GrabComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNoclipToolComponent> ToolComponent;

	// ---------- Vida ----------
	UPROPERTY(Replicated)
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.f;

	/** Queda acima disso machuca (cm/s) e quanto por 100 cm/s a mais. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float FallDamageSpeed = 1050.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float FallDamagePer100 = 9.f;

	/** Pitch da mira para quem nao e o dono (o yaw ja vem da rotacao do ator). */
	UPROPERTY(Replicated)
	int16 ReplicatedAimPitch = 0;

	UFUNCTION(Client, Reliable)
	void Client_RefillStamina();

	/** Dono: tranco na camera, vibracao e flash vermelho ao levar dano. */
	UFUNCTION(Client, Unreliable)
	void Client_HurtFeedback(float Amount, FVector_NetQuantize FromLocation);

	/** Dono: atravessou uma falha (chiado e imagem falhando). */
	UFUNCTION(Client, Reliable)
	void Client_NoclipFeedback(uint8 GlitchKind);

	/** Dono -> servidor: um toque no QTE do gerador. */
	UFUNCTION(Server, Reliable)
	void Server_GeneratorAttempt(class ANoclipPowerGenerator* Generator, bool bHit);

	// ---------- Input ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GrabAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ThrowAction;

	/** Olhar com o analogico direito (Axis2D). Separado do mouse porque e taxa (graus/s), nao delta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.01"))
	float LookSensitivity = 0.35f;

	/** Velocidade maxima de giro com o analogico (graus/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad")
	float GamepadYawRate = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad")
	float GamepadPitchRate = 170.f;

	/** Zona morta radial dos analogicos (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad", meta = (ClampMin = "0", ClampMax = "0.5"))
	float StickDeadZone = 0.15f;

	/** Curva de resposta do analogico de olhar (1 = linear, 2 = mais precisao no centro). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad", meta = (ClampMin = "1", ClampMax = "4"))
	float LookStickExponent = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad")
	bool bInvertGamepadY = false;

	/** Escala geral da vibracao (0 desliga). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float RumbleScale = 1.f;

	// ---------- Movimento base (aplicado no CommonLegacyMovementSettings em BeginPlay) ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float GroundAcceleration = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float GroundDeceleration = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float JumpUpwardsSpeed = 470.f;

	/** Segundos de corrida com o folego cheio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.5"))
	float SprintStaminaSeconds = 6.f;

	/** Segundos para recuperar o folego do zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.5"))
	float StaminaRecoverSeconds = 4.f;

	/** Janela em que um pulo apertado antes de tocar o chao ainda vale (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0"))
	float JumpBufferTime = 0.15f;

	// ---------- Game feel da camera (local) ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float BaseFOV = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float SprintFOVBonus = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float EyeHeight = 64.f;

	/** Amplitude vertical do head bob a velocidade de caminhada (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float HeadBobAmplitude = 1.6f;

	/** Ciclos de passo por segundo a velocidade de caminhada. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float HeadBobFrequency = 1.9f;

	/** Inclinacao da camera ao andar de lado (graus). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float StrafeRollDegrees = 1.5f;

	/** Quanto a camera afunda no pouso, por 1000 cm/s de queda (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float LandingDipPer1000 = 9.f;

	/** Rigidez da mola que devolve a camera apos o pouso. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float LandingSpringStiffness = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	float LandingSpringDamping = 18.f;

	/** Chamado localmente ao pousar (som, poeira). Intensidade 0..1. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Feel")
	void OnLandedFeel(float Intensity);

	/** Chamado localmente a cada passo (som de passo). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Feel")
	void OnFootstep(bool bSprinting);

private:
	void ApplyMovementSettings();
	void SyncCharacterFromPlayerState();
	void SetupToolInput(class UEnhancedInputComponent* Input);
	void AddToolMappingContext();
	void UpdateServerHealth(float DeltaSeconds);
	/** Passos dos colegas (quem nao e o dono): mesma cadencia do balanco da camera, pela velocidade replicada. */
	void UpdateRemoteFootsteps(float DeltaSeconds);
	void PlayFootstepSound(bool bSprinting, bool bLocal) const;
	float RemoteStepPhase = 0.f;
	bool bRemoteWasOnGround = true;
	float RemoteMinZSpeed = 0.f;

	// Cinto (acoes criadas em C++: 1 / 2 / 3, usar, soltar)
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToolSlotActions[3];
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToolUseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToolDropAction;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> ToolMappingContext;
	void OnToolSlot1(const FInputActionValue& Value);
	void OnToolSlot2(const FInputActionValue& Value);
	void OnToolSlot3(const FInputActionValue& Value);
	void OnToolUseStarted(const FInputActionValue& Value);
	void OnToolUseCompleted(const FInputActionValue& Value);
	void OnToolDrop(const FInputActionValue& Value);

	bool bServerWasOnGround = true;
	bool bPendingDeath = false;

	// Noclip / QTE (dono)
	double LastNoclipTime = -100.0;
	uint8 LastNoclipKind = 0;
	bool TryGeneratorQTE();
	void TickGeneratorQTE(float DeltaSeconds);
	void NewQTEZone();
	TWeakObjectPtr<class ANoclipPowerGenerator> CachedGenerator;
	double NextGeneratorLookup = 0.0;
	bool bQTEActive = false;
	float QTENeedle = 0.f;
	float QTEDir = 1.f;
	float QTEZoneCenter = 0.5f;
	float QTEZoneHalf = 0.1f;
	int32 QTEStreak = 0;
	bool bQTELastHit = false;
	double QTELastTime = -100.0;
	double QTENextPress = 0.0;
	ENoclipDeathCause PendingDeathCause = ENoclipDeathCause::None;
	FVector LastHitFrom = FVector::ZeroVector;
	double LastHurtTime = -100.0;
	float HurtStrength = 0.f;
	/** -1 = golpe pela esquerda, 1 = pela direita (inclina a camera para o outro lado). */
	float HurtSide = 0.f;
	float ServerMinZSpeed = 0.f;
	int32 AppliedCharacter = -1;
	void UpdateCameraFeel(float DeltaSeconds);

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);
	void OnLookStickTriggered(const FInputActionValue& Value);
	void OnLookStickCompleted(const FInputActionValue& Value);
	void ApplyStickLook(float DeltaSeconds);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);
	void OnSprintStarted(const FInputActionValue& Value);
	void OnSprintCompleted(const FInputActionValue& Value);
	void OnGrabStarted(const FInputActionValue& Value);
	void OnThrowStarted(const FInputActionValue& Value);
	void OnThrowCompleted(const FInputActionValue& Value);

	FVector CachedMoveInput = FVector::ZeroVector;
	FVector2D LookStickValue = FVector2D::ZeroVector;
	bool bJumpHeld = false;
	float JumpBufferRemaining = 0.f;
	bool bSprintHeld = false;
	float Stamina = 1.f;
	float StaminaRegenDelay = 0.f;
	bool bExhausted = false;
	void UpdateStamina(float DeltaSeconds);

	// Estado do game feel
	float CurrentFOV = 90.f;
	float BobPhase = 0.f;
	float CurrentRoll = 0.f;
	float LandingOffset = 0.f;
	float LandingVelocity = 0.f;
	bool bWasOnGround = true;
	float LastAirborneZSpeed = 0.f;
};
