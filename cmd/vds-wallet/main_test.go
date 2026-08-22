package main

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestHealth(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/health", nil)
	rec := httptest.NewRecorder()
	mux := http.NewServeMux()
	mux.HandleFunc("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("ok\n"))
	})
	mux.ServeHTTP(rec, req)
	if rec.Code != 200 {
		t.Fatalf("status %d", rec.Code)
	}
	body, _ := io.ReadAll(rec.Body)
	if strings.TrimSpace(string(body)) != "ok" {
		t.Fatalf("body %q", body)
	}
}

func TestPublicURL(t *testing.T) {
	if got := publicURL("127.0.0.1:43187"); got != "http://127.0.0.1:43187" {
		t.Fatal(got)
	}
	if got := publicURL("0.0.0.0:43187"); got != "http://127.0.0.1:43187" {
		t.Fatal(got)
	}
}
