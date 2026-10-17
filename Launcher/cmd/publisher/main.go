// Copyright Fuly Games. All Rights Reserved.

// NOCLIP-Publisher: publica um build empacotado do NOCLIP no GitHub Releases
// para o launcher baixar.
//
//	NOCLIP-Publisher.exe -build "F:\Dev\NOCLIP\Saved\Playtest\Development\Windows" -version 0.1.1 -notes notas.md
//
// So os arquivos que mudaram desde a ultima release sao enviados; os outros
// continuam apontando para o asset da release antiga (por isso nao apague
// releases antigas no GitHub; se apagar, publique com -full).
//
// Token: variavel GITHUB_TOKEN, ou o login do GitHub CLI (gh auth login).
package main

import (
	"bytes"
	"compress/gzip"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/ricardofuly/NoClip/Launcher/internal/patch"
)

func main() {
	var (
		build       = flag.String("build", "", "pasta do build empacotado (a que tem NOCLIP.exe)")
		repo        = flag.String("repo", "ricardofuly/NoClip", "repositorio dono/nome")
		version     = flag.String("version", "", "versao (ex.: 0.1.1)")
		notesPath   = flag.String("notes", "", "arquivo .md/.txt com as notas da versao (opcional)")
		notesText   = flag.String("notes-text", "", "notas da versao direto na linha de comando")
		launcher    = flag.String("launcher", "", "NOCLIP-Launcher.exe a publicar junto (opcional)")
		launcherVer = flag.String("launcher-version", "", "versao do launcher publicado (padrao: a que ele informa)")
		prerelease  = flag.Bool("prerelease", false, "marca como pre-release (o launcher ignora)")
		full        = flag.Bool("full", false, "envia todos os arquivos (nao reaproveita releases antigas)")
		dryRun      = flag.Bool("dry-run", false, "so mostra o que seria enviado")
	)
	flag.Parse()
	if *build == "" || *version == "" {
		fmt.Fprintln(os.Stderr, "uso: NOCLIP-Publisher -build <pasta do build> -version <x.y.z> [-notes notas.md] [-launcher NOCLIP-Launcher.exe]")
		os.Exit(2)
	}
	if err := run(*build, *repo, strings.TrimPrefix(*version, "v"), readNotes(*notesPath, *notesText), *launcher, *launcherVer, *prerelease, *full, *dryRun); err != nil {
		fmt.Fprintln(os.Stderr, "\nERRO:", err)
		os.Exit(1)
	}
}

func readNotes(path, text string) string {
	if text != "" {
		return text
	}
	if path == "" {
		return ""
	}
	b, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	return string(b)
}

func token() string {
	if t := strings.TrimSpace(os.Getenv("GITHUB_TOKEN")); t != "" {
		return t
	}
	if t := strings.TrimSpace(os.Getenv("GH_TOKEN")); t != "" {
		return t
	}
	out, err := exec.Command("gh", "auth", "token").Output()
	if err == nil {
		return strings.TrimSpace(string(out))
	}
	return ""
}

func run(buildDir, repo, version, notes, launcherPath, launcherVer string, prerelease, full, dryRun bool) error {
	if _, err := os.Stat(filepath.Join(buildDir, "NOCLIP.exe")); err != nil {
		return fmt.Errorf("NOCLIP.exe nao esta em %s (rode o PackagePlaytest.bat antes)", buildDir)
	}
	tok := token()
	if tok == "" && !dryRun {
		return errors.New("sem token do GitHub: rode 'gh auth login' (GitHub CLI) ou defina GITHUB_TOKEN")
	}
	c := &client{patch.NewClient(repo, tok, "NOCLIP-Publisher")}
	tag := "v" + version

	// 1. O que ja esta publicado (para reaproveitar arquivos iguais).
	reuse := map[string]patch.File{}
	if !full {
		if latest, err := c.LatestRelease(); err == nil {
			if latest.TagName == tag {
				return fmt.Errorf("a versao %s ja foi publicada; use outro numero", tag)
			}
			if old, err := c.FetchManifest(latest); err == nil {
				for _, f := range old.Files {
					reuse[f.SHA256] = f
				}
				fmt.Printf("Ultima versao publicada: %s (%d arquivos)\n", old.Version, len(old.Files))
			}
		} else if !errors.Is(err, patch.ErrNoRelease) {
			return fmt.Errorf("lendo a ultima release: %w", err)
		} else {
			fmt.Println("Primeira versao publicada neste repositorio.")
		}
	}

	// 2. Arquivos do build.
	rels, err := patch.Scan(buildDir)
	if err != nil {
		return err
	}
	m := &patch.Manifest{Game: "NOCLIP", Version: version, Tag: tag, Created: time.Now().UTC().Format(time.RFC3339), Exe: "NOCLIP.exe"}
	if _, err := os.Stat(filepath.Join(buildDir, "Engine", "Extras", "Redist", "en-us", "UEPrereqSetup_x64.exe")); err == nil {
		m.Prereq = "Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe"
	}
	var upload []patch.File
	var reusedBytes, uploadBytes int64
	fmt.Printf("Conferindo %d arquivos...\n", len(rels))
	for _, rel := range rels {
		sum, size, err := patch.HashFile(filepath.Join(buildDir, filepath.FromSlash(rel)))
		if err != nil {
			return err
		}
		f := patch.File{Path: rel, Size: size, SHA256: sum}
		if old, ok := reuse[sum]; ok && old.URL != "" {
			f.URL, f.GzSize = old.URL, old.GzSize
			reusedBytes += size
		} else {
			upload = append(upload, f)
			uploadBytes += size
		}
		m.Files = append(m.Files, f)
	}
	fmt.Printf("Versao %s: %d arquivos (%s). Novos/alterados: %d (%s). Reaproveitados: %s.\n",
		version, len(m.Files), human(m.TotalSize()), len(upload), human(uploadBytes), human(reusedBytes))
	for _, f := range upload {
		fmt.Printf("  + %s (%s)\n", f.Path, human(f.Size))
	}
	if dryRun {
		return nil
	}

	// 3. Release em rascunho (so aparece para o launcher quando terminar).
	body := notes
	if strings.TrimSpace(body) == "" {
		body = "Nova versao do NOCLIP."
	}
	body += "\n\n---\nBaixe o **NOCLIP-Launcher.exe** abaixo e abra: ele instala o jogo e baixa as proximas versoes sozinho (so o que mudou)."
	var rel patch.Release
	if err := c.postJSON("/repos/"+repo+"/releases", map[string]any{
		"tag_name": tag, "name": "NOCLIP " + version, "body": body, "draft": true, "prerelease": prerelease,
	}, &rel); err != nil {
		return fmt.Errorf("criando a release (o repositorio precisa ter pelo menos um commit): %w", err)
	}
	fmt.Printf("Release %s criada (rascunho). Enviando...\n", tag)
	cleanup := func() { _ = c.delete("/repos/" + repo + "/releases/" + fmt.Sprint(rel.ID)) }

	// 4. Arquivos novos, compactados.
	byPath := map[string]int{}
	for i, f := range m.Files {
		byPath[f.Path] = i
	}
	for i, f := range upload {
		fmt.Printf("[%d/%d] %s ", i+1, len(upload), f.Path)
		a, gzSize, err := c.uploadGz(&rel, patch.AssetName(f), filepath.Join(buildDir, filepath.FromSlash(f.Path)))
		if err != nil {
			cleanup()
			return fmt.Errorf("enviando %s: %w", f.Path, err)
		}
		fmt.Printf("-> %s\n", human(gzSize))
		idx := byPath[f.Path]
		m.Files[idx].URL = a.BrowserDownloadURL
		m.Files[idx].GzSize = gzSize
	}

	// 5. Launcher (sempre na release, para quem vai instalar pela primeira vez).
	if launcherPath != "" {
		sum, size, err := patch.HashFile(launcherPath)
		if err != nil {
			cleanup()
			return err
		}
		lv := launcherVer
		if lv == "" {
			if out, err := exec.Command(launcherPath, "--version").Output(); err == nil {
				lv = strings.TrimSpace(string(out))
			}
		}
		b, err := os.ReadFile(launcherPath)
		if err != nil {
			cleanup()
			return err
		}
		a, err := c.uploadBytes(&rel, patch.LauncherAsset, "application/vnd.microsoft.portable-executable", b)
		if err != nil {
			cleanup()
			return fmt.Errorf("enviando o launcher: %w", err)
		}
		m.Launcher = &patch.Launcher{Version: lv, URL: a.BrowserDownloadURL, SHA256: sum, Size: size}
		fmt.Printf("Launcher %s enviado.\n", lv)
	}

	// 6. Manifesto e publicar.
	mb, _ := json.MarshalIndent(m, "", "  ")
	if _, err := c.uploadBytes(&rel, patch.ManifestAsset, "application/json", mb); err != nil {
		cleanup()
		return fmt.Errorf("enviando o manifesto: %w", err)
	}
	if err := c.patchJSON("/repos/"+repo+"/releases/"+fmt.Sprint(rel.ID), map[string]any{"draft": false}, &rel); err != nil {
		return fmt.Errorf("publicando (a release ficou como rascunho no GitHub): %w", err)
	}
	fmt.Printf("\nPublicado: %s\nQuem abrir o launcher agora recebe a versao %s.\n", rel.HTMLURL, version)
	return nil
}

// ---------------------------------------------------------------- API de escrita

type client struct{ *patch.Client }

func (c *client) do(method, path string, in, out any) error {
	var body io.Reader
	if in != nil {
		b, _ := json.Marshal(in)
		body = bytes.NewReader(b)
	}
	req, err := http.NewRequest(method, c.API+path, body)
	if err != nil {
		return err
	}
	req.Header.Set("User-Agent", c.Agent)
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("X-GitHub-Api-Version", "2022-11-28")
	req.Header.Set("Authorization", "Bearer "+c.Token)
	if in != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	resp, err := c.HTTP.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode >= 300 {
		msg, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
		return fmt.Errorf("GitHub %d: %s", resp.StatusCode, strings.TrimSpace(string(msg)))
	}
	if out != nil {
		return json.NewDecoder(resp.Body).Decode(out)
	}
	return nil
}

func (c *client) postJSON(path string, in, out any) error {
	return c.do(http.MethodPost, path, in, out)
}
func (c *client) patchJSON(path string, in, out any) error {
	return c.do(http.MethodPatch, path, in, out)
}
func (c *client) delete(path string) error { return c.do(http.MethodDelete, path, nil, nil) }

func uploadBase(rel *patch.Release, name string) string {
	u := rel.UploadURL
	if i := strings.Index(u, "{"); i >= 0 {
		u = u[:i]
	}
	return u + "?name=" + url.QueryEscape(name)
}

// upload envia um asset; open e chamado de novo a cada tentativa (o cliente HTTP fecha o corpo).
func (c *client) upload(rel *patch.Release, name, contentType string, open func() (io.ReadCloser, error), size int64) (*patch.Asset, error) {
	var lastErr error
	for attempt := 0; attempt < 3; attempt++ {
		body, err := open()
		if err != nil {
			return nil, err
		}
		req, err := http.NewRequest(http.MethodPost, uploadBase(rel, name), body)
		if err != nil {
			body.Close()
			return nil, err
		}
		req.ContentLength = size
		req.Header.Set("User-Agent", c.Agent)
		req.Header.Set("Accept", "application/vnd.github+json")
		req.Header.Set("Authorization", "Bearer "+c.Token)
		req.Header.Set("Content-Type", contentType)
		resp, err := c.HTTP.Do(req)
		if err != nil {
			lastErr = err
			time.Sleep(3 * time.Second)
			continue
		}
		if resp.StatusCode >= 300 {
			msg, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
			resp.Body.Close()
			lastErr = fmt.Errorf("GitHub %d: %s", resp.StatusCode, strings.TrimSpace(string(msg)))
			if resp.StatusCode == 422 { // ja existe com esse nome: nao adianta repetir
				return nil, lastErr
			}
			time.Sleep(3 * time.Second)
			continue
		}
		var a patch.Asset
		err = json.NewDecoder(resp.Body).Decode(&a)
		resp.Body.Close()
		return &a, err
	}
	return nil, lastErr
}

func (c *client) uploadBytes(rel *patch.Release, name, contentType string, b []byte) (*patch.Asset, error) {
	return c.upload(rel, name, contentType, func() (io.ReadCloser, error) { return io.NopCloser(bytes.NewReader(b)), nil }, int64(len(b)))
}

// uploadGz compacta para um arquivo temporario (o GitHub exige o tamanho antes) e envia.
func (c *client) uploadGz(rel *patch.Release, name, src string) (*patch.Asset, int64, error) {
	in, err := os.Open(src)
	if err != nil {
		return nil, 0, err
	}
	defer in.Close()
	tmp, err := os.CreateTemp("", "noclip-*.gz")
	if err != nil {
		return nil, 0, err
	}
	tmpName := tmp.Name()
	defer os.Remove(tmpName)
	gz, _ := gzip.NewWriterLevel(tmp, gzip.BestCompression)
	if _, err := io.Copy(gz, in); err != nil {
		tmp.Close()
		return nil, 0, err
	}
	if err := gz.Close(); err != nil {
		tmp.Close()
		return nil, 0, err
	}
	size, _ := tmp.Seek(0, io.SeekEnd)
	tmp.Close()
	a, err := c.upload(rel, name, "application/gzip", func() (io.ReadCloser, error) { return os.Open(tmpName) }, size)
	return a, size, err
}

func human(n int64) string {
	switch {
	case n >= 1<<30:
		return fmt.Sprintf("%.2f GB", float64(n)/(1<<30))
	case n >= 1<<20:
		return fmt.Sprintf("%.1f MB", float64(n)/(1<<20))
	case n >= 1<<10:
		return fmt.Sprintf("%.0f KB", float64(n)/(1<<10))
	}
	return fmt.Sprintf("%d B", n)
}
