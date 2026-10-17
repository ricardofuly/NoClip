// Copyright Fuly Games. All Rights Reserved.

// NOCLIP-Launcher: instala o NOCLIP e mantem o jogo atualizado pelo GitHub
// Releases (baixa so os arquivos que mudaram entre as versoes).
package main

import (
	_ "embed"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
	"unsafe"

	webview2 "github.com/jchv/go-webview2"
	"golang.org/x/sys/windows"
)

//go:embed ui.html
var uiHTML string

var mainView webview2.WebView

func main() {
	for _, a := range os.Args[1:] {
		if a == "--version" || a == "-version" {
			fmt.Println(Version)
			return
		}
	}
	cleanupOldExe()

	// Um launcher aberto por vez (o segundo so traz o primeiro para frente).
	name, _ := windows.UTF16PtrFromString("Local\\NOCLIP-Launcher")
	mutex, err := windows.CreateMutex(nil, false, name)
	if err == windows.ERROR_ALREADY_EXISTS {
		if !waitForPrevious(os.Args) {
			focusExisting()
			return
		}
	}
	if mutex != 0 {
		defer windows.CloseHandle(mutex)
	}

	var app *App
	var view webview2.WebView
	push := func(s UIState) {
		if view == nil {
			return
		}
		b, _ := json.Marshal(s)
		js := "window.onState && window.onState(" + string(b) + ")"
		view.Dispatch(func() { view.Eval(js) })
	}
	app = NewApp(push)

	dataDir := filepath.Join(app.baseDir, "WebView2")
	view = webview2.NewWithOptions(webview2.WebViewOptions{
		Debug:     os.Getenv("NOCLIP_LAUNCHER_DEBUG") == "1",
		AutoFocus: true,
		DataPath:  dataDir,
		WindowOptions: webview2.WindowOptions{
			Title:  "NOCLIP",
			Width:  980,
			Height: 620,
			IconId: 2, // rsrc: manifesto = 1, grupo do icone = 2
			Center: true,
		},
	})
	if view == nil {
		messageBox("NOCLIP",
			"Este launcher precisa do Microsoft Edge WebView2 (já vem no Windows 11).\n\n"+
				"Instale em: https://go.microsoft.com/fwlink/p/?LinkId=2124703\n\nDepois abra o launcher de novo.")
		_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", "https://go.microsoft.com/fwlink/p/?LinkId=2124703").Start()
		return
	}
	mainView = view
	defer view.Destroy()
	view.SetSize(980, 620, webview2.HintFixed)

	_ = view.Bind("noclipState", func() string { return app.StateJSON() })
	_ = view.Bind("noclipAction", func(name string) { app.Action(name) })

	view.SetHtml(uiHTML)
	go func() {
		time.Sleep(300 * time.Millisecond)
		app.Check()
	}()
	// Steam aberta / fechada aparece sozinho na tela.
	go func() {
		last := steamRunning()
		for range time.Tick(3 * time.Second) {
			if now := steamRunning(); now != last {
				last = now
				push(app.Snapshot())
			}
		}
	}()
	view.Run()
}

// waitForPrevious: logo depois de um auto-update o launcher antigo ainda esta
// fechando; espera ele sair em vez de desistir.
func waitForPrevious(args []string) bool {
	after := false
	for _, a := range args[1:] {
		after = after || a == "--after-update"
	}
	if !after {
		return false
	}
	name, _ := windows.UTF16PtrFromString("Local\\NOCLIP-Launcher")
	for i := 0; i < 50; i++ {
		time.Sleep(100 * time.Millisecond)
		h, err := windows.CreateMutex(nil, false, name)
		if err != windows.ERROR_ALREADY_EXISTS {
			if h != 0 {
				windows.CloseHandle(h)
			}
			return true
		}
		windows.CloseHandle(h)
	}
	return true
}

func exitAfterSelfUpdate() {
	if mainView != nil {
		mainView.Dispatch(func() { mainView.Terminate() })
		time.Sleep(500 * time.Millisecond)
	}
	os.Exit(0)
}

var (
	user32         = windows.NewLazySystemDLL("user32.dll")
	procMessageBox = user32.NewProc("MessageBoxW")
	procFindWindow = user32.NewProc("FindWindowW")
	procSetFg      = user32.NewProc("SetForegroundWindow")
	procShowWindow = user32.NewProc("ShowWindow")
)

func messageBox(title, text string) {
	t, _ := windows.UTF16PtrFromString(title)
	m, _ := windows.UTF16PtrFromString(text)
	procMessageBox.Call(0, uintptr(unsafe.Pointer(m)), uintptr(unsafe.Pointer(t)), 0x40)
}

func focusExisting() {
	cls, _ := windows.UTF16PtrFromString("webview")
	title, _ := windows.UTF16PtrFromString("NOCLIP")
	if h, _, _ := procFindWindow.Call(uintptr(unsafe.Pointer(cls)), uintptr(unsafe.Pointer(title))); h != 0 {
		procShowWindow.Call(h, 9) // SW_RESTORE
		procSetFg.Call(h)
	}
}

// steamRunning procura o steam.exe entre os processos.
func steamRunning() bool {
	snap, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return true // sem como saber: nao incomoda
	}
	defer windows.CloseHandle(snap)
	var e windows.ProcessEntry32
	e.Size = uint32(unsafe.Sizeof(e))
	for err = windows.Process32First(snap, &e); err == nil; err = windows.Process32Next(snap, &e) {
		if strings.EqualFold(windows.UTF16ToString(e.ExeFile[:]), "steam.exe") {
			return true
		}
	}
	return false
}

// openPath abre uma pasta no Explorer ou um link no navegador.
func openPath(p string) {
	if strings.HasPrefix(p, "https://") {
		_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", p).Start()
		return
	}
	_ = exec.Command("explorer", p).Start()
}
