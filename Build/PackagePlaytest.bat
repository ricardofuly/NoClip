@echo off
setlocal EnableExtensions
rem ======================================================================
rem  NOCLIP - gera o build jogavel (Windows) e um .zip pronto para mandar.
rem
rem  Uso:  Build\PackagePlaytest.bat              (Development: console ' e logs)
rem        Build\PackagePlaytest.bat shipping     (Shipping: mais leve, sem console)
rem        Build\PackagePlaytest.bat nozip        (so a pasta, sem .zip: usado pelo PublishRelease.bat)
rem
rem  Saida: F:\Dev\NOCLIP\Saved\Playtest\NOCLIP_Playtest_<versao>_<config>.zip
rem  Feche o editor antes (o cook usa os mesmos arquivos).
rem ======================================================================

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
set "PROJECT=%PROJECT_DIR%\NOCLIP.uproject"
set "VERSION=0.1.0"

set "CONFIG=Development"
set "MAKEZIP=1"
for %%A in (%*) do (
	if /I "%%~A"=="shipping" set "CONFIG=Shipping"
	if /I "%%~A"=="nozip" set "MAKEZIP=0"
)

set "OUT=%PROJECT_DIR%\Saved\Playtest\%CONFIG%"
set "ZIP=%PROJECT_DIR%\Saved\Playtest\NOCLIP_Playtest_%VERSION%_%CONFIG%.zip"

if not exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" (
	echo [ERRO] Unreal 5.8 nao encontrado em "%UE_ROOT%". Edite UE_ROOT no topo deste arquivo.
	exit /b 1
)

echo.
echo === NOCLIP %VERSION% - empacotando (%CONFIG%) ===
echo Projeto: %PROJECT%
echo Saida:   %OUT%
echo.

if exist "%OUT%" rmdir /s /q "%OUT%"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
	-project="%PROJECT%" -noP4 -utf8output ^
	-platform=Win64 -clientconfig=%CONFIG% ^
	-build -cook -stage -pak -iostore -compressed -prereqs ^
	-archive -archivedirectory="%OUT%" ^
	-nodebuginfo -unattended
if errorlevel 1 (
	echo.
	echo [ERRO] O empacotamento falhou. Veja o log acima ^(procure por "Error:"^).
	exit /b 1
)

rem O build fica em %OUT%\Windows
if not exist "%OUT%\Windows\NOCLIP.exe" (
	echo [ERRO] NOCLIP.exe nao apareceu em "%OUT%\Windows".
	exit /b 1
)

rem Steam (AppID 480) ao lado do executavel do jogo, nos dois lugares que a Steam procura.
copy /y "%PROJECT_DIR%\Build\Steam\steam_appid.txt" "%OUT%\Windows\NOCLIP\Binaries\Win64\steam_appid.txt" >nul
copy /y "%PROJECT_DIR%\Build\Steam\steam_appid.txt" "%OUT%\Windows\steam_appid.txt" >nul
copy /y "%PROJECT_DIR%\Build\LEIA-ME.txt" "%OUT%\Windows\LEIA-ME.txt" >nul

if "%MAKEZIP%"=="0" (
	echo.
	echo Build pronto em "%OUT%\Windows".
	exit /b 0
)

echo.
echo === Compactando ===
if exist "%ZIP%" del /q "%ZIP%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%OUT%\Windows\*' -DestinationPath '%ZIP%' -CompressionLevel Optimal"
if errorlevel 1 (
	echo [ERRO] Nao deu para criar o .zip. A pasta pronta esta em "%OUT%\Windows".
	exit /b 1
)

echo.
echo ======================================================================
echo  Pronto: %ZIP%
echo  Mande esse .zip. Quem receber extrai e abre NOCLIP.exe (com a Steam aberta).
echo ======================================================================
explorer /select,"%ZIP%"
exit /b 0
