// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Engine/StreamableManager.h"
#include "NoclipAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class APlayerController;

/** O que a musica esta acompanhando agora. */
UENUM()
enum class ENoclipMusicState : uint8
{
	None,
	Menu,
	/** Sala de espera, aguardando a equipe. */
	WaitingRoom,
	/** Portas fechadas e elevador descendo (sala de espera -> nivel, contagem do turno). */
	Descending,
	/** Saguao entre turnos (cabine parada, loja). */
	Lobby,
	/** Turno: andando pelos corredores. */
	Explore,
	/** Uma entidade por perto procurando / ouviu algo. */
	Tension,
	/** Perseguicao. */
	Chase,
	/** Ultimo minuto: o nivel esta desligando. */
	Collapse,
	/** Dentro da Sala Manila. */
	SafeRoom,
	/** Morreu: assistindo aos colegas. */
	Dead,
	/** Relatorio: quota batida. */
	ReportGood,
	/** Relatorio: demitidos. */
	ReportFired
};

/** Piso embaixo de quem anda (escolhe o som do passo). */
enum class ENoclipSurface : uint8
{
	Carpet,
	Concrete,
	Metal,
	Wood,
	Stone,
	Stairs,
	Dirt,
	Grass,
	Gravel,
	Count
};

/** Lista de sons (para guardar arrays dentro de arrays como UPROPERTY). */
USTRUCT()
struct FNoclipSoundList
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Sounds;
};

/**
 * Uma camada de audio que toca faixas em sequencia, sempre com crossfade: na troca de faixa, na troca de
 * estado e (para a musica) atravessando a troca de mapa sem corte.
 */
USTRUCT()
struct FNoclipAudioLayer
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Current;

	/** Faixas saindo (fade out em andamento); destruidas quando param. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Fading;

	FSoftObjectPath CurrentPath;
	float Elapsed = 0.f;
	float Duration = 0.f;
	float Volume = 1.f;
	float CrossFade = 3.f;
	float LowPass = 0.f;
	/** Musica: sobrevive a troca de mapa e toca com o jogo pausado. Ambiente: e do mundo. */
	bool bPersistent = false;

	// Faixa pedida e ainda carregando (a antiga so sai quando a nova esta pronta: sem buraco).
	FSoftObjectPath PendingPath;
	float PendingVolume = 1.f;
	float PendingFadeIn = 3.f;
	float PendingFadeOut = 3.f;
	float PendingStartAt = 0.f;
	uint32 RequestSerial = 0;
};

/**
 * Som do NOCLIP (so em maquinas com tela):
 *  - Musica adaptativa por estado (menu, sala de espera, elevador, exploracao, tensao, perseguicao, colapso,
 *    Sala Manila, morte, relatorio), com crossfade em toda troca e continuando atraves das viagens de mapa.
 *    A exploracao volta de onde parou depois de uma perseguicao.
 *  - Ambiente do mundo: zumbido do ar/fluorescente no Level 0 e na sala de espera, vento no colapso.
 *  - Sons distantes aleatorios nos corredores, avisos (turno comecou, quota batida, colapso), folego, golpe,
 *    morte (sua e dos colegas) e passos das entidades.
 *  - Passos dos estagiarios por superficie (chamado pelo personagem).
 * Volume da musica nas opcoes (Musica); o resto segue o volume geral.
 */
UCLASS()
class NOCLIP_API UNoclipAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UNoclipAudioSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bReady; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	/** Um passo de Walker (estagiario). bLocal: o proprio jogador (som sem posicao, mais baixo). */
	void PlayFootstep(const AActor* Walker, bool bSprinting, bool bLocal);
	/** Pouso de um pulo / queda (Intensity 0..1). */
	void PlayLanding(const AActor* Walker, float Intensity, bool bLocal);

	ENoclipMusicState GetMusicState() const { return MusicState; }

	/** Divisoria mudou de porta (rangido grave no lugar). */
	void PlayWallShift(const FVector& Location);
	/** Toque no painel do gerador: estalo (acerto) ou faisca (erro). */
	void PlayGeneratorFeedback(const FVector& Location, bool bHit, float Progress);
	/** O jogador local atravessou uma falha: chiado de estatica. */
	void PlayNoclipSting();

private:
	enum class EBed : uint8 { None, WaitingHum, CabinHum, LevelHum, CollapseWind };

	APlayerController* GetLocalController(UWorld* World) const;
	ENoclipMusicState EvaluateMusicState(UWorld* World, double Now);
	EBed EvaluateBed(UWorld* World) const;
	void ApplyMusicState(ENoclipMusicState NewState);
	void ApplyBed(EBed NewBed);
	void TickEvents(UWorld* World, float DeltaTime, double Now);
	void TickEnemySteps(UWorld* World, float DeltaTime);
	void TickDistantSounds(UWorld* World, double Now);
	/** Chiado eletrico baixinho perto das paredes / pisos falsos (a pista para quem presta atencao). */
	void TickGlitchHiss(UWorld* World);

	// Camadas
	void PlayOnLayer(int32 LayerIndex, const FSoftObjectPath& Path, float Volume, float FadeIn, float FadeOutOld, float StartAt = 0.f);
	void OnTrackLoaded(int32 LayerIndex, uint32 Serial);
	void StartPending(int32 LayerIndex, USoundBase* Sound);
	void FadeOutLayer(int32 LayerIndex, float Seconds);
	void TickLayer(int32 LayerIndex, float DeltaTime);
	FNoclipAudioLayer& Layer(int32 LayerIndex) { return LayerIndex == 0 ? Music : Bed; }
	FSoftObjectPath PickFromPool(ENoclipMusicState State, const FSoftObjectPath& Avoid) const;
	float GetMusicScale() const;

	// Efeitos
	USoundBase* Sfx(const TCHAR* Name);
	USoundBase* PickSfx(const TArray<const TCHAR*>& Names, int32& LastIndex);
	void Play2D(USoundBase* Sound, float Volume, float Pitch = 1.f);
	void PlayAt(USoundBase* Sound, const FVector& Location, float Volume, USoundAttenuation* Attenuation, float Pitch = 1.f, float LowPass = 0.f);
	ENoclipSurface DetectSurface(const AActor* Walker, FVector& OutFeet) const;
	USoundBase* PickStep(ENoclipSurface Surface);
	USoundAttenuation* MakeAttenuation(float Inner, float Falloff, float LPFAtMax, bool bOcclusion, float ReverbMax);

	UPROPERTY(Transient)
	FNoclipAudioLayer Music;

	UPROPERTY(Transient)
	FNoclipAudioLayer Bed;

	/** Efeitos e passos carregados (pequenos; ficam na memoria). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> SfxCache;

	/** Passos por superficie (indice = ENoclipSurface). */
	UPROPERTY(Transient)
	TArray<FNoclipSoundList> Steps;

	/** Faixas de musica carregadas recentemente (as tocando tambem ficam presas no componente). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> TrackCache;
	TArray<FName> TrackCacheOrder;

	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FootstepAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> EnemyStepAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> DistantAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> EventAttenuation;

	FStreamableManager Streamable;
	bool bReady = false;

	// Estado da musica
	ENoclipMusicState MusicState = ENoclipMusicState::None;
	EBed BedState = EBed::None;
	double NextEvaluateTime = 0.0;
	double ChaseUntil = 0.0;
	double TensionUntil = 0.0;
	double SafeUntil = 0.0;
	float AppliedMusicScale = -1.f;
	/** Volta da exploracao: a faixa e o ponto onde parou. */
	FSoftObjectPath ExploreResumePath;
	float ExploreResumeAt = 0.f;
	TWeakObjectPtr<UWorld> LastWorld;

	// Eventos
	uint8 PrevPhase = 255;
	ENoclipMusicState LastEventMusicState = ENoclipMusicState::None;
	bool bQuotaChimed = false;
	bool bPrevDead = false;
	float PrevStamina = 1.f;
	float PrevHurtAge = 100.f;
	double NextJumpscareTime = 0.0;
	double NextChaseStingTime = 0.0;
	double NextBreathTime = 0.0;
	double NextDistantTime = 0.0;
	int32 LastDistant = -1;
	int32 LastDeathSfx = -1;
	int32 LastStep[static_cast<int32>(ENoclipSurface::Count)] = {};
	TSet<TWeakObjectPtr<AActor>> SeenCorpses;
	bool bPrevPowerOut = false;
	double NoclipStingStopTime = 0.0;
	double NextHissCheck = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> NoclipSting;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> HissComps;
	TArray<int32> HissGlitch;
	TMap<TWeakObjectPtr<AActor>, float> EnemyStepPhase;
};
