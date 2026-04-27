#pragma once

// ---------------------------------------------------------------
//  CBALT — Master Header
//  Versi  : 0.1.0
//  Lisensi: MIT
// ---------------------------------------------------------------

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <optional>
#include <variant>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdint>

// ---------------------------------------------------------------
//  Versi Compiler
// ---------------------------------------------------------------
#define CBALT_VERSION_MAJOR 0
#define CBALT_VERSION_MINOR 1
#define CBALT_VERSION_PATCH 0
#define CBALT_VERSION_STRING "0.1.0"

// ---------------------------------------------------------------
//  Tipe Dasar
// ---------------------------------------------------------------
namespace cbalt {

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

// ---------------------------------------------------------------
//  Source Location — buat error reporting yang informatif
// ---------------------------------------------------------------
struct SourceLocation {
    std::string filename;
    i32 line   = 1;
    i32 column = 1;

    std::string toString() const {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

// ---------------------------------------------------------------
//  Error CBALT
// ---------------------------------------------------------------
struct CBaltError {
    enum class Kind {
        Lexer,
        Parser,
        Semantic,
        CodeGen,
        IO
    };

    Kind        kind;
    std::string message;
    SourceLocation location;

    std::string format() const {
        return "[" + kindName() + " Error] " + location.toString() + " — " + message;
    }

private:
    std::string kindName() const {
        switch (kind) {
            case Kind::Lexer:    return "Lexer";
            case Kind::Parser:   return "Parser";
            case Kind::Semantic: return "Semantic";
            case Kind::CodeGen:  return "CodeGen";
            case Kind::IO:       return "IO";
        }
        return "Unknown";
    }
};

// ---------------------------------------------------------------
//  Target Platform
// ---------------------------------------------------------------
enum class TargetPlatform {
    Native,     // Langsung ke mesin (executable)
    WebAssembly, // Buat web frontend (WASM)
    LLVMIR,     // Dump IR buat debug
};

// ---------------------------------------------------------------
//  Build Mode
// ---------------------------------------------------------------
enum class BuildMode {
    Debug,   // Include debug info, no optimization
    Release, // Full optimization (-O3)
};

// ---------------------------------------------------------------
//  Konfigurasi Compiler
// ---------------------------------------------------------------
struct CompilerConfig {
    std::string        inputFile;
    std::string        outputFile  = "a.out";
    TargetPlatform     target      = TargetPlatform::Native;
    BuildMode          mode        = BuildMode::Debug;
    bool               verbose     = false;
    bool               dumpIR      = false;
    bool               dumpAST     = false;
    bool               noBanner    = false;
    bool               checkOnly   = false;
    bool               plainErrors = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> libraryPaths;
};

} // namespace cbalt
