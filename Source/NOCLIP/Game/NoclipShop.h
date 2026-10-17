// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/NoclipToolTypes.h"

/**
 * Almoxarifado do saguao (mesmos itens e precos do prototipo). Lanterna e kit medico vao para o cinto de quem
 * comprou (UNoclipToolComponent); os efeitos dos outros itens entram depois.
 */
namespace NoclipShop
{
	struct FItem
	{
		const TCHAR* Id;
		FText Name;
		FText Desc;
		int32 Price;
	};

	inline const TArray<FItem>& Items()
	{
		static const TArray<FItem> List = {
			{ TEXT("flashlight"), NSLOCTEXT("NoclipShop", "Flashlight", "Lanterna barata"), NSLOCTEXT("NoclipShop", "FlashlightDesc", "Afasta o Sorridente. Pisca no fim da bateria."), 10 },
			{ TEXT("medkit"), NSLOCTEXT("NoclipShop", "Medkit", "Kit de primeiros socorros"), NSLOCTEXT("NoclipShop", "MedkitDesc", "Cura 60 e devolve o fôlego. Até 3 no cinto."), 25 },
			{ TEXT("chalk"), NSLOCTEXT("NoclipShop", "Chalk", "Giz e fita"), NSLOCTEXT("NoclipShop", "ChalkDesc", "Marca caminhos e trava salas."), 15 },
			{ TEXT("water"), NSLOCTEXT("NoclipShop", "Water", "Água de amêndoa"), NSLOCTEXT("NoclipShop", "WaterDesc", "Cura e recupera fôlego. Voz fina."), 20 },
			{ TEXT("cart"),  NSLOCTEXT("NoclipShop", "Cart", "Carrinho"), NSLOCTEXT("NoclipShop", "CartDesc", "Leva 3 achados ou 1 colega."), 60 },
			{ TEXT("radio"), NSLOCTEXT("NoclipShop", "Radio", "Walkie-talkie"), NSLOCTEXT("NoclipShop", "RadioDesc", "Fala de longe, com estática."), 50 },
			{ TEXT("vhs"),   NSLOCTEXT("NoclipShop", "Vhs", "Câmera VHS"), NSLOCTEXT("NoclipShop", "VhsDesc", "Fixa a sala filmada."), 70 },
			{ TEXT("lamp"),  NSLOCTEXT("NoclipShop", "Lamp", "Lâmpada portátil"), NSLOCTEXT("NoclipShop", "LampDesc", "Ilumina um setor no apagão."), 120 },
		};
		return List;
	}

	/** Item do almoxarifado que vira ferramenta no cinto (ou None). */
	inline ENoclipTool ToolFor(int32 ItemIndex)
	{
		if (!Items().IsValidIndex(ItemIndex))
		{
			return ENoclipTool::None;
		}
		const FString Id = Items()[ItemIndex].Id;
		return Id == TEXT("flashlight") ? ENoclipTool::Flashlight : (Id == TEXT("medkit") ? ENoclipTool::Medkit : ENoclipTool::None);
	}
}
