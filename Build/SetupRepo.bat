@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem ======================================================================
rem  NOCLIP - sobe o projeto para https://github.com/ricardofuly/NoClip
rem  (uma vez so; depois e git add / commit / push normal).
rem
rem  Precisa: Git e Git LFS.   winget install Git.Git   (o Git LFS ja vem junto)
rem  Login: o Git abre o navegador para entrar no GitHub na primeira vez.
rem ======================================================================

set "REMOTE=https://github.com/ricardofuly/NoClip.git"
set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
cd /d "%PROJECT_DIR%"

where git >nul 2>nul || (echo [ERRO] Git nao encontrado. Instale com: winget install Git.Git & exit /b 1)
git lfs version >nul 2>nul || (echo [ERRO] Git LFS nao encontrado. Reinstale o Git marcando "Git LFS". & exit /b 1)

rem Branch padrao do repositorio no GitHub (o NoClip foi criado com "master").
set "BRANCH=main"
for /f "tokens=2" %%B in ('git ls-remote --symref "%REMOTE%" HEAD 2^>nul ^| findstr /b "ref:"') do set "REMOTEREF=%%B"
if defined REMOTEREF set "BRANCH=!REMOTEREF:refs/heads/=!"
echo Branch do GitHub: %BRANCH%

if not exist ".git" (
	git init -b %BRANCH% || exit /b 1
)
git lfs install --local >nul
git remote get-url origin >nul 2>nul && git remote set-url origin "%REMOTE%" || git remote add origin "%REMOTE%"
git rev-parse --abbrev-ref HEAD 2>nul | findstr /x /c:"%BRANCH%" >nul || git branch -M %BRANCH%

rem ---- Tamanho dos assets: o Git LFS gratis do GitHub e 1 GB de espaco e 1 GB de download por mes.
for /f %%S in ('powershell -NoProfile -Command "[math]::Round(((Get-ChildItem -Recurse -File 'Content' | Measure-Object Length -Sum).Sum)/1GB,2)"') do set "CONTENT_GB=%%S"
echo.
echo Pasta Content: !CONTENT_GB! GB (vai pelo Git LFS).
echo O plano gratis do GitHub tem 1 GB de LFS. Acima disso o push do Content falha,
echo a menos que voce compre um pacote de dados de LFS no GitHub.
echo O jogo para os jogadores NAO depende disso: o launcher baixa pelas Releases, que nao tem esse limite.
echo.
choice /C SN /M "Subir a pasta Content junto (S) ou so o codigo e configs (N)"
if errorlevel 2 (
	findstr /x /c:"Content/" ".git\info\exclude" >nul 2>nul || echo Content/>>".git\info\exclude"
	echo Content fica so no seu PC.
) else (
	rem Se antes tinha escolhido N, volta a incluir.
	if exist ".git\info\exclude" powershell -NoProfile -Command "(Get-Content '.git\info\exclude') | Where-Object { $_ -ne 'Content/' } | Set-Content '.git\info\exclude'"
)

git add -A || exit /b 1
git diff --cached --quiet || git commit -m "NOCLIP: projeto" || exit /b 1

rem ---- Repositorio criado com README/.gitignore no site: junta as historias (o que esta aqui vence).
git fetch origin >nul 2>nul
git rev-parse --verify --quiet origin/%BRANCH% >nul && (
	git merge origin/%BRANCH% --allow-unrelated-histories --no-edit -X ours || exit /b 1
)

echo.
echo === Enviando (a primeira vez demora: assets pelo LFS) ===
git push -u origin %BRANCH% || (
	echo.
	echo [ERRO] O push falhou. Se foi o limite do LFS, rode de novo e responda N para subir so o codigo.
	exit /b 1
)
echo.
echo Pronto: https://github.com/ricardofuly/NoClip
exit /b 0
