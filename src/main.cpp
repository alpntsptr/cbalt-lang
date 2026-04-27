//  CBALT Compiler — Entry Point
//  Versi  : 0.1.0
//  Penulis: [EI AI BY GIPITI FRI]

#include "cbalt.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;
using namespace cbalt;

// ---------------------------------------------------------------
//  Baca file ke string
// ---------------------------------------------------------------
static std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Gagal buka file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ---------------------------------------------------------------
//  Print banner CBALT
// ---------------------------------------------------------------
static void printBanner() {
    std::cout << R"(
   ██████╗██████╗  █████╗ ██╗  ████████╗
  ██╔════╝██╔══██╗██╔══██╗██║  ╚══██╔══╝
  ██║     ██████╔╝███████║██║     ██║
  ██║     ██╔══██╗██╔══██║██║     ██║
  ╚██████╗██████╔╝██║  ██║███████╗██║
   ╚═════╝╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝
  Compiler v)" CBALT_VERSION_STRING R"( — Tulis sekali, jalan di mana-mana.
)" << '\n';
}

// ---------------------------------------------------------------
//  Print bantuan CLI
// ---------------------------------------------------------------
static void printHelp(const char* prog) {
    std::cout
        << "Penggunaan: " << prog << " [opsi] <file.cbalt>\n\n"
        << "Opsi:\n"
        << "  -o <output>       Nama file output  (default: a.out)\n"
        << "  -t <target>       Target platform   (native|wasm|llvmir)\n"
        << "  -O, --release     Build optimasi penuh (O3)\n"
        << "  -v, --verbose     Output verbose\n"
        << "  --dump-ast        Print AST ke stdout\n"
        << "  --dump-ir         Print LLVM IR ke stdout\n"
        << "  --check           Hanya cek tanpa emit file\n"
        << "  --no-banner       Jangan tampilkan banner\n"
        << "  --plain-errors    Error tanpa warna ANSI\n"
        << "  -h, --help        Tampilkan bantuan ini\n"
        << "  --version         Tampilkan versi\n\n"
        << "Contoh:\n"
        << "  cbalt main.cbalt -o main\n"
        << "  cbalt game.cbalt -o game -t native -O\n"
        << "  cbalt api.cbalt  -o api.wasm -t wasm\n"
        << "  cbalt main.cbalt --dump-ir\n\n";
}

// ---------------------------------------------------------------
//  Parse argumen CLI
// ---------------------------------------------------------------
static CompilerConfig parseArgs(int argc, char** argv) {
    CompilerConfig cfg;

    if (argc < 2) {
        printHelp(argv[0]);
        exit(0);
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            exit(0);
        } else if (arg == "--version") {
            std::cout << "CBALT v" CBALT_VERSION_STRING "\n";
            exit(0);
        } else if (arg == "-o" && i + 1 < argc) {
            cfg.outputFile = argv[++i];
        } else if (arg == "-t" && i + 1 < argc) {
            std::string t = argv[++i];
            if      (t == "native")  cfg.target = TargetPlatform::Native;
            else if (t == "wasm")    cfg.target = TargetPlatform::WebAssembly;
            else if (t == "llvmir")  cfg.target = TargetPlatform::LLVMIR;
            else {
                std::cerr << "[Error] Target tidak dikenal: " << t << "\n";
                exit(1);
            }
        } else if (arg == "-O" || arg == "--release") {
            cfg.mode = BuildMode::Release;
        } else if (arg == "-v" || arg == "--verbose") {
            cfg.verbose = true;
        } else if (arg == "--dump-ast") {
            cfg.dumpAST = true;
        } else if (arg == "--dump-ir") {
            cfg.dumpIR = true;
        } else if (arg == "--check") {
            cfg.checkOnly = true;
        } else if (arg == "--no-banner") {
            cfg.noBanner = true;
        } else if (arg == "--plain-errors") {
            cfg.plainErrors = true;
        } else if (arg[0] == '-') {
            std::cerr << "[Peringatan] Opsi tidak dikenal: " << arg << "\n";
        } else {
            cfg.inputFile = arg;
        }
    }

    if (cfg.inputFile.empty()) {
        std::cerr << "[Error] Tidak ada file input.\n";
        printHelp(argv[0]);
        exit(1);
    }

    // Set output default dari nama file input
    if (cfg.outputFile == "a.out") {
        fs::path p(cfg.inputFile);
        cfg.outputFile = p.stem().string();
        if (cfg.target == TargetPlatform::WebAssembly) cfg.outputFile += ".wasm";
        else if (cfg.target == TargetPlatform::LLVMIR)  cfg.outputFile += ".ll";
    }

    return cfg;
}

// ---------------------------------------------------------------
//  Print semua error dengan warna (kalau terminal mendukung)
// ---------------------------------------------------------------
static void printErrors(const std::vector<CBaltError>& errs, bool plain = false) {
    for (const auto& e : errs) {
        if (plain) std::cerr << e.format() << "\n";
        else std::cerr << "\033[31m" << e.format() << "\033[0m\n";
    }
}

// ---------------------------------------------------------------
//  Main
// ---------------------------------------------------------------
int main(int argc, char** argv) {
    // Parse argumen CLI
    CompilerConfig cfg = parseArgs(argc, argv);
    if (!cfg.noBanner) {
        printBanner();
    }

    if (cfg.verbose) {
        std::cout << "[Info] Input  : " << cfg.inputFile  << "\n"
                  << "[Info] Output : " << cfg.outputFile << "\n"
                  << "[Info] Target : " << (cfg.target == TargetPlatform::Native ? "native" :
                                            cfg.target == TargetPlatform::WebAssembly ? "wasm" : "llvmir") << "\n"
                  << "[Info] Mode   : " << (cfg.mode == BuildMode::Release ? "release" : "debug") << "\n\n";
    }

    // --- FASE 1: Baca source ---
    std::string source;
    try {
        source = readFile(cfg.inputFile);
    } catch (const std::exception& e) {
        std::cerr << "\033[31m[IO Error] " << e.what() << "\033[0m\n";
        return 1;
    }

    // --- FASE 2: Lexing ---
    if (cfg.verbose) std::cout << "[1/3] Lexing...\n";
    Lexer lexer(source, cfg.inputFile);
    auto tokens = lexer.tokenize();

    if (lexer.hasErrors()) {
        printErrors(lexer.errors(), cfg.plainErrors);
        return 1;
    }

    if (cfg.verbose) {
        std::cout << "      " << tokens.size() << " token dihasilkan.\n";
    }

    // --- FASE 3: Parsing → AST ---
    if (cfg.verbose) std::cout << "[2/3] Parsing...\n";
    Parser parser(std::move(tokens));
    auto ast = parser.parse();

    if (parser.hasErrors()) {
        printErrors(parser.errors(), cfg.plainErrors);
        return 1;
    }

    if (cfg.dumpAST && ast) {
        std::cout << "\n--- AST Dump ---\n" << ast->dump() << "\n---\n\n";
    }

    // --- FASE 4: Code Generation → LLVM IR → Binary ---
    if (cfg.verbose) std::cout << "[3/3] Generating code...\n";
    CodeGen codegen(cfg);
    bool ok = codegen.generate(ast.get());

    if (!ok || codegen.hasErrors()) {
        printErrors(codegen.errors(), cfg.plainErrors);
        return 1;
    }

    if (cfg.dumpIR) {
        codegen.dumpIR();
    }

    if (cfg.checkOnly) {
        if (cfg.verbose) {
            std::cout << "[OK] Check selesai\n";
        }
        return 0;
    }

    // Emit output
    bool emitOk = false;
    switch (cfg.target) {
        case TargetPlatform::Native:
            emitOk = codegen.emitExecutable(cfg.outputFile);
            break;
        case TargetPlatform::WebAssembly:
            emitOk = codegen.emitWasm(cfg.outputFile);
            break;
        case TargetPlatform::LLVMIR:
            codegen.dumpIR(cfg.outputFile);
            emitOk = true;
            break;
    }

    if (!emitOk) {
        if (cfg.plainErrors) std::cerr << "[Error] Gagal emit output: " << cfg.outputFile << "\n";
        else std::cerr << "\033[31m[Error] Gagal emit output: " << cfg.outputFile << "\033[0m\n";
        return 1;
    }

    std::cout << "\033[32m[OK] Build selesai → " << cfg.outputFile << "\033[0m\n";
    if (cfg.plainErrors) {
        std::cout << "[OK] Build selesai -> " << cfg.outputFile << "\n";
        return 0;
    }
    return 0;
}
