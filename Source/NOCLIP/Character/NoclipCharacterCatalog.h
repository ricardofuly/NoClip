// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;

/** Animacoes de locomocao de cada estagiario (todas in-place, sem root motion). */
enum class ENoclipCharacterAnim : uint8
{
	Idle,
	Walk,
	Run,
	StrafeLeftWalk,
	StrafeRightWalk,
	StrafeLeftRun,
	StrafeRightRun,
	Jump,
	Count
};

/** Um estagiario da ficha de admissao (CharacterSelect.dc.html). Cosmetico: todos tem os mesmos verbos. */
struct NOCLIP_API FNoclipCharacterDef
{
	FName Id;
	/** Vaga "Em breve": aparece no carrossel, mas nao pode ser vestida. */
	bool bLocked = false;

	FText Short;      // "Davi “Dudu” Moreira" (abaixo do carrossel)
	FText Plate;      // "Davi M." (placa do pedestal)
	FText Name;       // "Davi Moreira"
	FText Nick;       // "“Dudu”"
	FText Role;
	FText File;       // "Nº 0412"
	FText Stamp;      // "Aprovado"
	FText Story;
	TArray<TPair<FText, FText>> Facts; // Idade, Altura, Peso, Sangue, Calcado, Admissao
	TArray<FText> Traits;

	/** Cores da "foto 3x4" da ficha (roupa, pele). */
	FLinearColor PhotoBody = FLinearColor::White;
	FLinearColor PhotoSkin = FLinearColor::White;

	FString MeshPath;
	FString AnimPaths[static_cast<int32>(ENoclipCharacterAnim::Count)];

	/** Control Rig das maos (IK dos bracos + cabeca) e os ossos que ele mexe. */
	FString HandsRigClassPath;
	FName UpperArmR, UpperArmL, HandR, HandL, Head;

	/** Yaw do modelo para olhar para +X do ator (o FBX vem olhando para +Y). */
	float MeshYaw = -90.f;
	/** Velocidade (cm/s) em que o ciclo de andar / correr toca a 1x. */
	float WalkAnimSpeed = 160.f;
	float RunAnimSpeed = 420.f;
};

namespace NoclipCharacters
{
	/** Todas as vagas do carrossel (as jogaveis primeiro). */
	NOCLIP_API int32 NumSlots();
	/** So os que podem ser vestidos. */
	NOCLIP_API int32 NumPlayable();
	NOCLIP_API const FNoclipCharacterDef& Get(int32 Slot);
	/** Indice jogavel valido (fora do intervalo vira 0). */
	NOCLIP_API int32 ClampPlayable(int32 Index);

	/** Carregam sob demanda e guardam em cache (raiz do GC). */
	NOCLIP_API USkeletalMesh* LoadMesh(int32 Index);
	NOCLIP_API UAnimSequence* LoadAnim(int32 Index, ENoclipCharacterAnim Anim);
	/** Classe gerada do Control Rig das maos (CR_NoclipHands_*), ou nulo. */
	NOCLIP_API UClass* LoadHandsRigClass(int32 Index);

	/** Material escuro das vagas trancadas ("Em breve"). */
	NOCLIP_API UMaterialInterface* GetSilhouetteMaterial();

	/** Escolha salva desta maquina (GameUserSettings.ini). Na primeira vez sorteia um. */
	NOCLIP_API int32 GetLocalChoice();
	NOCLIP_API void SetLocalChoice(int32 Index);
}
