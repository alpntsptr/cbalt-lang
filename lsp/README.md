# CBALT LSP

LSP ini adalah starter server untuk CBALT. Fokusnya sekarang:

- diagnostics dari compiler CBALT asli
- hover untuk keyword dan built-in dasar
- document symbols untuk `func`, `struct`, `enum`, `let/var/const`
- completion untuk keyword, built-in, snippet, fungsi, variabel, struct, dan enum
- go-to-definition sederhana untuk simbol dalam file yang sama
- signature help dasar untuk fungsi dan `printc(...)`

## Menjalankan

Build compiler dulu:

```powershell
cmake --build build
```

Lalu jalankan server:

```powershell
lsp\start_cbalt_lsp.bat
```

Atau langsung dengan interpreter Python:

```powershell
python lsp/cbalt_lsp.py
```

Atau di Windows:

```powershell
lsp\start_cbalt_lsp.bat
```

## Contoh konfigurasi VS Code

Kalau pakai extension client LSP generik, arahkan command ke:

```text
lsp/start_cbalt_lsp.bat
```

Atau langsung:

```text
lsp/start_cbalt_lsp.bat
```

## Cara kerja diagnostics

Server menyimpan isi file yang sedang dibuka, lalu menjalankan:

```text
cbalt --check --no-banner --plain-errors <tempfile>
```

Output error compiler diparsing kembali menjadi diagnostics LSP.

## Keterbatasan saat ini

- belum ada rename symbol
- hover untuk simbol user-defined masih sederhana
- definition masih satu file
- document symbol dan signature help masih berbasis scan teks sederhana

Tapi fondasinya sudah siap untuk dikembangkan ke semantic server yang lebih kaya.
