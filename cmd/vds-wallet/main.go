package main

import (
	"embed"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"net"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"os/exec"
	"path"
	"runtime"
	"strings"
	"time"
)

//go:embed all:web
var webFS embed.FS

func envOr(key, fallback string) string {
	if v := strings.TrimSpace(os.Getenv(key)); v != "" {
		return v
	}
	return fallback
}

func reverseProxy(raw string, stripPrefix string) http.Handler {
	target, err := url.Parse(raw)
	if err != nil {
		log.Fatalf("proxy target: %v", err)
	}
	proxy := httputil.NewSingleHostReverseProxy(target)
	orig := proxy.Director
	proxy.Director = func(req *http.Request) {
		orig(req)
		req.URL.Scheme = target.Scheme
		req.URL.Host = target.Host
		req.Host = target.Host
		if stripPrefix != "" {
			req.URL.Path = strings.TrimPrefix(req.URL.Path, stripPrefix)
			if req.URL.Path == "" {
				req.URL.Path = "/"
			}
		}
		req.Header.Del("Origin")
		req.Header.Del("Referer")
	}
	proxy.ErrorHandler = func(w http.ResponseWriter, r *http.Request, err error) {
		http.Error(w, "Explorer/Pool nicht erreichbar: "+err.Error(), http.StatusBadGateway)
	}
	return proxy
}

func openBrowser(u string) {
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("rundll32", "url.dll,FileProtocolHandler", u)
	case "darwin":
		cmd = exec.Command("open", u)
	default:
		cmd = exec.Command("xdg-open", u)
	}
	_ = cmd.Start()
}

func publicURL(listen string) string {
	host, port, err := net.SplitHostPort(listen)
	if err != nil {
		return "http://" + listen
	}
	if host == "" || host == "0.0.0.0" || host == "::" {
		host = "127.0.0.1"
	}
	return "http://" + net.JoinHostPort(host, port)
}

func main() {
	listen := flag.String("listen", envOr("VDS_WALLET_LISTEN", "127.0.0.1:43187"), "Adresse, auf der die Wallet lauscht")
	open := flag.Bool("open", true, "Browser nach dem Start öffnen")
	flag.Parse()

	sub, err := fs.Sub(webFS, "web")
	if err != nil {
		log.Fatal(err)
	}

	mux := http.NewServeMux()
	mux.Handle("/vds-api/", reverseProxy("https://www.vdscool.com", ""))
	mux.Handle("/pool-api/", reverseProxy("https://server.666pool.com", "/pool-api"))
	mux.HandleFunc("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		_, _ = w.Write([]byte("ok\n"))
	})

	fileServer := http.FileServer(http.FS(sub))
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		clean := path.Clean("/" + r.URL.Path)
		if clean != "/" {
			if _, err := fs.Stat(sub, strings.TrimPrefix(clean, "/")); err == nil {
				fileServer.ServeHTTP(w, r)
				return
			}
		}
		index, err := fs.ReadFile(sub, "index.html")
		if err != nil {
			http.Error(w, "Wallet-Dateien fehlen", http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-store")
		_, _ = w.Write(index)
	})

	ln, err := net.Listen("tcp", *listen)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Port belegt oder ungültig (%s): %v\n", *listen, err)
		os.Exit(1)
	}

	u := publicURL(ln.Addr().String())
	fmt.Printf("\n  VDS-Wallet läuft lokal.\n  Browser: %s\n  Fenster offen lassen. Beenden: Strg+C\n\n", u)

	if *open {
		go func() {
			time.Sleep(250 * time.Millisecond)
			openBrowser(u)
		}()
	}

	log.Fatal(http.Serve(ln, withLog(mux)))
}

func withLog(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if !strings.HasPrefix(r.URL.Path, "/assets/") {
			log.Printf("%s %s", r.Method, r.URL.RequestURI())
		}
		next.ServeHTTP(w, r)
	})
}
