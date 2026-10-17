// Copyright Fuly Games. All Rights Reserved.

using UnrealBuildTool;

public class NOCLIP : ModuleRules
{
	public NOCLIP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NetCore", "PhysicsCore",
			"Mover", "NetworkPrediction",
			"GameplayAbilities", "GameplayTags", "GameplayTasks", "AIModule",
			"OnlineSubsystem", "OnlineSubsystemUtils",
			"UMG", "Slate", "SlateCore", "CommonUI", "CommonInput", "ApplicationCore",
			"ControlRig", "RigVM"
		});

		// Escolha de fone e microfone (Voice/NoclipAudioDevices).
		PrivateDependencyModuleNames.AddRange(new string[] { "Voice", "AudioMixer", "AudioMixerCore", "SignalProcessing", "EngineSettings" });
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("dsound.lib");
		}

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		// Steam no jogo empacotado: o steam_appid.txt (480 = Spacewar, AppID de teste da Valve) vai junto do .exe.
		// Em Development o motor escreve o arquivo sozinho; em Shipping ele precisa estar la, senao a Steam fecha o jogo.
		if (Target.Type == TargetType.Game || Target.Type == TargetType.Client)
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/steam_appid.txt", "$(ProjectDir)/Build/Steam/steam_appid.txt", StagedFileType.NonUFS);
		}
	}
}
