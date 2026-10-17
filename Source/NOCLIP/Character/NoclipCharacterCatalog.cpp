// Copyright Fuly Games. All Rights Reserved.

#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipUserSettings.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/NoclipUIStyle.h"

#define LOCTEXT_NAMESPACE "NoclipCharacters"

namespace
{
	TArray<FNoclipCharacterDef> BuildDefs()
	{
		TArray<FNoclipCharacterDef> Defs;

		auto Facts = [](const FText& Age, const FText& Height, const FText& Weight, const FText& Blood, const FText& Shoe, const FText& Hired)
		{
			TArray<TPair<FText, FText>> F;
			F.Emplace(LOCTEXT("FAge", "Idade"), Age);
			F.Emplace(LOCTEXT("FHeight", "Altura"), Height);
			F.Emplace(LOCTEXT("FWeight", "Peso"), Weight);
			F.Emplace(LOCTEXT("FBlood", "Sangue"), Blood);
			F.Emplace(LOCTEXT("FShoe", "Calçado"), Shoe);
			F.Emplace(LOCTEXT("FHired", "Admissão"), Hired);
			return F;
		};

		// ---------- Davi (Casual Male) ----------
		{
			FNoclipCharacterDef& D = Defs.AddDefaulted_GetRef();
			D.Id = TEXT("davi");
			D.Short = LOCTEXT("DaviShort", "Davi “Dudu” Moreira");
			D.Plate = LOCTEXT("DaviPlate", "Davi M.");
			D.Name = LOCTEXT("DaviName", "Davi Moreira");
			D.Nick = LOCTEXT("DaviNick", "“Dudu”");
			D.Role = LOCTEXT("DaviRole", "Recuperação · ex-entregador de app");
			D.File = LOCTEXT("DaviFile", "Nº 0412");
			D.Stamp = LOCTEXT("Approved", "Aprovado");
			D.Facts = Facts(LOCTEXT("DaviAge", "26 anos"), LOCTEXT("DaviH", "1,78 m"), LOCTEXT("DaviW", "76 kg"), FText::FromString(TEXT("O+")), FText::FromString(TEXT("42")), FText::FromString(TEXT("03/2026")));
			D.Story = LOCTEXT("DaviStory", "Entregava marmita de bicicleta até dobrar a esquina errada e sair num corredor amarelo sem fim. Voltou nove horas depois com um ventilador de teto que ninguém pediu. A Baseline o contratou no mesmo dia. Ele ainda acha que o estágio é remunerado.");
			D.Traits = { LOCTEXT("DaviT1", "Conhece atalhos"), LOCTEXT("DaviT2", "Fala sozinho no rádio"), LOCTEXT("DaviT3", "Não tira o boné") };
			D.PhotoBody = NoclipUI::Hex(TEXT("4E6B3A"));
			D.PhotoSkin = NoclipUI::Hex(TEXT("C8A27E"));
			D.MeshPath = TEXT("/Game/NOCLIP/Characters/Male/male_casual.male_casual");
			D.HandsRigClassPath = TEXT("/Game/NOCLIP/Characters/Rigs/CR_NoclipHands_Male.CR_NoclipHands_Male_C");
			D.UpperArmR = TEXT("upper_arm_right"); D.UpperArmL = TEXT("upper_arm_left");
			D.HandR = TEXT("hand_right"); D.HandL = TEXT("hand_left"); D.Head = TEXT("head");
			const FString P = TEXT("/Game/NOCLIP/Characters/Male/Male_Locomotion_Pack/");
			auto A = [&P](const TCHAR* N) { return P + N + TEXT(".") + N; };
			D.AnimPaths[(int32)ENoclipCharacterAnim::Idle] = A(TEXT("idle"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Walk] = A(TEXT("walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Run] = A(TEXT("standard_run"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeLeftWalk] = A(TEXT("left_strafe_walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeRightWalk] = A(TEXT("right_strafe_walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeLeftRun] = A(TEXT("left_strafe"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeRightRun] = A(TEXT("right_strafe"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Jump] = A(TEXT("jump"));
		}

		// ---------- Regina (Female Secretary) ----------
		{
			FNoclipCharacterDef& D = Defs.AddDefaulted_GetRef();
			D.Id = TEXT("regina");
			D.Short = LOCTEXT("ReginaShort", "Regina Albuquerque");
			D.Plate = LOCTEXT("ReginaPlate", "Regina A.");
			D.Name = LOCTEXT("ReginaName", "Regina Albuquerque");
			D.Nick = LOCTEXT("ReginaNick", "“Dona Regina”");
			D.Role = LOCTEXT("ReginaRole", "Secretária executiva · realocada");
			D.File = LOCTEXT("ReginaFile", "Nº 0007");
			D.Stamp = LOCTEXT("Approved", "Aprovado");
			D.Facts = Facts(LOCTEXT("ReginaAge", "38 anos"), LOCTEXT("ReginaH", "1,66 m"), LOCTEXT("ReginaW", "61 kg"), FText::FromString(TEXT("A−")), FText::FromString(TEXT("36")), FText::FromString(TEXT("11/2011")));
			D.Story = LOCTEXT("ReginaStory", "Trabalhou quinze anos no 7º andar da Baseline até descobrir que o prédio só tem seis. Foi \"transferida para o campo\" por perguntar demais sobre o elevador B. Leva a agenda até nas Backrooms — e anota onde cada colega sumiu.");
			D.Traits = { LOCTEXT("ReginaT1", "Carimbo sempre à mão"), LOCTEXT("ReginaT2", "Nunca perde um prazo"), LOCTEXT("ReginaT3", "Odeia carpete úmido") };
			D.PhotoBody = NoclipUI::Hex(TEXT("9E2F2A"));
			D.PhotoSkin = NoclipUI::Hex(TEXT("D9B48F"));
			D.MeshPath = TEXT("/Game/NOCLIP/Characters/Female/female_secretary.female_secretary");
			D.HandsRigClassPath = TEXT("/Game/NOCLIP/Characters/Rigs/CR_NoclipHands_Female.CR_NoclipHands_Female_C");
			D.UpperArmR = TEXT("upper_arm_R"); D.UpperArmL = TEXT("upper_arm_L");
			D.HandR = TEXT("hand_R"); D.HandL = TEXT("hand_L"); D.Head = TEXT("head");
			const FString P = TEXT("/Game/NOCLIP/Characters/Female/Female_Locomotion_Pack/");
			auto A = [&P](const TCHAR* N) { return P + N + TEXT(".") + N; };
			D.AnimPaths[(int32)ENoclipCharacterAnim::Idle] = A(TEXT("idle"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Walk] = A(TEXT("walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Run] = A(TEXT("running"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeLeftWalk] = A(TEXT("left_strafe_walk"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeRightWalk] = A(TEXT("right_strafe_walk"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeLeftRun] = A(TEXT("left_strafe"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeRightRun] = A(TEXT("right_strafe"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Jump] = A(TEXT("jump"));
		}

		// ---------- Otavio (Male2) ----------
		{
			FNoclipCharacterDef& D = Defs.AddDefaulted_GetRef();
			D.Id = TEXT("otavio");
			D.Short = LOCTEXT("OtavioShort", "Otávio “Tavinho” Ramos");
			D.Plate = LOCTEXT("OtavioPlate", "Otávio R.");
			D.Name = LOCTEXT("OtavioName", "Otávio Ramos");
			D.Nick = LOCTEXT("OtavioNick", "“Tavinho”");
			D.Role = LOCTEXT("OtavioRole", "Suporte técnico · nível 0");
			D.File = LOCTEXT("OtavioFile", "Nº 1029");
			D.Stamp = LOCTEXT("Approved", "Aprovado");
			D.Facts = Facts(LOCTEXT("OtavioAge", "31 anos"), LOCTEXT("OtavioH", "1,80 m"), LOCTEXT("OtavioW", "82 kg"), FText::FromString(TEXT("B+")), FText::FromString(TEXT("43")), FText::FromString(TEXT("07/2025")));
			D.Story = LOCTEXT("OtavioStory", "Foi chamado para \"reiniciar o roteador do subsolo\" e desceu uma escada que não constava na planta. O chamado continua aberto. Ele jura que o zumbido das lâmpadas é um cooler com defeito e anda com um cabo de rede no bolso \"para qualquer emergência\".");
			D.Traits = { LOCTEXT("OtavioT1", "Já tentou desligar e ligar"), LOCTEXT("OtavioT2", "Mapeia tudo em planilha"), LOCTEXT("OtavioT3", "Não acredita em fantasma") };
			D.PhotoBody = NoclipUI::Hex(TEXT("3E5468"));
			D.PhotoSkin = NoclipUI::Hex(TEXT("B98B67"));
			D.MeshPath = TEXT("/Game/NOCLIP/Characters/Male2/MaleCharacter.MaleCharacter");
			D.HandsRigClassPath = TEXT("/Game/NOCLIP/Characters/Rigs/CR_NoclipHands_Male2.CR_NoclipHands_Male2_C");
			D.UpperArmR = TEXT("upper_arm_right"); D.UpperArmL = TEXT("upper_arm_left");
			D.HandR = TEXT("hand_right"); D.HandL = TEXT("hand_left"); D.Head = TEXT("head1");
			const FString P = TEXT("/Game/NOCLIP/Characters/Male2/");
			auto A = [&P](const TCHAR* N) { return P + N + TEXT(".") + N; };
			D.AnimPaths[(int32)ENoclipCharacterAnim::Idle] = A(TEXT("idle"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Walk] = A(TEXT("walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Run] = A(TEXT("standard_run"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeLeftWalk] = A(TEXT("left_strafe_walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeRightWalk] = A(TEXT("right_strafe_walking"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeLeftRun] = A(TEXT("left_strafe"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::StrafeRightRun] = A(TEXT("right_strafe"));
			D.AnimPaths[(int32)ENoclipCharacterAnim::Jump] = A(TEXT("jump"));
		}

		// ---------- Vagas em aberto ----------
		for (int32 i = 0; i < 2; ++i)
		{
			FNoclipCharacterDef& D = Defs.AddDefaulted_GetRef();
			D.Id = *FString::Printf(TEXT("locked%d"), i);
			D.bLocked = true;
			D.Short = LOCTEXT("LockedShort", "Vaga em aberto");
			D.Plate = LOCTEXT("LockedPlate", "Vaga em aberto");
			D.Name = LOCTEXT("LockedName", "Vaga em aberto");
			D.Nick = LOCTEXT("LockedNick", "Processo seletivo aberto");
			D.Role = LOCTEXT("LockedRole", "Chega numa próxima atualização");
			D.File = LOCTEXT("LockedFile", "Nº ————");
			D.Stamp = LOCTEXT("LockedStamp", "Em análise");
			const FText Dash = FText::FromString(TEXT("—"));
			D.Facts = Facts(Dash, Dash, Dash, Dash, Dash, Dash);
			D.Story = LOCTEXT("LockedStory", "A Baseline está reavaliando a política de contratação depois do incidente no Level 2. Enquanto isso, a vaga segue aberta e o crachá segue em branco.");
			D.Traits = { LOCTEXT("LockedT", "Em breve") };
			D.PhotoBody = NoclipUI::Hex(TEXT("CFC6A0"));
			D.PhotoSkin = NoclipUI::Hex(TEXT("CFC6A0"));
			// Silhueta: usa o corpo do Davi pintado de escuro.
			D.MeshPath = Defs[0].MeshPath;
			D.HandsRigClassPath = Defs[0].HandsRigClassPath;
			D.UpperArmR = Defs[0].UpperArmR; D.UpperArmL = Defs[0].UpperArmL;
			D.HandR = Defs[0].HandR; D.HandL = Defs[0].HandL; D.Head = Defs[0].Head;
			for (int32 a = 0; a < (int32)ENoclipCharacterAnim::Count; ++a)
			{
				D.AnimPaths[a] = Defs[0].AnimPaths[a];
			}
		}
		return Defs;
	}

	const TArray<FNoclipCharacterDef>& Defs()
	{
		static const TArray<FNoclipCharacterDef> GDefs = BuildDefs();
		return GDefs;
	}

	template <typename T>
	T* LoadCached(const FString& Path)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UObject>* Found = Cache.Find(Path); Found && Found->IsValid())
		{
			return Cast<T>(Found->Get());
		}
		T* Obj = LoadObject<T>(nullptr, *Path);
		if (Obj)
		{
			Obj->AddToRoot();
			Cache.Add(Path, Obj);
		}
		return Obj;
	}
}

namespace NoclipCharacters
{
	int32 NumSlots() { return Defs().Num(); }

	int32 NumPlayable()
	{
		int32 N = 0;
		for (const FNoclipCharacterDef& D : Defs())
		{
			N += D.bLocked ? 0 : 1;
		}
		return N;
	}

	const FNoclipCharacterDef& Get(int32 Slot)
	{
		return Defs()[FMath::Clamp(Slot, 0, Defs().Num() - 1)];
	}

	int32 ClampPlayable(int32 Index)
	{
		return (Index >= 0 && Index < NumPlayable()) ? Index : 0;
	}

	USkeletalMesh* LoadMesh(int32 Index)
	{
		return LoadCached<USkeletalMesh>(Get(Index).MeshPath);
	}

	UAnimSequence* LoadAnim(int32 Index, ENoclipCharacterAnim Anim)
	{
		return LoadCached<UAnimSequence>(Get(Index).AnimPaths[static_cast<int32>(Anim)]);
	}

	UClass* LoadHandsRigClass(int32 Index)
	{
		static TMap<FString, TWeakObjectPtr<UClass>> Cache;
		const FString& Path = Get(Index).HandsRigClassPath;
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UClass>* Found = Cache.Find(Path); Found && Found->IsValid())
		{
			return Found->Get();
		}
		UClass* Class = LoadObject<UClass>(nullptr, *Path);
		if (Class)
		{
			Class->AddToRoot();
			Cache.Add(Path, Class);
		}
		return Class;
	}

	UMaterialInterface* GetSilhouetteMaterial()
	{
		static TWeakObjectPtr<UMaterialInstanceDynamic> Cached;
		if (Cached.IsValid())
		{
			return Cached.Get();
		}
		UMaterialInterface* Base = LoadCached<UMaterialInterface>(TEXT("/Game/NOCLIP/Materials/MI_Loot_White.MI_Loot_White"));
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.035f, 0.032f, 0.026f, 1.f));
		MID->SetScalarParameterValue(TEXT("Roughness"), 1.f);
		MID->AddToRoot();
		Cached = MID;
		return MID;
	}

	int32 GetLocalChoice()
	{
		FNoclipUserSettings& S = FNoclipUserSettings::Get();
		if (S.CharacterIndex < 0 || S.CharacterIndex >= NumPlayable())
		{
			S.CharacterIndex = FMath::RandRange(0, FMath::Max(NumPlayable() - 1, 0));
			S.Save();
		}
		return S.CharacterIndex;
	}

	void SetLocalChoice(int32 Index)
	{
		FNoclipUserSettings& S = FNoclipUserSettings::Get();
		S.CharacterIndex = ClampPlayable(Index);
		S.Save();
	}
}

#undef LOCTEXT_NAMESPACE
