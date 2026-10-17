// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipLootCatalog.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "NoclipLoot"

namespace
{
	/** Sobe quando a tabela muda: estaticos de funcao nao reiniciam com o Live Coding. */
	constexpr int32 LootTableVersion = 2;

	struct FLootSpec
	{
		const TCHAR* Id;
		FText Name;
		FText Description;
		int32 MinValue;
		int32 MaxValue;
		float MassKg;
		float Fragility;
		float SpawnWeight;
		float Scale;
		/** Estagiarios para levantar (1 = sozinho). */
		int32 Carriers = 1;
		/** Bit 0 = Level 0, bit 1 = subsolo. */
		uint8 FloorMask = 1;
	};

	/**
	 * Root = pasta em Assets ("Office/extras", "Kitchen", "Hospital"...); Meshes = um ou mais modelos (variantes),
	 * cada um em Root/<modelo>/StaticMeshes/<modelo>.
	 */
	void AddThemeLoot(TArray<FNoclipLootDef>& Out, const TCHAR* Root, std::initializer_list<const TCHAR*> Meshes, const FLootSpec& S)
	{
		const float Weight = S.SpawnWeight / FMath::Max<int32>(static_cast<int32>(Meshes.size()), 1);
		for (const TCHAR* MeshName : Meshes)
		{
			FNoclipLootDef& D = Out.AddDefaulted_GetRef();
			D.Id = FName(*FString::Printf(TEXT("%s_%s"), S.Id, MeshName));
			D.Name = S.Name;
			D.Description = S.Description;
			D.MeshPath = FString::Printf(TEXT("/Game/NOCLIP/Assets/%s/%s/StaticMeshes/%s.%s"), Root, MeshName, MeshName, MeshName);
			D.MinValue = S.MinValue;
			D.MaxValue = FMath::Max(S.MaxValue, S.MinValue);
			D.MassKg = S.MassKg;
			D.Fragility = S.Fragility;
			D.Scale = S.Scale;
			D.SpawnWeight = Weight;
			D.Carriers = FMath::Clamp(S.Carriers, 1, 4);
			D.FloorMask = S.FloorMask;
		}
	}

	/** Achados do escritorio (Assets/Office/<categoria>). */
	void AddLoot(TArray<FNoclipLootDef>& Out, const TCHAR* Folder, std::initializer_list<const TCHAR*> Meshes, const FLootSpec& S)
	{
		AddThemeLoot(Out, *FString::Printf(TEXT("Office/%s"), Folder), Meshes, S);
	}

	/** Mesma ficha, outra quantidade de carregadores / andares. */
	FLootSpec Team(FLootSpec S, int32 Carriers, uint8 FloorMask = 1)
	{
		S.Carriers = Carriers;
		S.FloorMask = FloorMask;
		return S;
	}

	FLootSpec Floors(FLootSpec S, uint8 FloorMask)
	{
		S.FloorMask = FloorMask;
		return S;
	}

	TArray<FNoclipLootDef> BuildLootTable()
	{
		TArray<FNoclipLootDef> T;

		// ---------- Miudezas (uma mao) ----------
		AddLoot(T, TEXT("extras"), { TEXT("mug"), TEXT("mug_alt"), TEXT("mug_alt2"), TEXT("mug_alt3") },
			{ TEXT("mug"), LOCTEXT("Mug", "Caneca da firma"), LOCTEXT("MugDesc", "Ainda tem café de 2003 no fundo."), 8, 14, 0.4f, 0.9f, 1.4f, 1.f });
		AddLoot(T, TEXT("extras"), { TEXT("glass"), TEXT("glass2") },
			{ TEXT("glass"), LOCTEXT("Glass", "Copo de vidro"), LOCTEXT("GlassDesc", "Frágil. Qualquer tropeço vira caco."), 6, 10, 0.3f, 1.6f, 0.8f, 1.f });
		AddLoot(T, TEXT("computers"), { TEXT("computer_mouse") },
			{ TEXT("mouse"), LOCTEXT("Mouse", "Mouse de bolinha"), LOCTEXT("MouseDesc", "A bolinha sumiu. Ninguém sabe para onde."), 10, 16, 0.2f, 0.6f, 0.8f, 1.f });
		AddLoot(T, TEXT("computers"), { TEXT("computer_keyboard") },
			{ TEXT("keyboard"), LOCTEXT("Keyboard", "Teclado bege"), LOCTEXT("KeyboardDesc", "Falta a tecla Q. Sempre falta a tecla Q."), 18, 26, 1.2f, 0.5f, 1.f, 1.f });
		AddLoot(T, TEXT("extras"), { TEXT("desk_phone"), TEXT("desk_phone_with_cable") },
			{ TEXT("deskphone"), LOCTEXT("DeskPhone", "Telefone de mesa"), LOCTEXT("DeskPhoneDesc", "Às vezes ele toca. Não atenda."), 20, 30, 1.5f, 0.5f, 1.f, 1.f });
		AddLoot(T, TEXT("extras"), { TEXT("circular_desklamp"), TEXT("circular_desklamp_alt"), TEXT("square_desklamp"), TEXT("square_desklamp_alt") },
			{ TEXT("desklamp"), LOCTEXT("DeskLamp", "Luminária de mesa"), LOCTEXT("DeskLampDesc", "Ainda acende. Ninguém pergunta de onde vem a energia."), 22, 32, 2.f, 0.8f, 1.f, 1.f });
		AddLoot(T, TEXT("extras"), { TEXT("wall_clock") },
			{ TEXT("wallclock"), LOCTEXT("WallClock", "Relógio de parede"), LOCTEXT("WallClockDesc", "Marca 13h61. Sempre marcou."), 25, 35, 2.f, 0.9f, 0.7f, 1.f });
		AddLoot(T, TEXT("stationary"), { TEXT("paper_stack") },
			{ TEXT("papers"), LOCTEXT("Papers", "Pilha de relatórios"), LOCTEXT("PapersDesc", "Relatórios trimestrais de um trimestre que não existiu."), 6, 12, 2.f, 0.f, 0.8f, 1.f });
		AddLoot(T, TEXT("stationary"), { TEXT("paper_stack_large") },
			{ TEXT("ream"), LOCTEXT("Ream", "Resma de papel A4"), LOCTEXT("ReamDesc", "Quinhentas folhas de pura burocracia."), 10, 16, 4.f, 0.f, 0.6f, 1.f });
		AddLoot(T, TEXT("stationary"), { TEXT("elastic_ball") },
			{ TEXT("stressball"), LOCTEXT("StressBall", "Bolinha antiestresse"), LOCTEXT("StressBallDesc", "Apertada até o limite por alguém do RH."), 4, 8, 0.1f, 0.f, 0.5f, 1.f });
		AddLoot(T, TEXT("book_things"), { TEXT("books"), TEXT("books_alt"), TEXT("books_alt2") },
			{ TEXT("manuals"), LOCTEXT("Manuals", "Pilha de manuais"), LOCTEXT("ManualsDesc", "Manual do funcionário, 14ª edição revisada e ainda errada."), 14, 22, 6.f, 0.1f, 1.f, 1.f });
		AddLoot(T, TEXT("book_things"), { TEXT("file_binder"), TEXT("file_binder_alt"), TEXT("file_binder_alt2"), TEXT("file_binder_alt3"), TEXT("file_binder_alt4"), TEXT("file_binder_alt5") },
			{ TEXT("binder"), LOCTEXT("Binder", "Fichário de arquivo"), LOCTEXT("BinderDesc", "Contém a prova de que você foi contratado. Talvez."), 8, 14, 1.f, 0.1f, 1.f, 1.f });

		// ---------- Equipamento (uma mao, pesado) ----------
		AddLoot(T, TEXT("computers"), { TEXT("computer_monitor") },
			{ TEXT("monitor"), LOCTEXT("Monitor", "Monitor CRT 15\""), LOCTEXT("MonitorDesc", "Pesado, curvo e ainda morno."), 45, 60, 12.f, 0.6f, 1.f, 1.f });
		AddLoot(T, TEXT("computers"), { TEXT("horizontal_desktop") },
			{ TEXT("desktop"), LOCTEXT("Desktop", "Gabinete horizontal"), LOCTEXT("DesktopDesc", "Windows 98 instalado e um disquete preso no drive."), 50, 70, 8.f, 0.5f, 0.8f, 1.f });
		AddLoot(T, TEXT("computers"), { TEXT("computer_tower"), TEXT("computer_tower_alt") },
			{ TEXT("tower"), LOCTEXT("Tower", "Torre de PC"), LOCTEXT("TowerDesc", "Faz barulho de turbina ao ligar. Os cães adoram."), 55, 75, 11.f, 0.45f, 1.f, 1.f });
		AddLoot(T, TEXT("chairs"), { TEXT("conference_chair"), TEXT("conference_chair_black"), TEXT("conference_chair_blue"), TEXT("conference_chair_grey") },
			{ TEXT("confchair"), LOCTEXT("ConfChair", "Cadeira de reunião"), LOCTEXT("ConfChairDesc", "Ninguém nunca sentou nela até o fim de uma reunião."), 30, 42, 7.f, 0.2f, 0.9f, 1.f });
		AddLoot(T, TEXT("chairs"), { TEXT("office_chair_alt"), TEXT("office_chair_black"), TEXT("office_chair_black_alt"), TEXT("office_chair_blue"), TEXT("office_chair_blue_alt"), TEXT("office_chair_grey"), TEXT("office_chair_grey_alt"), TEXT("office_chair_orange") },
			{ TEXT("officechair"), LOCTEXT("OfficeChair", "Cadeira giratória"), LOCTEXT("OfficeChairDesc", "Gira, range e afunda sozinha."), 40, 55, 13.f, 0.25f, 1.f, 1.f });
		AddLoot(T, TEXT("chairs"), { TEXT("office_chair_fancy") },
			{ TEXT("bosschair"), LOCTEXT("BossChair", "Cadeira do diretor"), LOCTEXT("BossChairDesc", "Couro sintético e um pouco de poder de verdade."), 90, 120, 14.f, 0.25f, 0.35f, 1.f });
		AddLoot(T, TEXT("couches"), { TEXT("beanbag_blue"), TEXT("beanbag_green"), TEXT("beanbag_red") },
			{ TEXT("beanbag"), LOCTEXT("Beanbag", "Puff da sala de descanso"), LOCTEXT("BeanbagDesc", "Leve, macio e misteriosamente úmido."), 35, 50, 6.f, 0.f, 0.5f, 0.6f });

		// ---------- Duas maos (15 kg ou mais) ----------
		AddLoot(T, TEXT("extras"), { TEXT("water_cooler_bottle") },
			{ TEXT("waterjug"), LOCTEXT("WaterJug", "Galão de água"), LOCTEXT("WaterJugDesc", "Vinte litros. Não beba."), 18, 26, 20.f, 0.2f, 0.6f, 1.f });
		AddLoot(T, TEXT("computers"), { TEXT("TV") },
			{ TEXT("crt"), LOCTEXT("CrtTv", "TV de tubo 29\""), LOCTEXT("CrtTvDesc", "Já passou novela das oito de 1998 aqui."), 80, 100, 25.f, 0.5f, 0.6f, 1.f });
		AddLoot(T, TEXT("computers"), { TEXT("desktop_brick") },
			{ TEXT("server"), LOCTEXT("Server", "Servidor da empresa"), LOCTEXT("ServerDesc", "O e-mail da firma inteira mora aqui. Não derrube."), 120, 160, 30.f, 0.5f, 0.3f, 1.f });
		AddLoot(T, TEXT("extras"), { TEXT("water_cooler") },
			{ TEXT("cooler"), LOCTEXT("Cooler", "Bebedouro"), LOCTEXT("CoolerDesc", "Glub. Glub. Glub. Ponto de fofoca oficial."), 70, 90, 28.f, 0.4f, 0.4f, 1.f });
		AddLoot(T, TEXT("file_cabinets"), { TEXT("file_cabinet_smallest"), TEXT("file_cabinet_smallest_open"), TEXT("file_cabinet_smallest_with_shelfs") },
			{ TEXT("cabinet2"), LOCTEXT("Cabinet2", "Gaveteiro baixo"), LOCTEXT("Cabinet2Desc", "Gavetas trancadas. A chave ficou em outro nível."), 60, 80, 25.f, 0.15f, 0.45f, 1.f });
		AddLoot(T, TEXT("file_cabinets"), { TEXT("file_cabinet_smaller"), TEXT("file_cabinet_smaller_open"), TEXT("file_cabinet_smaller_with_shelfs") },
			{ TEXT("cabinet3"), LOCTEXT("Cabinet3", "Arquivo de aço (3 gavetas)"), LOCTEXT("Cabinet3Desc", "Fichas de funcionários que nunca voltaram."), 80, 100, 35.f, 0.15f, 0.35f, 1.f });
		AddLoot(T, TEXT("file_cabinets"), { TEXT("file_cabinet_small"), TEXT("file_cabinet_small_open"), TEXT("file_cabinet_small_with_shelfs") },
			{ TEXT("cabinet4"), LOCTEXT("Cabinet4", "Arquivo de aço (4 gavetas)"), LOCTEXT("Cabinet4Desc", "Pesa como a consciência de quem assinou as demissões."), 100, 130, 45.f, 0.15f, 0.25f, 1.f });
		AddLoot(T, TEXT("couches"), { TEXT("armchair_blue"), TEXT("armchair_green"), TEXT("armchair_pattern"), TEXT("armchair_red") },
			{ TEXT("armchair"), LOCTEXT("Armchair", "Poltrona da recepção"), LOCTEXT("ArmchairDesc", "Confortável demais para ser deste mundo."), 90, 120, 32.f, 0.15f, 0.35f, 1.f });
		AddLoot(T, TEXT("desks"), { TEXT("desk1"), TEXT("desk1_alt"), TEXT("desk1_weathered"), TEXT("desk2"), TEXT("desk2_alt"), TEXT("desk2_weathered") },
			Team({ TEXT("desk"), LOCTEXT("Desk", "Mesa de escritório"), LOCTEXT("DeskDesc", "Ainda tem chiclete grudado embaixo."), 150, 190, 60.f, 0.1f, 0.2f, 1.f }, 2));
		AddLoot(T, TEXT("couches"), { TEXT("couch_blue"), TEXT("couch_green"), TEXT("couch_pattern"), TEXT("couch_red") },
			Team({ TEXT("couch"), LOCTEXT("Couch", "Sofá de dois lugares"), LOCTEXT("CouchDesc", "Precisa de dois estagiários. Ou de um muito determinado."), 190, 240, 55.f, 0.1f, 0.2f, 1.f }, 2));
		AddLoot(T, TEXT("extras"), { TEXT("coffee_machine") },
			Team({ TEXT("coffee"), LOCTEXT("Coffee", "Máquina de café"), LOCTEXT("CoffeeDesc", "A única coisa que funcionava no escritório."), 240, 300, 80.f, 0.35f, 0.15f, 0.8f }, 2));

		// ---------- Copa (Assets/Kitchen): Level 0 e subsolo ----------
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("can_cola_cola"), TEXT("can_fantac"), TEXT("can_beer") },
			Floors({ TEXT("can"), LOCTEXT("Can", "Latinha esquecida"), LOCTEXT("CanDesc", "Gelada. Ninguém sabe como."), 3, 6, 0.35f, 0.1f, 1.2f, 1.f }, 3));
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("beer_bottle"), TEXT("soda_bottle") },
			Floors({ TEXT("bottle"), LOCTEXT("Bottle", "Garrafa da geladeira da firma"), LOCTEXT("BottleDesc", "Tem um nome escrito na fita crepe: \"NÃO MEXER\"."), 6, 11, 1.f, 1.2f, 1.f, 1.f }, 3));
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("Chipsy_Chunks_box"), TEXT("Choco_Chumps_box"), TEXT("Choco_Munch_box") },
			{ TEXT("cereal"), LOCTEXT("Cereal", "Caixa de cereal"), LOCTEXT("CerealDesc", "Validade: \"sim\"."), 5, 9, 0.5f, 0.f, 0.9f, 1.f });
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("pizza_box_semi_close_with_pizza"), TEXT("pizza_full") },
			{ TEXT("pizza"), LOCTEXT("Pizza", "Pizza da sexta-feira"), LOCTEXT("PizzaDesc", "Sobrou da festa de fim de ano de 1997. Ainda morna."), 12, 20, 1.2f, 0.f, 0.7f, 1.f });
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("pan") },
			Floors({ TEXT("pan"), LOCTEXT("Pan", "Frigideira de ferro"), LOCTEXT("PanDesc", "Arma oficial da copa. Não amassa."), 15, 22, 2.5f, 0.02f, 0.8f, 1.f }, 3));
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("knife_block") },
			{ TEXT("knives"), LOCTEXT("Knives", "Cepo de facas"), LOCTEXT("KnivesDesc", "Falta uma. Melhor não pensar em onde ela está."), 20, 28, 3.f, 0.2f, 0.6f, 1.f });
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("toaster") },
			{ TEXT("toaster"), LOCTEXT("Toaster", "Torradeira"), LOCTEXT("ToasterDesc", "Queima um lado e congela o outro."), 26, 38, 3.f, 0.45f, 0.8f, 1.f });
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("microwave") },
			Floors({ TEXT("microwave"), LOCTEXT("Microwave", "Micro-ondas da copa"), LOCTEXT("MicrowaveDesc", "Apita sozinho às 3h da manhã."), 60, 80, 14.f, 0.5f, 0.7f, 1.f }, 3));
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("chair") },
			{ TEXT("kitchenchair"), LOCTEXT("KitchenChair", "Cadeira da copa"), LOCTEXT("KitchenChairDesc", "Bamba de propósito. Ninguém fica muito tempo no café."), 22, 32, 6.f, 0.2f, 0.7f, 1.f });
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("table_01") },
			{ TEXT("kitchentable"), LOCTEXT("KitchenTable", "Mesa da copa"), LOCTEXT("KitchenTableDesc", "Quatro lugares e mil reuniões que podiam ter sido um e-mail."), 70, 95, 28.f, 0.15f, 0.35f, 1.f });
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("table_02") },
			Team({ TEXT("diningtable"), LOCTEXT("DiningTable", "Mesa comprida do refeitório"), LOCTEXT("DiningTableDesc", "Uma ponta para cada estagiário. Ninguém leva sozinho."), 170, 220, 55.f, 0.15f, 0.25f, 1.f }, 2));
		AddThemeLoot(T, TEXT("Kitchen"), { TEXT("fridge_close") },
			Team({ TEXT("fridge"), LOCTEXT("Fridge", "Geladeira da copa"), LOCTEXT("FridgeDesc", "Cheia de marmitas com nome de gente que não existe mais."), 300, 380, 95.f, 0.3f, 0.15f, 1.f }, 2, 3));

		// ---------- Ala hospitalar (Assets/Hospital): mais no subsolo ----------
		AddThemeLoot(T, TEXT("Hospital"), { TEXT("iv_stand") },
			Floors({ TEXT("ivstand"), LOCTEXT("IvStand", "Suporte de soro"), LOCTEXT("IvStandDesc", "A bolsa ainda pinga. Não é soro."), 32, 46, 6.f, 0.35f, 0.9f, 1.f }, 2));
		AddThemeLoot(T, TEXT("Hospital"), { TEXT("wheelchair") },
			Floors({ TEXT("wheelchair"), LOCTEXT("Wheelchair", "Cadeira de rodas"), LOCTEXT("WheelchairDesc", "Anda sozinha quando ninguém está olhando."), 90, 120, 18.f, 0.25f, 0.7f, 1.f }, 2));
		AddThemeLoot(T, TEXT("Hospital"), { TEXT("bedside_cabinet") },
			Floors({ TEXT("bedside"), LOCTEXT("Bedside", "Criado-mudo hospitalar"), LOCTEXT("BedsideDesc", "Gaveta com prontuários de pacientes sem rosto."), 60, 80, 22.f, 0.2f, 0.6f, 1.f }, 2));
		AddThemeLoot(T, TEXT("Hospital"), { TEXT("hospital_bed") },
			Team({ TEXT("hospitalbed"), LOCTEXT("HospitalBed", "Maca hospitalar"), LOCTEXT("HospitalBedDesc", "Três estagiários, uma maca e muitas perguntas."), 360, 450, 110.f, 0.15f, 0.35f, 1.f }, 3, 2));

		// ---------- Bunker (Assets/Bunker): subsolo ----------
		AddThemeLoot(T, TEXT("Bunker"), { TEXT("gas_masks") },
			Floors({ TEXT("gasmasks"), LOCTEXT("GasMasks", "Máscaras de gás"), LOCTEXT("GasMasksDesc", "Para o caso de o ar das Backrooms ficar pior."), 36, 50, 3.f, 0.25f, 1.f, 1.f }, 2));
		AddThemeLoot(T, TEXT("Bunker"), { TEXT("drum_water") },
			Floors({ TEXT("drum"), LOCTEXT("Drum", "Tambor de água potável"), LOCTEXT("DrumDesc", "Duzentos litros e um gosto metálico."), 80, 105, 40.f, 0.1f, 0.6f, 1.f }, 2));
		AddThemeLoot(T, TEXT("Bunker"), { TEXT("locker_bunker") },
			Team({ TEXT("locker"), LOCTEXT("Locker", "Armário de aço do bunker"), LOCTEXT("LockerDesc", "Trancado por dentro."), 200, 260, 80.f, 0.15f, 0.4f, 1.f }, 2, 2));
		AddThemeLoot(T, TEXT("Bunker"), { TEXT("shelf_supplies") },
			Team({ TEXT("shelf"), LOCTEXT("Shelf", "Estante de suprimentos"), LOCTEXT("ShelfDesc", "Enlatados para uma guerra que ainda não começou."), 170, 220, 70.f, 0.2f, 0.4f, 1.f }, 2, 2));
		AddThemeLoot(T, TEXT("Bunker"), { TEXT("bunk_bed") },
			Team({ TEXT("bunkbed"), LOCTEXT("BunkBed", "Beliche de aço"), LOCTEXT("BunkBedDesc", "Alguém dormiu aqui por muito tempo. Talvez ainda durma."), 380, 470, 105.f, 0.1f, 0.3f, 1.f }, 3, 2));

		// ---------- Casa de máquinas (Assets/PuzzleMachinery): subsolo ----------
		AddThemeLoot(T, TEXT("PuzzleMachinery"), { TEXT("fuse_cartridge") },
			Floors({ TEXT("fuse"), LOCTEXT("Fuse", "Fusível industrial"), LOCTEXT("FuseDesc", "Queimado. Ou quase. Melhor não testar."), 26, 40, 0.8f, 0.6f, 1.f, 1.f }, 2));
		AddThemeLoot(T, TEXT("PuzzleMachinery"), { TEXT("gauge_meter") },
			Floors({ TEXT("gauge"), LOCTEXT("Gauge", "Manômetro de caldeira"), LOCTEXT("GaugeDesc", "O ponteiro marca uma pressão que não existe."), 40, 56, 2.f, 0.7f, 0.8f, 1.f }, 2));

		return T;
	}

	const TArray<FNoclipLootDef>& LootTable()
	{
		static TArray<FNoclipLootDef> Defs;
		static int32 BuiltVersion = INDEX_NONE;
		if (BuiltVersion != LootTableVersion || Defs.Num() == 0)
		{
			Defs = BuildLootTable();
			BuiltVersion = LootTableVersion;
		}
		return Defs;
	}
}

namespace NoclipLoot
{
	int32 Num()
	{
		return LootTable().Num();
	}

	const FNoclipLootDef* Get(int32 Index)
	{
		const TArray<FNoclipLootDef>& T = LootTable();
		return T.IsValidIndex(Index) ? &T[Index] : nullptr;
	}

	UStaticMesh* LoadMesh(int32 Index)
	{
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
		const FNoclipLootDef* D = Get(Index);
		if (!D)
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(D->MeshPath); Found && Found->IsValid())
		{
			return Found->Get();
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *D->MeshPath);
		if (Mesh)
		{
			Mesh->AddToRoot();
			Cache.Add(D->MeshPath, Mesh);
		}
		return Mesh;
	}

	int32 PickWeighted(FRandomStream& Stream, int32 Floor)
	{
		const TArray<FNoclipLootDef>& T = LootTable();
		const uint8 Bit = static_cast<uint8>(1u << FMath::Clamp(Floor, 0, 1));
		auto Weight = [Bit](const FNoclipLootDef& D) { return (D.FloorMask & Bit) ? FMath::Max(D.SpawnWeight, 0.f) : 0.f; };
		float Total = 0.f;
		for (const FNoclipLootDef& D : T)
		{
			Total += Weight(D);
		}
		if (Total <= 0.f)
		{
			return T.Num() > 0 ? 0 : INDEX_NONE;
		}
		float Roll = Stream.FRandRange(0.f, Total);
		int32 LastValid = 0;
		for (int32 i = 0; i < T.Num(); ++i)
		{
			const float W = Weight(T[i]);
			if (W <= 0.f)
			{
				continue;
			}
			LastValid = i;
			Roll -= W;
			if (Roll <= 0.f)
			{
				return i;
			}
		}
		return LastValid;
	}
}

#undef LOCTEXT_NAMESPACE
