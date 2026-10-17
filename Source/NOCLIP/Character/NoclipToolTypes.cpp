// Copyright Fuly Games. All Rights Reserved.

#include "Character/NoclipToolTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/NoclipExtractionZone.h"

#define LOCTEXT_NAMESPACE "NoclipTools"

namespace
{
	TArray<NoclipTools::FToolDef> BuildTools()
	{
		using namespace NoclipTools;
		TArray<FToolDef> Defs;
		Defs.AddDefaulted(4);

		// Assets em /Game/NOCLIP/Assets (um modelo de cada pacote).
		{
			FToolDef& D = Defs[static_cast<int32>(ENoclipTool::Flashlight)];
			D.Tool = ENoclipTool::Flashlight;
			D.Name = LOCTEXT("Flashlight", "Lanterna barata");
			D.Short = LOCTEXT("FlashlightShort", "Lanterna");
			D.Slot = 1;
			D.MeshPath = TEXT("/Game/NOCLIP/Assets/Flashlight/FlashLight_Set_2_1/StaticMeshes/FlashLight_Set_2_1.FlashLight_Set_2_1");
			D.MeshScale = 1.f;
			D.AimRotation = FRotator(-90.f, 0.f, 0.f); // a lente (+Z do modelo) aponta para a mira
			D.GripOffset = FVector(4.f, 0.f, 0.f);
			D.ViewOffset = FVector(46.f, 19.f, -19.f);
			D.ViewRotation = FRotator(-90.f, 0.f, 0.f);
		}
		{
			FToolDef& D = Defs[static_cast<int32>(ENoclipTool::Medkit)];
			D.Tool = ENoclipTool::Medkit;
			D.Name = LOCTEXT("Medkit", "Kit de primeiros socorros");
			D.Short = LOCTEXT("MedkitShort", "Kit médico");
			D.Slot = 2;
			D.MeshPath = TEXT("/Game/NOCLIP/Assets/Medpack/Med_1/StaticMeshes/Med_1.Med_1");
			D.MeshScale = 0.014f; // o FBX veio em escala de metros (18 x 10 m)
			D.AimRotation = FRotator(0.f, 90.f, 0.f);
			D.GripOffset = FVector(6.f, -10.f, 0.f);
			D.ViewOffset = FVector(36.f, 9.f, -19.f);
			D.ViewRotation = FRotator(8.f, 90.f, 0.f);
		}
		{
			FToolDef& D = Defs[static_cast<int32>(ENoclipTool::Phone)];
			D.Tool = ENoclipTool::Phone;
			D.Name = LOCTEXT("Phone", "Celular pessoal");
			D.Short = LOCTEXT("PhoneShort", "Celular");
			D.Slot = 3;
			D.MeshPath = TEXT("/Game/NOCLIP/Assets/Phone/PSX_MOBILE_000/StaticMeshes/PSX_MOBILE_000.PSX_MOBILE_000");
			D.MeshScale = 0.34f; // 36 cm no FBX -> 12 cm
			D.AimRotation = FRotator(0.f, -90.f, 0.f); // tela virada para quem segura
			D.GripOffset = FVector(3.f, 0.f, 3.f);
			D.ViewOffset = FVector(30.f, 13.f, -15.f);
			D.ViewRotation = FRotator(20.f, -90.f, 0.f);
		}
		return Defs;
	}
}

namespace NoclipTools
{
	const FToolDef& Get(ENoclipTool Tool)
	{
		// Versao da tabela: com Live Coding a tabela nova substitui a antiga sem reiniciar o editor.
		constexpr int32 TableVersion = 2;
		static TArray<FToolDef> Defs;
		static int32 BuiltVersion = -1;
		if (BuiltVersion != TableVersion)
		{
			Defs = BuildTools();
			BuiltVersion = TableVersion;
		}
		return Defs[FMath::Clamp(static_cast<int32>(Tool), 0, Defs.Num() - 1)];
	}

	UStaticMesh* LoadMesh(ENoclipTool Tool)
	{
		static TMap<uint8, TWeakObjectPtr<UStaticMesh>> Cache;
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(static_cast<uint8>(Tool)); Found && Found->IsValid())
		{
			return Found->Get();
		}
		const FToolDef& Def = Get(Tool);
		UStaticMesh* Mesh = Def.MeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Def.MeshPath);
		if (Mesh)
		{
			Mesh->AddToRoot();
			Cache.Add(static_cast<uint8>(Tool), Mesh);
		}
		return Mesh;
	}

	int32 PhoneSignalBars(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return -1;
		}
		static TWeakObjectPtr<const ANoclipExtractionZone> CachedZone;
		const ANoclipExtractionZone* Zone = CachedZone.Get();
		if (!Zone || Zone->GetWorld() != World)
		{
			Zone = nullptr;
			for (TActorIterator<ANoclipExtractionZone> It(const_cast<UWorld*>(World)); It; ++It)
			{
				Zone = *It;
				break;
			}
			CachedZone = Zone;
		}
		if (!Zone)
		{
			return -1;
		}
		const float Meters = FVector::Dist2D(Zone->GetActorLocation(), Location) / 100.f;
		return Meters < 25.f ? 3 : (Meters < 50.f ? 2 : (Meters < 80.f ? 1 : 0));
	}
}

#undef LOCTEXT_NAMESPACE
