module github.com/ricardofuly/NoClip/Launcher

go 1.24

// golang.org nao responde em algumas redes de build; o espelho oficial no GitHub e o mesmo codigo.
replace golang.org/x/sys => github.com/golang/sys v0.30.0

require (
	github.com/jchv/go-webview2 v0.0.0-20260205173254-56598839c808
	golang.org/x/sys v0.30.0
)

require github.com/jchv/go-winloader v0.0.0-20250406163304-c1995be93bd1 // indirect
