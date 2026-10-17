// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NoclipConsoleSubsystem.generated.h"

class FNoclipConsoleKeyProcessor;

/**
 * Console de comandos acima da UI. Com a CommonUI em modo menu (pausa, sala de espera, relatorio) o foco fica
 * num widget e a tecla do console nunca chega ao viewport. Aqui um pre-processador do Slate ve a tecla antes
 * de qualquer widget, abre o console e deixa o foco no viewport enquanto se digita.
 * Teclas: as de ConsoleKeys (DefaultInput.ini: ' no teclado ABNT2 e ~ no americano). Nao existe em Shipping.
 */
UCLASS()
class NOCLIP_API UNoclipConsoleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** O console esta aberto (digitando)? A UI nao deve puxar o foco de volta enquanto isso. */
	static bool IsConsoleOpen();

private:
	TSharedPtr<FNoclipConsoleKeyProcessor> Processor;
};
