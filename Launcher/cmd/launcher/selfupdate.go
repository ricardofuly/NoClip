// Copyright Fuly Games. All Rights Reserved.

package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/ricardofuly/NoClip/Launcher/internal/patch"
)

// selfUpdate baixa o launcher novo, troca o executavel e abre o novo.
// No Windows nao da para sobrescrever um .exe aberto, mas da para renomear:
// o atual vira .old (apagado na proxima abertura) e o novo assume o nome.
func selfUpdate(c *patch.Client, l *patch.Launcher) error {
	exe, err := os.Executable()
	if err != nil {
		return err
	}
	exe, _ = filepath.EvalSymlinks(exe)
	resp, err := c.Open(l.URL)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	newPath := exe + ".new"
	out, err := os.Create(newPath)
	if err != nil {
		return err
	}
	h := sha256.New()
	_, err = io.Copy(io.MultiWriter(out, h), resp.Body)
	if cerr := out.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		os.Remove(newPath)
		return err
	}
	if l.SHA256 != "" && hex.EncodeToString(h.Sum(nil)) != l.SHA256 {
		os.Remove(newPath)
		return fmt.Errorf("o launcher baixado veio corrompido")
	}
	oldPath := exe + ".old"
	os.Remove(oldPath)
	if err := os.Rename(exe, oldPath); err != nil {
		os.Remove(newPath)
		return err
	}
	if err := os.Rename(newPath, exe); err != nil {
		_ = os.Rename(oldPath, exe)
		return err
	}
	cmd := exec.Command(exe, "--after-update")
	if err := cmd.Start(); err != nil {
		return err
	}
	exitAfterSelfUpdate()
	return nil
}

// cleanupOldExe apaga o executavel antigo deixado por um auto-update.
func cleanupOldExe() {
	if exe, err := os.Executable(); err == nil {
		os.Remove(exe + ".old")
		os.Remove(exe + ".new")
	}
}
