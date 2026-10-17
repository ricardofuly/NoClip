// Copyright Fuly Games. All Rights Reserved.

package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"

	"github.com/ricardofuly/NoClip/Launcher/internal/patch"
)

// fakeGitHub imita o pedaco da API de releases que o publicador e o launcher usam.
type fakeGitHub struct {
	mu        sync.Mutex
	srv       *httptest.Server
	releases  []*patch.Release
	blobs     map[string][]byte
	downloads map[string]int // asset -> vezes baixado
	nextID    int64
}

func newFake(t *testing.T) *fakeGitHub {
	f := &fakeGitHub{blobs: map[string][]byte{}, downloads: map[string]int{}}
	mux := http.NewServeMux()
	mux.HandleFunc("/repos/o/r/releases/latest", func(w http.ResponseWriter, r *http.Request) {
		f.mu.Lock()
		defer f.mu.Unlock()
		for i := len(f.releases) - 1; i >= 0; i-- {
			if !f.releases[i].Draft {
				_ = json.NewEncoder(w).Encode(f.releases[i])
				return
			}
		}
		http.NotFound(w, r)
	})
	mux.HandleFunc("/repos/o/r/releases", func(w http.ResponseWriter, r *http.Request) {
		var in map[string]any
		_ = json.NewDecoder(r.Body).Decode(&in)
		f.mu.Lock()
		defer f.mu.Unlock()
		f.nextID++
		rel := &patch.Release{ID: f.nextID, TagName: in["tag_name"].(string), Name: in["name"].(string), Body: in["body"].(string), Draft: in["draft"].(bool),
			HTMLURL: f.srv.URL + "/html/" + in["tag_name"].(string), UploadURL: fmt.Sprintf("%s/upload/%d/assets{?name,label}", f.srv.URL, f.nextID)}
		f.releases = append(f.releases, rel)
		w.WriteHeader(201)
		_ = json.NewEncoder(w).Encode(rel)
	})
	mux.HandleFunc("/repos/o/r/releases/", func(w http.ResponseWriter, r *http.Request) {
		id := strings.TrimPrefix(r.URL.Path, "/repos/o/r/releases/")
		f.mu.Lock()
		defer f.mu.Unlock()
		for i, rel := range f.releases {
			if fmt.Sprint(rel.ID) == id {
				if r.Method == http.MethodDelete {
					f.releases = append(f.releases[:i], f.releases[i+1:]...)
					w.WriteHeader(204)
					return
				}
				var in map[string]any
				_ = json.NewDecoder(r.Body).Decode(&in)
				if d, ok := in["draft"].(bool); ok {
					rel.Draft = d
				}
				_ = json.NewEncoder(w).Encode(rel)
				return
			}
		}
		http.NotFound(w, r)
	})
	mux.HandleFunc("/upload/", func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Authorization") != "Bearer test-token" {
			http.Error(w, "sem token", 401)
			return
		}
		parts := strings.Split(strings.TrimPrefix(r.URL.Path, "/upload/"), "/")
		name := r.URL.Query().Get("name")
		b, _ := io.ReadAll(r.Body)
		if int64(len(b)) != r.ContentLength {
			http.Error(w, "tamanho errado", 400)
			return
		}
		f.mu.Lock()
		defer f.mu.Unlock()
		for _, rel := range f.releases {
			if fmt.Sprint(rel.ID) == parts[0] {
				key := rel.TagName + "/" + name
				f.blobs[key] = b
				a := patch.Asset{ID: int64(len(f.blobs)), Name: name, Size: int64(len(b)), BrowserDownloadURL: f.srv.URL + "/dl/" + key}
				rel.Assets = append(rel.Assets, a)
				w.WriteHeader(201)
				_ = json.NewEncoder(w).Encode(a)
				return
			}
		}
		http.NotFound(w, r)
	})
	mux.HandleFunc("/dl/", func(w http.ResponseWriter, r *http.Request) {
		key := strings.TrimPrefix(r.URL.Path, "/dl/")
		f.mu.Lock()
		b, ok := f.blobs[key]
		f.downloads[key]++
		f.mu.Unlock()
		if !ok {
			http.NotFound(w, r)
			return
		}
		_, _ = w.Write(b)
	})
	f.srv = httptest.NewServer(mux)
	t.Cleanup(f.srv.Close)
	return f
}

func write(t *testing.T, root, rel, content string) {
	t.Helper()
	p := filepath.Join(root, filepath.FromSlash(rel))
	if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
}

func TestPublishInstallUpdate(t *testing.T) {
	fake := newFake(t)
	t.Setenv("NOCLIP_GITHUB_API", fake.srv.URL)
	t.Setenv("GITHUB_TOKEN", "test-token")

	// ---------- Versao 0.1.0
	build := t.TempDir()
	write(t, build, "NOCLIP.exe", "bootstrap")
	write(t, build, "NOCLIP/Binaries/Win64/NOCLIP.exe", strings.Repeat("codigo-v1 ", 5000))
	write(t, build, "NOCLIP/Content/Paks/NOCLIP-Windows.ucas", strings.Repeat("conteudo ", 20000))
	write(t, build, "Engine/Old.dll", "vai sair na 0.1.1")
	write(t, build, "NOCLIP/Saved/Logs/NOCLIP.log", "log do desenvolvedor: nao publica")
	write(t, build, "Manifest_UFSFiles_Win64.txt", "lista do UAT: nao publica")
	launcherExe := filepath.Join(t.TempDir(), "NOCLIP-Launcher.exe")
	write(t, filepath.Dir(launcherExe), "NOCLIP-Launcher.exe", "launcher 1.0.0")

	if err := run(build, "o/r", "0.1.0", "Primeira versao", launcherExe, "1.0.0", false, false, false); err != nil {
		t.Fatal(err)
	}
	c := patch.NewClient("o/r", "", "teste")
	rel, err := c.LatestRelease()
	if err != nil {
		t.Fatal(err)
	}
	m1, err := c.FetchManifest(rel)
	if err != nil {
		t.Fatal(err)
	}
	if m1.Version != "0.1.0" || len(m1.Files) != 4 {
		t.Fatalf("manifesto 0.1.0: versao %s, %d arquivos (esperado 4, sem Saved e Manifest_*)", m1.Version, len(m1.Files))
	}
	if m1.Launcher == nil || m1.Launcher.Version != "1.0.0" {
		t.Fatalf("launcher nao entrou no manifesto: %+v", m1.Launcher)
	}

	// ---------- Instalacao do zero (o que o launcher faz)
	game := t.TempDir()
	statePath := filepath.Join(t.TempDir(), "launcher.json")
	st := patch.LoadState(statePath)
	plan, err := patch.MakePlan(m1, game, st, false, nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(plan.Needed) != 4 {
		t.Fatalf("instalacao do zero deveria baixar 4 arquivos, plano tem %d", len(plan.Needed))
	}
	if err := patch.Apply(context.Background(), c, plan, game, st, statePath, nil); err != nil {
		t.Fatal(err)
	}
	st.Version = m1.Version
	_ = st.Save(statePath)
	if b, _ := os.ReadFile(filepath.Join(game, "Engine/Old.dll")); string(b) != "vai sair na 0.1.1" {
		t.Fatalf("arquivo instalado errado: %q", b)
	}
	// O jogador ja tem config/logs na pasta Saved: o launcher nunca mexe.
	write(t, game, "NOCLIP/Saved/Config/Windows/GameUserSettings.ini", "configs do jogador")

	// Abrir de novo sem mudar nada: nada a fazer (sem recalcular hash).
	st = patch.LoadState(statePath)
	plan, _ = patch.MakePlan(m1, game, st, false, nil)
	if !plan.UpToDate() {
		t.Fatalf("instalado e atualizado, mas o plano pede %d arquivos / apaga %d", len(plan.Needed), len(plan.Stale))
	}

	// ---------- Versao 0.1.1: muda o executavel, sai uma dll, entra outra; o .ucas grande fica igual.
	write(t, build, "NOCLIP/Binaries/Win64/NOCLIP.exe", strings.Repeat("codigo-v2 ", 5000))
	os.Remove(filepath.Join(build, "Engine/Old.dll"))
	write(t, build, "Engine/New.dll", "nova")
	if err := run(build, "o/r", "0.1.1", "- Corrigido o elevador", "", "", false, false, false); err != nil {
		t.Fatal(err)
	}
	rel2, _ := c.LatestRelease()
	m2, err := c.FetchManifest(rel2)
	if err != nil {
		t.Fatal(err)
	}
	uploaded := 0
	for _, a := range rel2.Assets {
		if strings.HasSuffix(a.Name, ".gz") {
			uploaded++
		}
	}
	if uploaded != 2 {
		t.Fatalf("a 0.1.1 deveria enviar so 2 arquivos (exe e New.dll), enviou %d", uploaded)
	}
	for _, f := range m2.Files {
		if f.Path == "NOCLIP/Content/Paks/NOCLIP-Windows.ucas" && !strings.Contains(f.URL, "/v0.1.0/") {
			t.Fatalf("o .ucas igual deveria apontar para a 0.1.0: %s", f.URL)
		}
	}

	// ---------- Atualizar a instalacao
	before := fake.downloads["v0.1.0/"+patch.AssetName(m1.ByPath()["NOCLIP/Content/Paks/NOCLIP-Windows.ucas"])]
	st = patch.LoadState(statePath)
	plan, _ = patch.MakePlan(m2, game, st, false, nil)
	if len(plan.Needed) != 2 || len(plan.Stale) != 1 || plan.Stale[0] != "Engine/Old.dll" {
		t.Fatalf("update: precisa %d (esperado 2), apaga %v (esperado Engine/Old.dll)", len(plan.Needed), plan.Stale)
	}
	var last patch.Progress
	if err := patch.Apply(context.Background(), c, plan, game, st, statePath, func(p patch.Progress) { last = p }); err != nil {
		t.Fatal(err)
	}
	if after := fake.downloads["v0.1.0/"+patch.AssetName(m1.ByPath()["NOCLIP/Content/Paks/NOCLIP-Windows.ucas"])]; after != before {
		t.Fatal("o update baixou de novo o .ucas que nao mudou")
	}
	if last.Done != last.Total || last.Total != plan.DownloadBytes {
		t.Fatalf("progresso final %d/%d (esperado %d)", last.Done, last.Total, plan.DownloadBytes)
	}
	if _, err := os.Stat(filepath.Join(game, "Engine/Old.dll")); !os.IsNotExist(err) {
		t.Fatal("Engine/Old.dll deveria ter sido apagado")
	}
	if b, _ := os.ReadFile(filepath.Join(game, "NOCLIP/Binaries/Win64/NOCLIP.exe")); !strings.HasPrefix(string(b), "codigo-v2") {
		t.Fatal("o executavel nao foi atualizado")
	}
	if b, _ := os.ReadFile(filepath.Join(game, "NOCLIP/Saved/Config/Windows/GameUserSettings.ini")); string(b) != "configs do jogador" {
		t.Fatal("o update mexeu na pasta Saved do jogador")
	}

	// ---------- Arquivo corrompido no disco: "Verificar arquivos" acha e baixa so ele.
	write(t, game, "Engine/New.dll", "nove") // mesmo tamanho, conteudo errado
	st = patch.LoadState(statePath)
	plan, _ = patch.MakePlan(m2, game, st, false, nil)
	if len(plan.Needed) != 1 {
		t.Fatalf("data mudou: deveria conferir e achar 1 arquivo, achou %d", len(plan.Needed))
	}
	plan, _ = patch.MakePlan(m2, game, st, true, nil)
	if len(plan.Needed) != 1 || plan.Needed[0].Path != "Engine/New.dll" {
		t.Fatalf("verificar deveria achar so Engine/New.dll: %+v", plan.Needed)
	}

	// ---------- Publicar a mesma versao de novo e recusado.
	if err := run(build, "o/r", "0.1.1", "", "", "", false, false, false); err == nil {
		t.Fatal("publicar a mesma versao duas vezes deveria falhar")
	}
}

func TestManifestPathsAreSafe(t *testing.T) {
	for _, bad := range []string{"../x", "a/../../b", "/abs", "C:/win", `a\b`, ""} {
		if _, err := patch.SafeJoin("/game", bad); err == nil {
			t.Errorf("SafeJoin aceitou %q", bad)
		}
	}
	if p, err := patch.SafeJoin("/game", "NOCLIP/Content/Paks/a.pak"); err != nil || !strings.HasSuffix(filepath.ToSlash(p), "/game/NOCLIP/Content/Paks/a.pak") {
		t.Errorf("SafeJoin recusou caminho valido: %v %v", p, err)
	}
}

func TestVersions(t *testing.T) {
	cases := []struct {
		a, b string
		want int
	}{{"1.0.0", "1.0.0", 0}, {"1.0.1", "1.0.0", 1}, {"1.10.0", "1.9.9", 1}, {"v0.2", "0.10", -1}, {"1.0", "1.0.0", 0}, {"1.0.0-beta", "1.0.0", 0}}
	for _, c := range cases {
		if got := patch.CompareVersions(c.a, c.b); got != c.want {
			t.Errorf("CompareVersions(%q, %q) = %d, esperado %d", c.a, c.b, got, c.want)
		}
	}
}
