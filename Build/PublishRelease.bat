@echo off
setlocal EnableExtensions
rem ======================================================================
rem  NOCLIP - publica uma versao nova para o launcher baixar.
rem
rem  Uso:  Build\PublishRelease.bat 0.1.1
rem
rem  1. grava a versao em Config\DefaultGame.ini (as salas so juntam a mesma versao)
rem  2. empacota (Build\PackagePlaytest.bat)
rem  3. envia ao GitHub Releases so o que mudou desde a ultima versao + o launcher
rem
rem  Notas da versao: edite Build\notas.md antes (aparece no launcher).
rem  Precisa: GitHub CLI logado (winget install GitHub.cli  e depois  gh auth login)
rem           ou a variavel GITHUB_TOKEN com permissao de "Contents: write".
rem  Feche o editor do Unreal antes.
rem ======================================================================

set "VERSION=%~1"
if "%VERSION%"=="" (
	echo Uso: Build\PublishRelease.bat ^<versao^>     ex.: Build\PublishRelease.bat 0.1.1
	exit /b 2
)
set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
set "TOOLS=%PROJECT_DIR%\Build\Tools"
set "BUILD=%PROJECT_DIR%\Saved\Playtest\Development\Windows"

if not exist "%TOOLS%\NOCLIP-Publisher.exe" (
	echo [ERRO] Falta %TOOLS%\NOCLIP-Publisher.exe
	exit /b 1
)
if "%GITHUB_TOKEN%"=="" (
	where gh >nul 2>nul || (
		echo [ERRO] Sem GitHub CLI e sem GITHUB_TOKEN. Instale: winget install GitHub.cli  e rode: gh auth login
		exit /b 1
	)
	gh auth status >nul 2>nul || (
		echo [ERRO] GitHub CLI sem login. Rode: gh auth login
		exit /b 1
	)
)

echo === 1/3 Versao %VERSION% ===
powershell -NoProfile -Command "$p='%PROJECT_DIR%\Config\DefaultGame.ini'; $t=[IO.File]::ReadAllText($p); if ($t -match 'ProjectVersion=') { $t=[regex]::Replace($t,'ProjectVersion=[^\r\n]*','ProjectVersion=%VERSION%') } else { $t=$t.Replace('ProjectName=NOCLIP','ProjectName=NOCLIP'+[Environment]::NewLine+'ProjectVersion=%VERSION%') }; [IO.File]::WriteAllText($p,$t)"
if errorlevel 1 exit /b 1

echo === 2/3 Empacotando ===
call "%PROJECT_DIR%\Build\PackagePlaytest.bat" nozip
if errorlevel 1 exit /b 1

echo === 3/3 Publicando no GitHub ===
set "NOTES=%PROJECT_DIR%\Build\notas.md"
"%TOOLS%\NOCLIP-Publisher.exe" -build "%BUILD%" -version %VERSION% -notes "%NOTES%" -launcher "%TOOLS%\NOCLIP-Launcher.exe"
if errorlevel 1 exit /b 1

rem Guarda a versao no repositorio (se o projeto ja estiver no git).
if exist "%PROJECT_DIR%\.git" (
	pushd "%PROJECT_DIR%"
	git add Config\DefaultGame.ini Build\notas.md >nul 2>nul
	git commit -m "Versao %VERSION%" >nul 2>nul
	git push >nul 2>nul
	popd
)
echo.
echo Versao %VERSION% publicada. Quem abrir o launcher recebe a atualizacao.
exit /b 0
