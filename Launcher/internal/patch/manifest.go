// Copyright Fuly Games. All Rights Reserved.

// Package patch descreve uma versao do jogo publicada no GitHub Releases e
// o que precisa ser baixado para chegar nela.
//
// Cada release tem um manifest.json com todos os arquivos do jogo (caminho,
// tamanho e sha256). Cada arquivo e um asset .gz proprio; arquivos que nao
// mudaram entre versoes apontam para o asset da release antiga, entao o
// launcher so baixa o que mudou.
package patch

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// ManifestAsset e o nome do asset com o manifesto em cada release.
const ManifestAsset = "manifest.json"

// LauncherAsset e o nome do executavel do launcher em cada release.
const LauncherAsset = "NOCLIP-Launcher.exe"

// File e um arquivo do jogo instalado.
type File struct {
	Path   string `json:"path"`   // relativo a pasta do jogo, com "/"
	Size   int64  `json:"size"`   // tamanho descompactado
	SHA256 string `json:"sha256"` // do arquivo descompactado
	URL    string `json:"url"`    // asset .gz (pode ser de uma release anterior)
	GzSize int64  `json:"gzSize"` // tamanho do download
}

// Launcher descreve o executavel do launcher publicado junto da versao.
type Launcher struct {
	Version string `json:"version"`
	URL     string `json:"url"`
	SHA256  string `json:"sha256"`
	Size    int64  `json:"size"`
}

// Manifest e uma versao do jogo.
type Manifest struct {
	Game     string    `json:"game"`
	Version  string    `json:"version"`
	Tag      string    `json:"tag"`
	Created  string    `json:"created"`
	Exe      string    `json:"exe"`              // o que o launcher abre (relativo)
	Prereq   string    `json:"prereq,omitempty"` // instalador do Visual C++ do Unreal
	Launcher *Launcher `json:"launcher,omitempty"`
	Files    []File    `json:"files"`
}

// TotalSize soma o tamanho instalado.
func (m *Manifest) TotalSize() int64 {
	var n int64
	for _, f := range m.Files {
		n += f.Size
	}
	return n
}

// ByPath indexa os arquivos pelo caminho.
func (m *Manifest) ByPath() map[string]File {
	out := make(map[string]File, len(m.Files))
	for _, f := range m.Files {
		out[f.Path] = f
	}
	return out
}

// HashFile devolve o sha256 (hex) e o tamanho de um arquivo.
func HashFile(path string) (string, int64, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", 0, err
	}
	defer f.Close()
	h := sha256.New()
	n, err := io.Copy(h, f)
	if err != nil {
		return "", 0, err
	}
	return hex.EncodeToString(h.Sum(nil)), n, nil
}

// Skip diz se um caminho do build fica de fora da publicacao (e nunca e
// apagado pelo launcher): configuracoes, logs e crashes de quem joga.
func Skip(rel string) bool {
	parts := strings.Split(rel, "/")
	if len(parts) >= 2 && strings.EqualFold(parts[1], "Saved") {
		return true // NOCLIP/Saved, Engine/Saved
	}
	if strings.EqualFold(parts[0], "Saved") {
		return true
	}
	base := strings.ToLower(parts[len(parts)-1])
	return strings.HasSuffix(base, ".pdb") || strings.HasPrefix(base, "manifest_") || base == "thumbs.db" || base == "desktop.ini"
}

// Scan lista os arquivos de uma pasta de build (sem hash).
func Scan(root string) ([]string, error) {
	var out []string
	err := filepath.WalkDir(root, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		rel, err := filepath.Rel(root, p)
		if err != nil {
			return err
		}
		rel = filepath.ToSlash(rel)
		if !Skip(rel) {
			out = append(out, rel)
		}
		return nil
	})
	sort.Strings(out)
	return out, err
}

// SafeJoin junta root e um caminho do manifesto sem deixar sair da pasta.
func SafeJoin(root, rel string) (string, error) {
	if rel == "" || strings.Contains(rel, "\\") || strings.HasPrefix(rel, "/") || strings.Contains(rel, ":") {
		return "", fmt.Errorf("caminho invalido no manifesto: %q", rel)
	}
	for _, part := range strings.Split(rel, "/") {
		if part == ".." || part == "." || part == "" {
			return "", fmt.Errorf("caminho invalido no manifesto: %q", rel)
		}
	}
	return filepath.Join(root, filepath.FromSlash(rel)), nil
}

// CompareVersions compara "1.2.3" com "1.10.0" (numerico, ignora um "v" no
// comeco e sufixos depois de "-"). -1, 0 ou 1.
func CompareVersions(a, b string) int {
	pa, pb := parseVersion(a), parseVersion(b)
	for i := 0; i < len(pa) || i < len(pb); i++ {
		var x, y int
		if i < len(pa) {
			x = pa[i]
		}
		if i < len(pb) {
			y = pb[i]
		}
		if x != y {
			if x < y {
				return -1
			}
			return 1
		}
	}
	return 0
}

func parseVersion(v string) []int {
	v = strings.TrimPrefix(strings.TrimSpace(v), "v")
	if i := strings.IndexAny(v, "-+ "); i >= 0 {
		v = v[:i]
	}
	var out []int
	for _, p := range strings.Split(v, ".") {
		n, err := strconv.Atoi(p)
		if err != nil {
			n = 0
		}
		out = append(out, n)
	}
	return out
}

// AssetName monta um nome de asset aceito pelo GitHub para um arquivo do jogo.
func AssetName(f File) string {
	clean := strings.Map(func(r rune) rune {
		switch {
		case r >= 'a' && r <= 'z', r >= 'A' && r <= 'Z', r >= '0' && r <= '9', r == '.', r == '-', r == '_':
			return r
		case r == '/':
			return '.'
		}
		return '_'
	}, f.Path)
	return f.SHA256[:12] + "_" + clean + ".gz"
}

// ErrNoRelease: o repositorio ainda nao tem nenhuma versao publicada.
var ErrNoRelease = errors.New("nenhuma versao publicada ainda")
