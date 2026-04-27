#!/usr/bin/env python3
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
from typing import Any


KEYWORDS = {
    "func": "Deklarasi fungsi CBALT.",
    "let": "Deklarasi variabel immutable.",
    "var": "Deklarasi variabel mutable.",
    "const": "Deklarasi konstanta.",
    "return": "Keluar dari fungsi dan mengembalikan nilai.",
    "if": "Percabangan kondisional.",
    "else": "Cabang alternatif untuk if.",
    "while": "Loop selama kondisi bernilai true.",
    "for": "Loop for-in sederhana.",
    "struct": "Deklarasi tipe struct.",
    "enum": "Deklarasi enum.",
    "import": "Import modul CBALT.",
    "unsafe": "Blok operasi low-level.",
    "spawn": "Eksekusi task secara async-style.",
    "printc": "Built-in output universal. Bisa string, bool, int, float, atau ekspresi.",
    "print": "Alias printer yang tetap aktif untuk kompatibilitas.",
    "println": "Alias printer dengan newline.",
    "i32": "Tipe integer 32-bit signed.",
    "i64": "Tipe integer 64-bit signed.",
    "f32": "Tipe floating-point 32-bit.",
    "f64": "Tipe floating-point 64-bit.",
    "bool": "Tipe boolean true/false.",
    "string": "Tipe string UTF-8 sederhana.",
    "any": "Tipe dinamis/pointer umum pada backend saat ini.",
}

COMPLETIONS = sorted(KEYWORDS.keys())
ERROR_RE = re.compile(r"^\[(?P<kind>\w+) Error\]\s+(?P<file>.*?):(?P<line>\d+):(?P<col>\d+)\s+.\s+(?P<msg>.*)$")
TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
FUNC_RE = re.compile(r"^\s*(?:async\s+)?func\s+([A-Za-z_][A-Za-z0-9_]*)\s*\((.*?)\)(?:\s*->\s*([A-Za-z_][A-Za-z0-9_]*))?")
VAR_RE = re.compile(r"^\s*(?:let|var|const)\s+([A-Za-z_][A-Za-z0-9_]*)")
STRUCT_RE = re.compile(r"^\s*struct\s+([A-Za-z_][A-Za-z0-9_]*)")
ENUM_RE = re.compile(r"^\s*enum\s+([A-Za-z_][A-Za-z0-9_]*)")
SNIPPETS = [
    {
        "label": "func main",
        "kind": 15,
        "detail": "Snippet fungsi main",
        "insertText": "func main() -> i32 {\n    $0\n    return 0;\n}",
        "insertTextFormat": 2,
    },
    {
        "label": "func",
        "kind": 15,
        "detail": "Snippet deklarasi fungsi",
        "insertText": "func ${1:nama}(${2:arg}: ${3:i32}) -> ${4:i32} {\n    $0\n}",
        "insertTextFormat": 2,
    },
    {
        "label": "if",
        "kind": 15,
        "detail": "Snippet percabangan if",
        "insertText": "if ${1:kondisi} {\n    $0\n}",
        "insertTextFormat": 2,
    },
    {
        "label": "while",
        "kind": 15,
        "detail": "Snippet loop while",
        "insertText": "while ${1:kondisi} {\n    $0\n}",
        "insertTextFormat": 2,
    },
    {
        "label": "printc",
        "kind": 15,
        "detail": "Snippet print universal",
        "insertText": "printc(${1:\"text\"});",
        "insertTextFormat": 2,
    },
]


def make_response(request_id: Any, result: Any) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": request_id, "result": result}


def make_notification(method: str, params: Any) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "method": method, "params": params}


def make_error(request_id: Any, code: int, message: str) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


class JsonRpcStream:
    def read_message(self) -> dict[str, Any] | None:
        content_length = None
        while True:
            line = sys.stdin.buffer.readline()
            if not line:
                return None
            if line == b"\r\n":
                break
            header = line.decode("utf-8", errors="replace").strip()
            if header.lower().startswith("content-length:"):
                content_length = int(header.split(":", 1)[1].strip())

        if content_length is None:
            return None

        body = sys.stdin.buffer.read(content_length)
        return json.loads(body.decode("utf-8"))

    def write_message(self, payload: dict[str, Any]) -> None:
        raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        sys.stdout.buffer.write(f"Content-Length: {len(raw)}\r\n\r\n".encode("ascii"))
        sys.stdout.buffer.write(raw)
        sys.stdout.buffer.flush()


class CBaltLanguageServer:
    def __init__(self, compiler_path: pathlib.Path) -> None:
        self.compiler_path = compiler_path
        self.rpc = JsonRpcStream()
        self.documents: dict[str, str] = {}
        self.shutdown_requested = False

    def run(self) -> None:
        while True:
            message = self.rpc.read_message()
            if message is None:
                return
            self.handle_message(message)

    def handle_message(self, message: dict[str, Any]) -> None:
        method = message.get("method")
        if method:
            self.handle_method(message)
            return

        request_id = message.get("id")
        self.rpc.write_message(make_error(request_id, -32601, "Method tidak dikenal"))

    def handle_method(self, message: dict[str, Any]) -> None:
        method = message["method"]
        params = message.get("params", {})
        request_id = message.get("id")

        if method == "initialize":
            result = {
                "serverInfo": {"name": "cbalt-lsp", "version": "0.1.0"},
                "capabilities": {
                    "textDocumentSync": 1,
                    "hoverProvider": True,
                    "definitionProvider": True,
                    "documentSymbolProvider": True,
                    "signatureHelpProvider": {"triggerCharacters": ["(", ","]},
                    "completionProvider": {"resolveProvider": False, "triggerCharacters": [".", "("]},
                },
            }
            self.rpc.write_message(make_response(request_id, result))
            return

        if method == "initialized":
            return

        if method == "shutdown":
            self.shutdown_requested = True
            self.rpc.write_message(make_response(request_id, None))
            return

        if method == "exit":
            raise SystemExit(0 if self.shutdown_requested else 1)

        if method == "textDocument/didOpen":
            doc = params["textDocument"]
            self.documents[doc["uri"]] = doc["text"]
            self.publish_diagnostics(doc["uri"])
            return

        if method == "textDocument/didChange":
            doc = params["textDocument"]
            changes = params.get("contentChanges", [])
            if changes:
                self.documents[doc["uri"]] = changes[-1]["text"]
            self.publish_diagnostics(doc["uri"])
            return

        if method == "textDocument/didSave":
            self.publish_diagnostics(params["textDocument"]["uri"])
            return

        if method == "textDocument/hover":
            self.rpc.write_message(make_response(request_id, self.handle_hover(params)))
            return

        if method == "textDocument/documentSymbol":
            self.rpc.write_message(make_response(request_id, self.handle_document_symbols(params)))
            return

        if method == "textDocument/completion":
            self.rpc.write_message(make_response(request_id, self.handle_completion(params)))
            return

        if method == "textDocument/definition":
            self.rpc.write_message(make_response(request_id, self.handle_definition(params)))
            return

        if method == "textDocument/signatureHelp":
            self.rpc.write_message(make_response(request_id, self.handle_signature_help(params)))
            return

        if request_id is not None:
            self.rpc.write_message(make_error(request_id, -32601, f"Method tidak didukung: {method}"))

    def handle_hover(self, params: dict[str, Any]) -> dict[str, Any] | None:
        uri = params["textDocument"]["uri"]
        text = self.documents.get(uri, "")
        position = params["position"]
        word = self.word_at_position(text, position["line"], position["character"])
        if not word:
            return None

        description = KEYWORDS.get(word)
        if not description:
            return None

        return {
            "contents": {
                "kind": "markdown",
                "value": f"```cbalt\n{word}\n```\n{description}",
            }
        }

    def handle_document_symbols(self, params: dict[str, Any]) -> list[dict[str, Any]]:
        uri = params["textDocument"]["uri"]
        text = self.documents.get(uri, "")
        symbols: list[dict[str, Any]] = []
        lines = text.splitlines()

        for idx, line in enumerate(lines):
            symbols.extend(self.symbols_from_line(line, idx))

        return symbols

    def handle_completion(self, params: dict[str, Any]) -> dict[str, Any]:
        uri = params.get("textDocument", {}).get("uri", "")
        text = self.documents.get(uri, "")
        items = list(SNIPPETS)
        for label in COMPLETIONS:
            kind = 14 if label in {"i32", "i64", "f32", "f64", "bool", "string", "any"} else 14
            if label in {"func", "let", "var", "const", "return", "if", "else", "while", "for", "struct", "enum"}:
                kind = 14
            if label in {"printc", "print", "println"}:
                kind = 3
            items.append({"label": label, "kind": kind, "detail": KEYWORDS.get(label, "")})

        seen = {item["label"] for item in items}
        for symbol in self.collect_symbols(text):
            if symbol["name"] in seen:
                continue
            items.append({
                "label": symbol["name"],
                "kind": symbol["kind"],
                "detail": symbol["detail"],
            })
            seen.add(symbol["name"])
        return {"isIncomplete": False, "items": items}

    def handle_definition(self, params: dict[str, Any]) -> list[dict[str, Any]]:
        uri = params["textDocument"]["uri"]
        text = self.documents.get(uri, "")
        position = params["position"]
        word = self.word_at_position(text, position["line"], position["character"])
        if not word:
            return []

        for symbol in self.collect_symbols(text):
            if symbol["name"] != word:
                continue
            return [{
                "uri": uri,
                "range": symbol["range"],
            }]
        return []

    def handle_signature_help(self, params: dict[str, Any]) -> dict[str, Any] | None:
        uri = params["textDocument"]["uri"]
        text = self.documents.get(uri, "")
        position = params["position"]
        lines = text.splitlines()
        if position["line"] >= len(lines):
            return None

        prefix = lines[position["line"]][:position["character"]]
        match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\([^()]*$", prefix)
        if not match:
            return None

        func_name = match.group(1)
        active_parameter = prefix.count(",")

        if func_name == "printc":
            return {
                "signatures": [{
                    "label": "printc(any, ...)",
                    "documentation": "Printer universal CBALT. Bisa string, bool, int, float, atau ekspresi campuran.",
                    "parameters": [{"label": "any"}],
                }],
                "activeSignature": 0,
                "activeParameter": active_parameter,
            }

        for symbol in self.collect_symbols(text):
            if symbol["name"] != func_name or not symbol.get("signature"):
                continue
            return {
                "signatures": [{
                    "label": symbol["signature"],
                    "documentation": symbol["detail"],
                    "parameters": [{"label": p.strip()} for p in symbol.get("params", []) if p.strip()],
                }],
                "activeSignature": 0,
                "activeParameter": min(active_parameter, max(len(symbol.get("params", [])) - 1, 0)),
            }
        return None

    def publish_diagnostics(self, uri: str) -> None:
        text = self.documents.get(uri, "")
        diagnostics = self.run_compiler_check(uri, text)
        self.rpc.write_message(make_notification(
            "textDocument/publishDiagnostics",
            {"uri": uri, "diagnostics": diagnostics},
        ))

    def run_compiler_check(self, uri: str, text: str) -> list[dict[str, Any]]:
        suffix = pathlib.Path(self.uri_to_path(uri)).suffix or ".cbalt"
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=suffix, delete=False) as tmp:
            tmp.write(text)
            tmp_path = pathlib.Path(tmp.name)

        try:
            proc = subprocess.run(
                [
                    str(self.compiler_path),
                    "--check",
                    "--no-banner",
                    "--plain-errors",
                    str(tmp_path),
                ],
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            output = "\n".join(part for part in [proc.stdout, proc.stderr] if part)
            diagnostics = []
            for line in output.splitlines():
                match = ERROR_RE.match(line.strip())
                if not match:
                    continue
                line_no = max(int(match.group("line")) - 1, 0)
                col_no = max(int(match.group("col")) - 1, 0)
                diagnostics.append({
                    "range": {
                        "start": {"line": line_no, "character": col_no},
                        "end": {"line": line_no, "character": col_no + 1},
                    },
                    "severity": 1,
                    "source": match.group("kind").lower(),
                    "message": match.group("msg"),
                })
            return diagnostics
        finally:
            try:
                tmp_path.unlink()
            except OSError:
                pass

    def word_at_position(self, text: str, line: int, character: int) -> str | None:
        lines = text.splitlines()
        if line < 0 or line >= len(lines):
            return None
        row = lines[line]
        if character > len(row):
            character = len(row)

        for match in TOKEN_RE.finditer(row):
            if match.start() <= character <= match.end():
                return match.group(0)
        return None

    def symbols_from_line(self, line: str, line_no: int) -> list[dict[str, Any]]:
        results = []
        func_match = FUNC_RE.search(line)
        if func_match:
            name = func_match.group(1)
            params = [p.strip() for p in (func_match.group(2) or "").split(",") if p.strip()]
            ret = func_match.group(3) or "void"
            start = func_match.start(1)
            end = func_match.end(1)
            results.append({
                "name": name,
                "kind": 12,
                "detail": f"func {name} -> {ret}",
                "signature": f"func {name}({', '.join(params)}) -> {ret}",
                "params": params,
                "range": {
                    "start": {"line": line_no, "character": 0},
                    "end": {"line": line_no, "character": len(line)},
                },
                "selectionRange": {
                    "start": {"line": line_no, "character": start},
                    "end": {"line": line_no, "character": end},
                },
            })

        for regex, kind, detail_prefix in ((STRUCT_RE, 23, "struct"), (ENUM_RE, 10, "enum"), (VAR_RE, 13, "var")):
            match = regex.search(line)
            if not match:
                continue
            name = match.group(1)
            start = match.start(1)
            end = match.end(1)
            results.append({
                "name": name,
                "kind": kind,
                "detail": f"{detail_prefix} {name}",
                "range": {
                    "start": {"line": line_no, "character": 0},
                    "end": {"line": line_no, "character": len(line)},
                },
                "selectionRange": {
                    "start": {"line": line_no, "character": start},
                    "end": {"line": line_no, "character": end},
                },
            })
        return results

    def collect_symbols(self, text: str) -> list[dict[str, Any]]:
        symbols: list[dict[str, Any]] = []
        for idx, line in enumerate(text.splitlines()):
            symbols.extend(self.symbols_from_line(line, idx))
        return symbols

    @staticmethod
    def uri_to_path(uri: str) -> str:
        if uri.startswith("file:///"):
            path = uri[8:] if os.name == "nt" else uri[7:]
            return path.replace("/", os.sep)
        return uri


def default_compiler_path() -> pathlib.Path:
    root = pathlib.Path(__file__).resolve().parents[1]
    name = "cbalt.exe" if os.name == "nt" else "cbalt"
    return root / "bin" / name


def main() -> int:
    parser = argparse.ArgumentParser(description="CBALT Language Server")
    parser.add_argument("--compiler", type=pathlib.Path, default=default_compiler_path())
    args = parser.parse_args()

    server = CBaltLanguageServer(args.compiler)
    server.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
