// Copyright Fuly Games. All Rights Reserved.

//go:build !windows

// Fora do Windows o launcher nao tem janela: so serve para rodar os testes.
package main

import (
	"fmt"
	"os"
)

func main() {
	fmt.Println("O NOCLIP-Launcher roda no Windows.")
	os.Exit(1)
}

func steamRunning() bool   { return true }
func openPath(string)      {}
func exitAfterSelfUpdate() { os.Exit(0) }
