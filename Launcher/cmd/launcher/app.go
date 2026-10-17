// Copyright Fuly Games. All Rights Reserved.

package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/ricardofuly/NoClip/Launcher/internal/patch"
)

// Repo de onde vem o jogo (as releases precisam ser publicas).
const Repo = "ricardofuly/NoClip"

// Version do launcher (definida no build: -ldflags "-X main.Version=1.0.0").
var Version = "dev"

// Fases que a tela mostra.
const (
	PhaseChecking    = "checking"
	PhaseInstall     = "install"     // nada instalado: botao Instalar
	PhaseUpdate      = "update"      // versao nova: botao Atualizar
	PhaseDownloading = "downloading" // baixando
	PhaseVerifying   = "verifying"   // conferindo arquivos
	PhaseReady       = "ready"       // botao Jogar
	PhaseRunning     = "running"     // jogo aberto
	PhaseOffline     = "offline"     // sem internet: joga a versao instalada
	PhaseError       = "error"
	PhaseSelfUpdate  = "selfupdate" // trocando o proprio launcher
)

// UIState e o que vai para a pagina.
type UIState struct {
	Phase           string          `json:"phase"`
	Installed       string          `json:"installed"`
	Latest          string          `json:"latest"`
	Notes           string          `json:"notes"`
	ReleaseURL      string          `json:"releaseUrl"`
	Message         string          `json:"message"`
	Detail          string          `json:"detail"`
	Download        int64           `json:"download"` // bytes a baixar
	InstallSize     int64           `json:"installSize"`
	Progress        *patch.Progress `json:"progress,omitempty"`
	Hash            [2]int          `json:"hash"` // conferindo: feito, total
	SteamRunning    bool            `json:"steamRunning"`
	LauncherVersion string          `json:"launcherVersion"`
	GameDir         string          `json:"gameDir"`
}

// App guarda o estado do launcher.
type App struct {
	mu        sync.Mutex
	ui        UIState
	push      func(UIState)
	client    *patch.Client
	baseDir   string
	gameDir   string
	statePath string
	state     *patch.State
	manifest  *patch.Manifest
	plan      *patch.Plan
	busy      bool
	cancel    context.CancelFunc
	lastPush  time.Time
}

// NewApp prepara as pastas (%LOCALAPPDATA%\NOCLIP).
func NewApp(push func(UIState)) *App {
	base := os.Getenv("LOCALAPPDATA")
	if base == "" {
		base, _ = os.UserCacheDir()
	}
	base = filepath.Join(base, "NOCLIP")
	a := &App{
		push:      push,
		client:    patch.NewClient(Repo, "", "NOCLIP-Launcher/"+Version),
		baseDir:   base,
		gameDir:   filepath.Join(base, "Game"),
		statePath: filepath.Join(base, "launcher.json"),
	}
	a.state = patch.LoadState(a.statePath)
	a.ui = UIState{Phase: PhaseChecking, Installed: a.state.Version, LauncherVersion: Version, GameDir: a.gameDir}
	return a
}

// Snapshot do estado atual.
func (a *App) Snapshot() UIState {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.ui.SteamRunning = steamRunning()
	return a.ui
}

func (a *App) update(f func(*UIState), force bool) {
	a.mu.Lock()
	f(&a.ui)
	s := a.ui
	now := time.Now()
	if !force && now.Sub(a.lastPush) < 100*time.Millisecond {
		a.mu.Unlock()
		return
	}
	a.lastPush = now
	a.mu.Unlock()
	s.SteamRunning = steamRunning()
	a.push(s)
}

func (a *App) fail(msg string, err error) {
	a.update(func(s *UIState) {
		s.Phase = PhaseError
		s.Message = msg
		s.Detail = ""
		if err != nil {
			s.Detail = err.Error()
		}
		s.Progress = nil
	}, true)
}

func (a *App) installed() bool {
	_, err := os.Stat(filepath.Join(a.gameDir, "NOCLIP.exe"))
	return err == nil && a.state.Version != ""
}

// Check procura a versao nova (chamado ao abrir e no "Tentar de novo").
func (a *App) Check() {
	if !a.begin() {
		return
	}
	defer a.end()
	a.update(func(s *UIState) {
		s.Phase = PhaseChecking
		s.Message = "Procurando atualizações..."
		s.Detail = ""
		s.Progress = nil
	}, true)

	rel, err := a.client.LatestRelease()
	if err == nil {
		var m *patch.Manifest
		m, err = a.client.FetchManifest(rel)
		if err == nil {
			a.manifest = m
			a.update(func(s *UIState) {
				s.Latest = m.Version
				s.Notes = rel.Body
				s.ReleaseURL = rel.HTMLURL
				s.InstallSize = m.TotalSize()
			}, true)
		}
	}
	if err != nil {
		if a.installed() {
			a.update(func(s *UIState) {
				s.Phase = PhaseOffline
				s.Message = "Sem conexão com o servidor de atualizações. Você pode jogar a versão instalada."
				s.Detail = err.Error()
			}, true)
			return
		}
		if errors.Is(err, patch.ErrNoRelease) {
			a.fail("Ainda não há nenhuma versão publicada.", nil)
		} else {
			a.fail("Não foi possível falar com o GitHub. Confira a internet e tente de novo.", err)
		}
		return
	}

	// Launcher novo primeiro (o jogo novo pode depender dele).
	if l := a.manifest.Launcher; l != nil && Version != "dev" && l.URL != "" && patch.CompareVersions(l.Version, Version) > 0 {
		a.update(func(s *UIState) {
			s.Phase = PhaseSelfUpdate
			s.Message = "Atualizando o launcher para a versão " + l.Version + "..."
		}, true)
		if err := selfUpdate(a.client, l); err != nil {
			// Segue com o launcher atual: nao e motivo para travar o jogo.
			a.update(func(s *UIState) { s.Detail = "Não deu para atualizar o launcher: " + err.Error() }, true)
		} else {
			return // o novo ja abriu; este fecha
		}
	}
	a.planAndShow(false)
}

// planAndShow compara a pasta com o manifesto e mostra o botao certo.
func (a *App) planAndShow(verify bool) {
	if a.manifest == nil {
		return
	}
	if verify {
		a.update(func(s *UIState) {
			s.Phase = PhaseVerifying
			s.Message = "Conferindo os arquivos do jogo..."
			s.Hash = [2]int{0, len(a.manifest.Files)}
		}, true)
	}
	plan, err := patch.MakePlan(a.manifest, a.gameDir, a.state, verify, func(done, total int) {
		if verify {
			a.update(func(s *UIState) { s.Hash = [2]int{done, total} }, false)
		}
	})
	if err != nil {
		a.fail("O manifesto da versão nova está com problema.", err)
		return
	}
	_ = a.state.Save(a.statePath)
	a.plan = plan
	upToDate := plan.UpToDate() && a.state.Version == a.manifest.Version
	if plan.UpToDate() && a.state.Version != a.manifest.Version {
		// Os arquivos ja batem (ex.: instalado a mao): so registra a versao.
		a.state.Version = a.manifest.Version
		_ = a.state.Save(a.statePath)
		upToDate = true
	}
	a.update(func(s *UIState) {
		s.Installed = a.state.Version
		s.Download = plan.DownloadBytes
		s.Progress = nil
		s.Detail = ""
		switch {
		case upToDate:
			s.Phase = PhaseReady
			s.Message = "Pronto para o expediente."
			if verify {
				s.Message = "Todos os arquivos conferidos. Pronto para o expediente."
			}
		case !a.installed():
			s.Phase = PhaseInstall
			s.Message = fmt.Sprintf("O jogo ainda não está instalado. Download de %s.", human(plan.DownloadBytes))
		default:
			s.Phase = PhaseUpdate
			s.Message = fmt.Sprintf("Versão %s disponível: %d arquivo(s), %s para baixar.", a.manifest.Version, len(plan.Needed), human(plan.DownloadBytes))
		}
	}, true)
}

// Install baixa o que falta (instalar e atualizar sao a mesma coisa).
func (a *App) Install() {
	if !a.begin() {
		return
	}
	defer a.end()
	if a.manifest == nil || a.plan == nil {
		return
	}
	if err := os.MkdirAll(a.gameDir, 0o755); err != nil {
		a.fail("Não deu para criar a pasta do jogo.", err)
		return
	}
	ctx, cancel := context.WithCancel(context.Background())
	a.mu.Lock()
	a.cancel = cancel
	a.mu.Unlock()
	defer cancel()

	a.update(func(s *UIState) {
		s.Phase = PhaseDownloading
		s.Message = "Baixando a versão " + a.manifest.Version + "..."
		s.Progress = &patch.Progress{Total: a.plan.DownloadBytes, FileCount: len(a.plan.Needed)}
	}, true)
	err := patch.Apply(ctx, a.client, a.plan, a.gameDir, a.state, a.statePath, func(p patch.Progress) {
		a.update(func(s *UIState) { pp := p; s.Progress = &pp }, false)
	})
	if errors.Is(err, context.Canceled) {
		_ = a.state.Save(a.statePath)
		a.planAndShow(false) // mostra o que ainda falta (continua de onde parou)
		return
	}
	if err != nil {
		a.fail("O download não terminou. Clique em tentar de novo: ele continua de onde parou.", err)
		return
	}
	a.state.Version = a.manifest.Version
	_ = a.state.Save(a.statePath)
	a.planAndShow(false)
}

// Cancel pausa o download.
func (a *App) Cancel() {
	a.mu.Lock()
	c := a.cancel
	a.mu.Unlock()
	if c != nil {
		c()
	}
}

// Verify recalcula o hash de tudo e baixa o que estiver errado.
func (a *App) Verify() {
	if !a.begin() {
		return
	}
	defer a.end()
	if a.manifest == nil {
		return
	}
	a.planAndShow(true)
}

// Play abre o jogo (instala o Visual C++ do Unreal na primeira vez).
func (a *App) Play() {
	if !a.begin() {
		return
	}
	defer a.end()
	exe := filepath.Join(a.gameDir, "NOCLIP.exe")
	if a.manifest != nil && a.manifest.Exe != "" {
		if p, err := patch.SafeJoin(a.gameDir, a.manifest.Exe); err == nil {
			exe = p
		}
	}
	if !a.state.PrereqDone {
		prereq := filepath.Join(a.gameDir, "Engine", "Extras", "Redist", "en-us", "UEPrereqSetup_x64.exe")
		if a.manifest != nil && a.manifest.Prereq != "" {
			if p, err := patch.SafeJoin(a.gameDir, a.manifest.Prereq); err == nil {
				prereq = p
			}
		}
		if _, err := os.Stat(prereq); err == nil {
			a.update(func(s *UIState) { s.Message = "Instalando os componentes do Windows (só na primeira vez)..." }, true)
			cmd := exec.Command(prereq, "/quiet", "/norestart")
			_ = cmd.Run() // ja instalado ou recusado: o jogo tenta abrir mesmo assim
		}
		a.state.PrereqDone = true
		_ = a.state.Save(a.statePath)
	}
	cmd := exec.Command(exe)
	cmd.Dir = a.gameDir
	if err := cmd.Start(); err != nil {
		a.fail("Não deu para abrir o jogo.", err)
		return
	}
	a.update(func(s *UIState) {
		s.Phase = PhaseRunning
		s.Message = "Expediente em andamento. Boa sorte, estagiário."
		s.Detail = ""
	}, true)
	_ = cmd.Wait()
	a.update(func(s *UIState) { s.Phase = PhaseReady; s.Message = "Pronto para o expediente." }, true)
}

func (a *App) begin() bool {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.busy {
		return false
	}
	a.busy = true
	return true
}

func (a *App) end() {
	a.mu.Lock()
	a.busy = false
	a.cancel = nil
	a.mu.Unlock()
}

// Action vem dos botoes da pagina.
func (a *App) Action(name string) {
	switch name {
	case "primary":
		switch a.Snapshot().Phase {
		case PhaseInstall, PhaseUpdate:
			go a.Install()
		case PhaseReady, PhaseOffline:
			go a.Play()
		case PhaseError:
			go a.Check()
		}
	case "cancel":
		a.Cancel()
	case "verify":
		go a.Verify()
	case "retry":
		go a.Check()
	case "folder":
		_ = os.MkdirAll(a.gameDir, 0o755)
		openPath(a.gameDir)
	case "logs":
		logs := filepath.Join(a.gameDir, "NOCLIP", "Saved", "Logs")
		if _, err := os.Stat(logs); err != nil {
			logs = a.gameDir
		}
		openPath(logs)
	case "release":
		if u := a.Snapshot().ReleaseURL; strings.HasPrefix(u, "https://") {
			openPath(u)
		}
	}
}

// StateJSON para a pagina.
func (a *App) StateJSON() string {
	b, _ := json.Marshal(a.Snapshot())
	return string(b)
}

func human(n int64) string {
	switch {
	case n >= 1<<30:
		return fmt.Sprintf("%.2f GB", float64(n)/(1<<30))
	case n >= 1<<20:
		return fmt.Sprintf("%.0f MB", float64(n)/(1<<20))
	case n >= 1<<10:
		return fmt.Sprintf("%.0f KB", float64(n)/(1<<10))
	}
	return fmt.Sprintf("%d B", n)
}
