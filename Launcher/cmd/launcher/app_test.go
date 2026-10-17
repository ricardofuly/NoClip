// Copyright Fuly Games. All Rights Reserved.

package main

import (
	"bytes"
	"compress/gzip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"github.com/ricardofuly/NoClip/Launcher/internal/patch"
)

// Um release publicado com dois arquivos; o App instala, fica pronto e abre "o jogo".
func TestAppInstallFlow(t *testing.T) {
	files := map[string][]byte{
		"NOCLIP.exe":                  []byte("#!/bin/sh\nexit 0\n"),
		"NOCLIP/Content/Paks/a.pak":   bytes.Repeat([]byte("pak"), 1000),
		"NOCLIP/Binaries/Win64/x.dll": []byte("dll"),
	}
	var srv *httptest.Server
	blobs := map[string][]byte{}
	m := patch.Manifest{Game: "NOCLIP", Version: "0.2.0", Tag: "v0.2.0", Exe: "NOCLIP.exe"}
	for p, b := range files {
		var gz bytes.Buffer
		w := gzip.NewWriter(&gz)
		w.Write(b)
		w.Close()
		sum := sha256.Sum256(b)
		name := hex.EncodeToString(sum[:]) + ".gz"
		blobs[name] = gz.Bytes()
		m.Files = append(m.Files, patch.File{Path: p, Size: int64(len(b)), SHA256: hex.EncodeToString(sum[:]), GzSize: int64(gz.Len()), URL: "SRV/dl/" + name})
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/repos/"+Repo+"/releases/latest", func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode(patch.Release{TagName: "v0.2.0", Body: "notas", HTMLURL: "https://github.com/x",
			Assets: []patch.Asset{{Name: patch.ManifestAsset, BrowserDownloadURL: srv.URL + "/dl/manifest.json"}}})
	})
	mux.HandleFunc("/dl/", func(w http.ResponseWriter, r *http.Request) {
		name := filepath.Base(r.URL.Path)
		if name == "manifest.json" {
			mm := m
			mm.Files = nil
			for _, f := range m.Files {
				f.URL = srv.URL + f.URL[3:]
				mm.Files = append(mm.Files, f)
			}
			json.NewEncoder(w).Encode(mm)
			return
		}
		w.Write(blobs[name])
	})
	srv = httptest.NewServer(mux)
	defer srv.Close()
	t.Setenv("NOCLIP_GITHUB_API", srv.URL)
	t.Setenv("LOCALAPPDATA", t.TempDir())

	var mu sync.Mutex
	var phases []string
	app := NewApp(func(s UIState) {
		mu.Lock()
		if len(phases) == 0 || phases[len(phases)-1] != s.Phase {
			phases = append(phases, s.Phase)
		}
		mu.Unlock()
	})
	app.Check()
	if got := app.Snapshot(); got.Phase != PhaseInstall || got.Latest != "0.2.0" || got.Download == 0 {
		t.Fatalf("depois de procurar: %+v", got)
	}
	app.Install()
	if got := app.Snapshot(); got.Phase != PhaseReady || got.Installed != "0.2.0" {
		t.Fatalf("depois de instalar: fase %s, instalado %s (%s %s)", got.Phase, got.Installed, got.Message, got.Detail)
	}
	for p, b := range files {
		got, err := os.ReadFile(filepath.Join(app.gameDir, filepath.FromSlash(p)))
		if err != nil || !bytes.Equal(got, b) {
			t.Fatalf("%s instalado errado", p)
		}
	}
	// Abrir de novo: ja esta pronto (nao baixa nada).
	app2 := NewApp(func(UIState) {})
	app2.Check()
	if got := app2.Snapshot(); got.Phase != PhaseReady || got.Download != 0 {
		t.Fatalf("segunda abertura: %+v", got)
	}
	// Botao principal em "pronto" abre o jogo e volta para pronto quando ele fecha.
	os.Chmod(filepath.Join(app2.gameDir, "NOCLIP.exe"), 0o755)
	app2.state.PrereqDone = true
	app2.Action("primary")
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) && app2.Snapshot().Phase != PhaseReady {
		time.Sleep(20 * time.Millisecond)
	}
	time.Sleep(100 * time.Millisecond)
	if got := app2.Snapshot(); got.Phase != PhaseReady {
		t.Fatalf("depois de jogar: %s %s %s", got.Phase, got.Message, got.Detail)
	}
	mu.Lock()
	t.Logf("fases: %v", phases)
	mu.Unlock()
}
