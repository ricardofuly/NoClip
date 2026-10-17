// Copyright Fuly Games. All Rights Reserved.

package patch

import (
	"compress/gzip"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"sync"
	"sync/atomic"
	"time"
)

// FileState e o que o launcher lembra de um arquivo instalado (para nao
// recalcular o hash de 600 MB toda vez que abre).
type FileState struct {
	Size    int64  `json:"size"`
	SHA256  string `json:"sha256"`
	ModTime int64  `json:"modTime"`
}

// State e o estado da instalacao (launcher.json).
type State struct {
	Version    string               `json:"version"`
	Files      map[string]FileState `json:"files"`
	PrereqDone bool                 `json:"prereqDone"`
	mu         sync.Mutex
}

// LoadState le o estado (vazio se nao existir ou estiver corrompido).
func LoadState(path string) *State {
	st := &State{Files: map[string]FileState{}}
	b, err := os.ReadFile(path)
	if err == nil {
		_ = json.Unmarshal(b, st)
	}
	if st.Files == nil {
		st.Files = map[string]FileState{}
	}
	return st
}

// Save grava o estado de forma atomica.
func (s *State) Save(path string) error {
	s.mu.Lock()
	b, err := json.MarshalIndent(s, "", "  ")
	s.mu.Unlock()
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, b, 0o644); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

func (s *State) set(rel string, fs FileState) {
	s.mu.Lock()
	s.Files[rel] = fs
	s.mu.Unlock()
}

// Plan e o que falta para a pasta do jogo bater com o manifesto.
type Plan struct {
	Needed        []File   // baixar
	Stale         []string // apagar (eram do jogo e sairam da versao nova)
	DownloadBytes int64
}

// UpToDate: nada para baixar nem apagar.
func (p *Plan) UpToDate() bool { return len(p.Needed) == 0 && len(p.Stale) == 0 }

// MakePlan compara a pasta com o manifesto. verify=true recalcula o hash de
// tudo (botao "Verificar arquivos"); senao confia no tamanho + data salvos.
// hashed e chamado a cada arquivo conferido (para a barra de progresso).
func MakePlan(m *Manifest, gameDir string, st *State, verify bool, hashed func(done, total int)) (*Plan, error) {
	plan := &Plan{}
	for i, f := range m.Files {
		if hashed != nil {
			hashed(i, len(m.Files))
		}
		dst, err := SafeJoin(gameDir, f.Path)
		if err != nil {
			return nil, err
		}
		info, err := os.Stat(dst)
		if err != nil || info.IsDir() || info.Size() != f.Size {
			plan.Needed = append(plan.Needed, f)
			continue
		}
		known, ok := st.Files[f.Path]
		if !verify && ok && known.SHA256 == f.SHA256 && known.Size == info.Size() && known.ModTime == info.ModTime().UnixNano() {
			continue
		}
		sum, _, err := HashFile(dst)
		if err != nil || sum != f.SHA256 {
			plan.Needed = append(plan.Needed, f)
			continue
		}
		st.set(f.Path, FileState{Size: info.Size(), SHA256: sum, ModTime: info.ModTime().UnixNano()})
	}
	inNew := m.ByPath()
	for rel := range st.Files {
		if _, ok := inNew[rel]; !ok && !Skip(rel) {
			plan.Stale = append(plan.Stale, rel)
		}
	}
	sort.Strings(plan.Stale)
	for _, f := range plan.Needed {
		plan.DownloadBytes += f.GzSize
	}
	return plan, nil
}

// Progress de um update.
type Progress struct {
	Done, Total int64 // bytes baixados (compactados)
	Files       int   // arquivos prontos
	FileCount   int
	Current     string
	Speed       float64 // bytes/s
}

type countingReader struct {
	r     io.Reader
	total *int64 // todos os downloads
	mine  *int64 // so esta tentativa (desfeito se falhar)
}

func (c countingReader) Read(p []byte) (int, error) {
	n, err := c.r.Read(p)
	atomic.AddInt64(c.total, int64(n))
	atomic.AddInt64(c.mine, int64(n))
	return n, err
}

// Apply baixa o que o plano pede (3 ao mesmo tempo), confere o sha256 de
// cada arquivo e so entao troca o arquivo do jogo. O estado e salvo a cada
// arquivo: se a internet cair, o proximo update continua de onde parou.
func Apply(ctx context.Context, c *Client, plan *Plan, gameDir string, st *State, statePath string, onProgress func(Progress)) error {
	tmpDir := filepath.Join(gameDir, ".noclip-download")
	if err := os.MkdirAll(tmpDir, 0o755); err != nil {
		return err
	}
	defer os.RemoveAll(tmpDir)

	// Apaga o que saiu da versao.
	for _, rel := range plan.Stale {
		if p, err := SafeJoin(gameDir, rel); err == nil {
			if err := os.Remove(p); err == nil || errors.Is(err, os.ErrNotExist) {
				st.mu.Lock()
				delete(st.Files, rel)
				st.mu.Unlock()
			}
		}
	}

	var done int64
	var files int32
	total := plan.DownloadBytes
	jobs := make(chan File)
	errCh := make(chan error, len(plan.Needed)+1)
	var wg sync.WaitGroup
	var current atomic.Value
	current.Store("")

	worker := func() {
		defer wg.Done()
		for f := range jobs {
			current.Store(f.Path)
			var err error
			for attempt := 0; attempt < 4; attempt++ {
				if ctx.Err() != nil {
					err = ctx.Err()
					break
				}
				var mine int64
				err = fetchOne(ctx, c, f, gameDir, tmpDir, &done, &mine, st)
				if err == nil {
					break
				}
				atomic.AddInt64(&done, -atomic.LoadInt64(&mine)) // desfaz a contagem da tentativa
				time.Sleep(time.Duration(attempt+1) * 2 * time.Second)
			}
			if err != nil {
				errCh <- fmt.Errorf("%s: %w", f.Path, err)
				continue
			}
			atomic.AddInt32(&files, 1)
			_ = st.Save(statePath)
		}
	}
	for i := 0; i < 3; i++ {
		wg.Add(1)
		go worker()
	}

	stop := make(chan struct{})
	go func() {
		t := time.NewTicker(150 * time.Millisecond)
		defer t.Stop()
		last, lastT := int64(0), time.Now()
		speed := 0.0
		for {
			select {
			case <-stop:
				return
			case now := <-t.C:
				d := atomic.LoadInt64(&done)
				if dt := now.Sub(lastT).Seconds(); dt >= 1 {
					inst := float64(d-last) / dt
					speed = speed*0.6 + inst*0.4
					last, lastT = d, now
				}
				if onProgress != nil {
					onProgress(Progress{Done: d, Total: total, Files: int(atomic.LoadInt32(&files)), FileCount: len(plan.Needed), Current: current.Load().(string), Speed: speed})
				}
			}
		}
	}()

feed:
	for _, f := range plan.Needed {
		select {
		case jobs <- f:
		case <-ctx.Done():
			break feed
		}
	}
	close(jobs)
	wg.Wait()
	close(stop)
	close(errCh)
	var errs []error
	for e := range errCh {
		errs = append(errs, e)
	}
	if len(errs) > 0 {
		return errors.Join(errs...)
	}
	if onProgress != nil {
		onProgress(Progress{Done: total, Total: total, Files: len(plan.Needed), FileCount: len(plan.Needed)})
	}
	return ctx.Err()
}

func fetchOne(ctx context.Context, c *Client, f File, gameDir, tmpDir string, done, mine *int64, st *State) error {
	dst, err := SafeJoin(gameDir, f.Path)
	if err != nil {
		return err
	}
	resp, err := c.Open(f.URL)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	gz, err := gzip.NewReader(countingReader{r: resp.Body, total: done, mine: mine})
	if err != nil {
		return err
	}
	defer gz.Close()

	tmp := filepath.Join(tmpDir, f.SHA256+".part")
	out, err := os.Create(tmp)
	if err != nil {
		return err
	}
	h := sha256.New()
	n, err := io.Copy(io.MultiWriter(out, h), ctxReader{ctx: ctx, r: gz})
	if cerr := out.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		os.Remove(tmp)
		return err
	}
	if sum := hex.EncodeToString(h.Sum(nil)); sum != f.SHA256 || n != f.Size {
		os.Remove(tmp)
		return fmt.Errorf("arquivo veio corrompido (sha %s..., %d bytes)", sum[:8], n)
	}
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	if err := os.Rename(tmp, dst); err != nil {
		os.Remove(tmp)
		return fmt.Errorf("nao deu para trocar o arquivo (o jogo esta aberto?): %w", err)
	}
	info, err := os.Stat(dst)
	if err != nil {
		return err
	}
	st.set(f.Path, FileState{Size: info.Size(), SHA256: f.SHA256, ModTime: info.ModTime().UnixNano()})
	return nil
}

type ctxReader struct {
	ctx context.Context
	r   io.Reader
}

func (c ctxReader) Read(p []byte) (int, error) {
	if err := c.ctx.Err(); err != nil {
		return 0, err
	}
	return c.r.Read(p)
}
