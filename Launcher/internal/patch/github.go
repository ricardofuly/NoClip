// Copyright Fuly Games. All Rights Reserved.

package patch

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

// Release e o pedaco da resposta da API do GitHub que interessa.
type Release struct {
	ID      int64   `json:"id"`
	TagName string  `json:"tag_name"`
	Name    string  `json:"name"`
	Body    string  `json:"body"`
	Draft   bool    `json:"draft"`
	HTMLURL string  `json:"html_url"`
	Assets  []Asset `json:"assets"`
	// Usado pelo publicador.
	UploadURL string `json:"upload_url"`
}

// Asset de uma release.
type Asset struct {
	ID                 int64  `json:"id"`
	Name               string `json:"name"`
	Size               int64  `json:"size"`
	BrowserDownloadURL string `json:"browser_download_url"`
}

// Find devolve o asset com este nome.
func (r *Release) Find(name string) *Asset {
	for i := range r.Assets {
		if r.Assets[i].Name == name {
			return &r.Assets[i]
		}
	}
	return nil
}

// Client fala com a API do GitHub (o token e opcional: o launcher le releases publicas sem token).
type Client struct {
	Repo  string // "dono/repo"
	Token string
	API   string // "https://api.github.com" (trocado nos testes)
	HTTP  *http.Client
	Agent string
}

// NewClient com tempos razoaveis para downloads grandes.
func NewClient(repo, token, agent string) *Client {
	api := "https://api.github.com"
	if v := os.Getenv("NOCLIP_GITHUB_API"); v != "" { // testes / GitHub Enterprise
		api = strings.TrimRight(v, "/")
	}
	return &Client{
		Repo:  repo,
		Token: token,
		API:   api,
		HTTP:  &http.Client{Timeout: 0, Transport: &http.Transport{Proxy: http.ProxyFromEnvironment, ResponseHeaderTimeout: 60 * time.Second, IdleConnTimeout: 90 * time.Second}},
		Agent: agent,
	}
}

func (c *Client) newRequest(method, url string, body io.Reader) (*http.Request, error) {
	req, err := http.NewRequest(method, url, body)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", c.Agent)
	if strings.HasPrefix(url, c.API) || strings.Contains(url, "uploads.github.com") {
		req.Header.Set("Accept", "application/vnd.github+json")
		req.Header.Set("X-GitHub-Api-Version", "2022-11-28")
		if c.Token != "" {
			req.Header.Set("Authorization", "Bearer "+c.Token)
		}
	}
	return req, nil
}

// GetJSON faz um GET na API e decodifica.
func (c *Client) GetJSON(path string, out any) (int, error) {
	req, err := c.newRequest(http.MethodGet, c.API+path, nil)
	if err != nil {
		return 0, err
	}
	resp, err := c.HTTP.Do(req)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		msg, _ := io.ReadAll(io.LimitReader(resp.Body, 2048))
		return resp.StatusCode, fmt.Errorf("GitHub respondeu %d: %s", resp.StatusCode, strings.TrimSpace(string(msg)))
	}
	return resp.StatusCode, json.NewDecoder(resp.Body).Decode(out)
}

// LatestRelease devolve a ultima release publicada (sem rascunhos e pre-releases).
func (c *Client) LatestRelease() (*Release, error) {
	var r Release
	status, err := c.GetJSON("/repos/"+c.Repo+"/releases/latest", &r)
	if status == http.StatusNotFound {
		return nil, ErrNoRelease
	}
	if err != nil {
		return nil, err
	}
	return &r, nil
}

// Open abre um download (segue o redirecionamento do GitHub para o armazenamento).
func (c *Client) Open(url string) (*http.Response, error) {
	req, err := c.newRequest(http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/octet-stream")
	resp, err := c.HTTP.Do(req)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != http.StatusOK {
		resp.Body.Close()
		return nil, fmt.Errorf("download %s: HTTP %d", url, resp.StatusCode)
	}
	return resp, nil
}

// FetchManifest baixa e le o manifest.json de uma release.
func (c *Client) FetchManifest(r *Release) (*Manifest, error) {
	a := r.Find(ManifestAsset)
	if a == nil {
		return nil, fmt.Errorf("a release %s nao tem %s", r.TagName, ManifestAsset)
	}
	resp, err := c.Open(a.BrowserDownloadURL)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	var m Manifest
	if err := json.NewDecoder(io.LimitReader(resp.Body, 32<<20)).Decode(&m); err != nil {
		return nil, fmt.Errorf("manifesto invalido: %w", err)
	}
	return &m, nil
}
