#pragma once

#include "cbalt.hpp"

namespace cbalt {

// ---------------------------------------------------------------
//  Semua jenis Token yang dikenali CBALT
// ---------------------------------------------------------------
enum class TokenKind {

    // Literal
    IntLiteral,     // 42
    FloatLiteral,   // 3.14
    StringLiteral,  // "hello"
    BoolLiteral,    // true / false
    NullLiteral,    // null

    // Identifier & Keywords
    Identifier,

    // Keyword — Deklarasi
    KW_let,         // let
    KW_var,         // var
    KW_const,       // const
    KW_func,        // func
    KW_return,      // return
    KW_struct,      // struct
    KW_enum,        // enum
    KW_impl,        // impl
    KW_trait,       // trait

    // Keyword — Kontrol Alur
    KW_if,          // if
    KW_else,        // else
    KW_elif,        // elif
    KW_while,       // while
    KW_for,         // for
    KW_in,          // in
    KW_break,       // break
    KW_continue,    // continue
    KW_match,       // match (pattern matching)

    // Keyword — Module & Import
    KW_import,      // import
    KW_export,      // export
    KW_use,         // use
    KW_as,          // as

    // Keyword — Memory & Low Level
    KW_asm,         // asm { ... } inline assembly
    KW_unsafe,      // unsafe { ... }
    KW_ptr,         // ptr<T>
    KW_ref,         // ref<T>
    KW_sizeof,      // sizeof(T)
    KW_typeof,      // typeof(x)

    // Keyword — Tipe Primitif
    KW_int,         // int    (i32 default)
    KW_i8,          // i8
    KW_i16,         // i16
    KW_i32,         // i32
    KW_i64,         // i64
    KW_u8,          // u8
    KW_u16,         // u16
    KW_u32,         // u32
    KW_u64,         // u64
    KW_float,       // float  (f32 default)
    KW_f32,         // f32
    KW_f64,         // f64
    KW_bool,        // bool
    KW_string,      // string
    KW_void,        // void
    KW_any,         // any    (dynamic type)
    KW_never,       // never  (function never returns)

    // Keyword — Concurrency
    KW_async,       // async
    KW_await,       // await
    KW_spawn,       // spawn (goroutine-style)
    KW_chan,        // chan  (channel)

    // Keyword — Web / Decorators
    KW_at,          // @ (decorator prefix)

    // Keyword — Game / ECS
    KW_entity,      // @entity
    KW_component,   // @component
    KW_system,      // @system

    // Operator — Aritmatik
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    StarStar,       // **  (power)

    // Operator — Bitwise
    Ampersand,      // &
    Pipe,           // |
    Caret,          // ^
    Tilde,          // ~
    ShiftLeft,      // <<
    ShiftRight,     // >>

    // Operator — Perbandingan
    EqualEqual,     // ==
    BangEqual,      // !=
    Less,           // <
    Greater,        // >
    LessEqual,      // <=
    GreaterEqual,   // >=

    // Operator — Logika
    AmpAmp,         // &&
    PipePipe,       // ||
    Bang,           // !

    // Operator — Assign
    Equal,          // =
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=
    AmpEqual,       // &=
    PipeEqual,      // |=
    CaretEqual,     // ^=

    // Operator — Lain
    Arrow,          // ->  (return type)
    FatArrow,       // =>  (lambda / match arm)
    ColonColon,     // ::  (path separator)
    DotDot,         // ..  (range)
    DotDotDot,      // ... (variadic / spread)
    QuestionMark,   // ?   (optional chaining)
    QuestionColon,  // ?:  (null coalescing)
    At,             // @   (decorator)
    Hash,           // #   (pragma / attribute)

    // Tanda Baca
    Dot,            // .
    Comma,          // ,
    Colon,          // :
    Semicolon,      // ;
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    LBracket,       // [
    RBracket,       // ]

    // Spesial
    Newline,        // \n  (signifikan di beberapa konteks)
    EOF_,           // Akhir file
    Unknown,        // Token tidak dikenali
};

// ---------------------------------------------------------------
//  Token
// ---------------------------------------------------------------
struct Token {
    TokenKind      kind;
    std::string    value;    // Raw teks dari source
    SourceLocation location;

    bool is(TokenKind k) const { return kind == k; }
    bool isKeyword()     const { return kind >= TokenKind::KW_let && kind <= TokenKind::KW_at; }
    bool isLiteral()     const { return kind >= TokenKind::IntLiteral && kind <= TokenKind::NullLiteral; }
    bool isOperator()    const { return kind >= TokenKind::Plus && kind <= TokenKind::QuestionColon; }

    std::string kindName() const;
    std::string toString() const;
};

// ---------------------------------------------------------------
//  Lexer — Pemecah Kata CBALT
//
//  Tugas: Baca source code karakter per karakter,
//         hasilkan stream of Tokens.
// ---------------------------------------------------------------
class Lexer {
public:
    explicit Lexer(const std::string& source, const std::string& filename = "<stdin>");

    // Ambil semua token sekaligus
    std::vector<Token> tokenize();

    // Ambil token satu per satu (lazy)
    Token nextToken();

    // Peek tanpa konsumsi
    Token peekToken();

    const std::vector<CBaltError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::string   source_;
    std::string   filename_;
    size_t        pos_    = 0;
    i32           line_   = 1;
    i32           column_ = 1;
    std::vector<CBaltError> errors_;

    // --- Karakter helpers ---
    char current()  const;
    char peek(i32 offset = 1) const;
    char advance();
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    bool isAtEnd() const;

    // --- Pembaca token spesifik ---
    Token readIdentifierOrKeyword();
    Token readNumber();
    Token readString();
    Token readCharLiteral();
    Token readAsmBlock();
    Token readOperatorOrPunct();

    // --- Utilities ---
    TokenKind resolveKeyword(const std::string& word) const;
    SourceLocation currentLocation() const;
    void emitError(const std::string& msg);
    Token makeToken(TokenKind kind, const std::string& value = "") const;
};

} // namespace cbalt