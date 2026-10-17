# NOCLIP

Party game cooperativo nas Backrooms, com visual PSX. De 1 a 6 estagiários da Baseline Recuperações descem ao Level 0 para recuperar achados e bater a quota antes do colapso. Pelo caminho tem noclip, salas que mudam, apagões e entidades.

Feito por **Fuly Games** em Unreal Engine 5.8 (C++), com Mover, CommonUI e Steam (sessões e voz).

## Jogar

1. Baixe o **`NOCLIP-Launcher.exe`** na [última release](https://github.com/ricardofuly/NoClip/releases/latest).
2. Abra a Steam e entre na sua conta.
3. Abra o launcher e clique em **Instalar**. Depois é só **Jogar**.

Quando sair uma versão nova, o launcher avisa e baixa **só os arquivos que mudaram**. O jogo fica em `%LOCALAPPDATA%\NOCLIP\Game`, e os logs ficam em `...\Game\NOCLIP\Saved\Logs` (tem um botão **LOGS** no launcher).

Por enquanto o jogo usa o AppID de teste da Steam (480, "Spacewar"). Todo mundo precisa estar na mesma versão para se encontrar.

## Desenvolvimento

| Pasta | O que tem |
|---|---|
| `Source/NOCLIP` | Todo o jogo em C++ (gameplay, gerador de nível, entidades, rede, UI, áudio) |
| `Source/NOCLIP/Tests` | Testes de automação (`NOCLIP.Unit.*`) e o teste do jogo inteiro (`noclip.SelfTest`) |
| `Content/NOCLIP` | Assets (Git LFS) |
| `Launcher` | Launcher e publicador, em Go |
| `Build` | Scripts de empacotar, publicar e configurar o repositório |

### Publicar uma versão

```bat
Build\PublishRelease.bat 0.1.1
```

O script:
1. grava a versão em `Config/DefaultGame.ini`. É ela que separa as salas por versão;
2. empacota o jogo (`Build\PackagePlaytest.bat`);
3. sobe no GitHub Releases só o que mudou desde a última versão, junto com o launcher.

As notas da versão vêm de `Build\notas.md`.

Precisa do [GitHub CLI](https://cli.github.com) logado (`gh auth login`) ou da variável `GITHUB_TOKEN`.

> Não apague releases antigas: os arquivos que não mudaram continuam sendo baixados delas. Se precisar apagar, publique a próxima versão com `-full`.

### Recompilar o launcher

```bat
cd Launcher
go build -trimpath -ldflags "-s -w -H windowsgui -X main.Version=1.0.1" -o ..\Build\Tools\NOCLIP-Launcher.exe .\cmd\launcher
go build -trimpath -ldflags "-s -w" -o ..\Build\Tools\NOCLIP-Publisher.exe .\cmd\publisher
go test ./...
```

Os launchers que já estão instalados se atualizam sozinhos quando uma release traz uma versão nova do launcher.
