#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace cbalt {

// ---------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------
Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename) {}

// ---------------------------------------------------------------
//  Public: tokenize — kembalikan semua token sekaligus
// ---------------------------------------------------------------
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> result;
    while (!isAtEnd()) {
        Token t = nextToken();
        if (t.kind == TokenKind::EOF_) {
            result.push_back(t);
            break;
        }
        // Buang newline kecuali signifikan (untuk sekarang skip)
        if (t.kind == TokenKind::Newline) continue;
        result.push_back(std::move(t));
    }
    if (result.empty() || result.back().kind != TokenKind::EOF_) {
        result.push_back(makeToken(TokenKind::EOF_, ""));
    }
    return result;
}

// ---------------------------------------------------------------
//  Public: nextToken
// ---------------------------------------------------------------
Token Lexer::nextToken() {
    skipWhitespace();

    if (isAtEnd()) {
        return makeToken(TokenKind::EOF_, "");
    }

    char c = current();

    // Komentar
    if (c == '/' && peek() == '/') {
        skipLineComment();
        return nextToken();
    }
    if (c == '/' && peek() == '*') {
        skipBlockComment();
        return nextToken();
    }

    // Newline
    if (c == '\n') {
        advance();
        return makeToken(TokenKind::Newline, "\n");
    }

    // String literal
    if (c == '"') return readString();

    // Angka
    if (std::isdigit(c) || (c == '-' && std::isdigit(peek()))) return readNumber();

    // Identifier / keyword
    if (std::isalpha(c) || c == '_') return readIdentifierOrKeyword();

    // Operator / tanda baca
    return readOperatorOrPunct();
}

// ---------------------------------------------------------------
//  Karakter helpers
// ---------------------------------------------------------------
char Lexer::current() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char Lexer::peek(i32 offset) const {
    size_t p = pos_ + offset;
    if (p >= source_.size()) return '\0';
    return source_[p];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { line_++; column_ = 1; }
    else           { column_++; }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r') advance();
        else break;
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && current() != '\n') advance();
}

void Lexer::skipBlockComment() {
    advance(); advance(); // konsumsi /*
    while (!isAtEnd()) {
        if (current() == '*' && peek() == '/') {
            advance(); advance();
            return;
        }
        advance();
    }
    emitError("Block comment tidak ditutup (missing '*/')");
}

// ---------------------------------------------------------------
//  readIdentifierOrKeyword
// ---------------------------------------------------------------
Token Lexer::readIdentifierOrKeyword() {
    SourceLocation loc = currentLocation();
    std::string word;
    while (!isAtEnd() && (std::isalnum(current()) || current() == '_')) {
        word += advance();
    }
    TokenKind kind = resolveKeyword(word);
    Token t;
    t.kind     = kind;
    t.value    = word;
    t.location = loc;
    return t;
}

// ---------------------------------------------------------------
//  resolveKeyword — map string → TokenKind
// ---------------------------------------------------------------
TokenKind Lexer::resolveKeyword(const std::string& w) const {
    static const std::unordered_map<std::string, TokenKind> kw = {
        // Deklarasi
        {"let",       TokenKind::KW_let},
        {"var",       TokenKind::KW_var},
        {"const",     TokenKind::KW_const},
        {"fn",        TokenKind::KW_func},
        {"func",      TokenKind::KW_func},
        {"return",    TokenKind::KW_return},
        {"struct",    TokenKind::KW_struct},
        {"enum",      TokenKind::KW_enum},
        {"impl",      TokenKind::KW_impl},
        {"trait",     TokenKind::KW_trait},
        // Kontrol alur
        {"if",        TokenKind::KW_if},
        {"else",      TokenKind::KW_else},
        {"elif",      TokenKind::KW_elif},
        {"while",     TokenKind::KW_while},
        {"for",       TokenKind::KW_for},
        {"in",        TokenKind::KW_in},
        {"break",     TokenKind::KW_break},
        {"continue",  TokenKind::KW_continue},
        {"match",     TokenKind::KW_match},
        // Module
        {"import",    TokenKind::KW_import},
        {"export",    TokenKind::KW_export},
        {"use",       TokenKind::KW_use},
        {"as",        TokenKind::KW_as},
        // Low level
        {"asm",       TokenKind::KW_asm},
        {"unsafe",    TokenKind::KW_unsafe},
        {"ptr",       TokenKind::KW_ptr},
        {"ref",       TokenKind::KW_ref},
        {"sizeof",    TokenKind::KW_sizeof},
        {"typeof",    TokenKind::KW_typeof},
        // Tipe primitif
        {"int",       TokenKind::KW_int},
        {"i8",        TokenKind::KW_i8},
        {"i16",       TokenKind::KW_i16},
        {"i32",       TokenKind::KW_i32},
        {"i64",       TokenKind::KW_i64},
        {"u8",        TokenKind::KW_u8},
        {"u16",       TokenKind::KW_u16},
        {"u32",       TokenKind::KW_u32},
        {"u64",       TokenKind::KW_u64},
        {"float",     TokenKind::KW_float},
        {"f32",       TokenKind::KW_f32},
        {"f64",       TokenKind::KW_f64},
        {"bool",      TokenKind::KW_bool},
        {"string",    TokenKind::KW_string},
        {"void",      TokenKind::KW_void},
        {"any",       TokenKind::KW_any},
        {"never",     TokenKind::KW_never},
        // Concurrency
        {"async",     TokenKind::KW_async},
        {"await",     TokenKind::KW_await},
        {"spawn",     TokenKind::KW_spawn},
        {"chan",       TokenKind::KW_chan},
        // Literal bool
        {"true",      TokenKind::BoolLiteral},
        {"false",     TokenKind::BoolLiteral},
        {"null",      TokenKind::NullLiteral},
    };
    auto it = kw.find(w);
    return it != kw.end() ? it->second : TokenKind::Identifier;
}

// ---------------------------------------------------------------
//  readNumber — int, float, hex, binary, octal
// ---------------------------------------------------------------
Token Lexer::readNumber() {
    SourceLocation loc = currentLocation();
    std::string num;
    bool isFloat = false;

    // Hex: 0x...
    if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
        num += advance(); num += advance();
        while (!isAtEnd() && std::isxdigit(current())) num += advance();
        return Token{TokenKind::IntLiteral, num, loc};
    }

    // Binary: 0b...
    if (current() == '0' && (peek() == 'b' || peek() == 'B')) {
        num += advance(); num += advance();
        while (!isAtEnd() && (current() == '0' || current() == '1')) num += advance();
        return Token{TokenKind::IntLiteral, num, loc};
    }

    // Octal: 0o...
    if (current() == '0' && (peek() == 'o' || peek() == 'O')) {
        num += advance(); num += advance();
        while (!isAtEnd() && current() >= '0' && current() <= '7') num += advance();
        return Token{TokenKind::IntLiteral, num, loc};
    }

    // Decimal / float
    while (!isAtEnd() && (std::isdigit(current()) || current() == '_')) {
        if (current() != '_') num += current();
        advance();
    }
    if (!isAtEnd() && current() == '.' && std::isdigit(peek())) {
        isFloat = true;
        num += advance();
        while (!isAtEnd() && std::isdigit(current())) num += advance();
    }
    // Exponent
    if (!isAtEnd() && (current() == 'e' || current() == 'E')) {
        isFloat = true;
        num += advance();
        if (!isAtEnd() && (current() == '+' || current() == '-')) num += advance();
        while (!isAtEnd() && std::isdigit(current())) num += advance();
    }

    return Token{isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, num, loc};
}

// ---------------------------------------------------------------
//  readString — "..." dengan escape sequence
// ---------------------------------------------------------------
Token Lexer::readString() {
    SourceLocation loc = currentLocation();
    advance(); // skip opening "
    std::string val;
    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n':  val += '\n'; advance(); break;
                case 't':  val += '\t'; advance(); break;
                case 'r':  val += '\r'; advance(); break;
                case '"':  val += '"';  advance(); break;
                case '\\': val += '\\'; advance(); break;
                case '0':  val += '\0'; advance(); break;
                default:
                    emitError(std::string("Escape sequence tidak dikenal: \\") + current());
                    advance();
            }
        } else {
            val += advance();
        }
    }
    if (isAtEnd()) {
        emitError("String literal tidak ditutup");
    } else {
        advance(); // skip closing "
    }
    return Token{TokenKind::StringLiteral, val, loc};
}

// ---------------------------------------------------------------
//  readOperatorOrPunct
// ---------------------------------------------------------------
Token Lexer::readOperatorOrPunct() {
    SourceLocation loc = currentLocation();
    char c = advance();

    auto make = [&](TokenKind k, const std::string& v) {
        return Token{k, v, loc};
    };

    switch (c) {
        case '+':
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::PlusEqual,  "+="); }
            return make(TokenKind::Plus, "+");
        case '-':
            if (!isAtEnd() && current() == '>') { advance(); return make(TokenKind::Arrow,      "->"); }
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::MinusEqual, "-="); }
            return make(TokenKind::Minus, "-");
        case '*':
            if (!isAtEnd() && current() == '*') { advance(); return make(TokenKind::StarStar,   "**"); }
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::StarEqual,  "*="); }
            return make(TokenKind::Star, "*");
        case '/':
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::SlashEqual, "/="); }
            return make(TokenKind::Slash, "/");
        case '%': return make(TokenKind::Percent, "%");
        case '&':
            if (!isAtEnd() && current() == '&') { advance(); return make(TokenKind::AmpAmp,     "&&"); }
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::AmpEqual,   "&="); }
            return make(TokenKind::Ampersand, "&");
        case '|':
            if (!isAtEnd() && current() == '|') { advance(); return make(TokenKind::PipePipe,   "||"); }
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::PipeEqual,  "|="); }
            return make(TokenKind::Pipe, "|");
        case '^':
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::CaretEqual, "^="); }
            return make(TokenKind::Caret, "^");
        case '~': return make(TokenKind::Tilde, "~");
        case '<':
            if (!isAtEnd() && current() == '<') { advance(); return make(TokenKind::ShiftLeft,  "<<"); }
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::LessEqual,  "<="); }
            return make(TokenKind::Less, "<");
        case '>':
            if (!isAtEnd() && current() == '>') { advance(); return make(TokenKind::ShiftRight, ">>"); }
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::GreaterEqual, ">="); }
            return make(TokenKind::Greater, ">");
        case '=':
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::EqualEqual, "=="); }
            if (!isAtEnd() && current() == '>') { advance(); return make(TokenKind::FatArrow,   "=>"); }
            return make(TokenKind::Equal, "=");
        case '!':
            if (!isAtEnd() && current() == '=') { advance(); return make(TokenKind::BangEqual,  "!="); }
            return make(TokenKind::Bang, "!");
        case ':':
            if (!isAtEnd() && current() == ':') { advance(); return make(TokenKind::ColonColon, "::"); }
            return make(TokenKind::Colon, ":");
        case '.':
            if (!isAtEnd() && current() == '.') {
                advance();
                if (!isAtEnd() && current() == '.') { advance(); return make(TokenKind::DotDotDot, "..."); }
                return make(TokenKind::DotDot, "..");
            }
            return make(TokenKind::Dot, ".");
        case '?':
            if (!isAtEnd() && current() == ':') { advance(); return make(TokenKind::QuestionColon, "?:"); }
            return make(TokenKind::QuestionMark, "?");
        case '@': return make(TokenKind::At, "@");
        case '#': return make(TokenKind::Hash, "#");
        case ',': return make(TokenKind::Comma, ",");
        case ';': return make(TokenKind::Semicolon, ";");
        case '(': return make(TokenKind::LParen, "(");
        case ')': return make(TokenKind::RParen, ")");
        case '{': return make(TokenKind::LBrace, "{");
        case '}': return make(TokenKind::RBrace, "}");
        case '[': return make(TokenKind::LBracket, "[");
        case ']': return make(TokenKind::RBracket, "]");
        default:
            emitError(std::string("Karakter tidak dikenal: '") + c + "'");
            return make(TokenKind::Unknown, std::string(1, c));
    }
}

// ---------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------
SourceLocation Lexer::currentLocation() const {
    return {filename_, line_, column_};
}

Token Lexer::makeToken(TokenKind kind, const std::string& value) const {
    return Token{kind, value, currentLocation()};
}

void Lexer::emitError(const std::string& msg) {
    errors_.push_back({CBaltError::Kind::Lexer, msg, currentLocation()});
}

// ---------------------------------------------------------------
//  Token::toString / kindName
// ---------------------------------------------------------------
std::string Token::kindName() const {
    switch (kind) {
        case TokenKind::IntLiteral:    return "IntLiteral";
        case TokenKind::FloatLiteral:  return "FloatLiteral";
        case TokenKind::StringLiteral: return "StringLiteral";
        case TokenKind::BoolLiteral:   return "BoolLiteral";
        case TokenKind::NullLiteral:   return "NullLiteral";
        case TokenKind::Identifier:    return "Identifier";
        case TokenKind::KW_let:        return "let";
        case TokenKind::KW_var:        return "var";
        case TokenKind::KW_const:      return "const";
        case TokenKind::KW_func:       return "func";
        case TokenKind::KW_return:     return "return";
        case TokenKind::KW_struct:     return "struct";
        case TokenKind::KW_if:         return "if";
        case TokenKind::KW_else:       return "else";
        case TokenKind::KW_while:      return "while";
        case TokenKind::KW_for:        return "for";
        case TokenKind::KW_in:         return "in";
        case TokenKind::KW_asm:        return "asm";
        case TokenKind::KW_unsafe:     return "unsafe";
        case TokenKind::KW_import:     return "import";
        case TokenKind::KW_async:      return "async";
        case TokenKind::KW_await:      return "await";
        case TokenKind::KW_spawn:      return "spawn";
        case TokenKind::Plus:          return "+";
        case TokenKind::Minus:         return "-";
        case TokenKind::Star:          return "*";
        case TokenKind::Slash:         return "/";
        case TokenKind::Equal:         return "=";
        case TokenKind::EqualEqual:    return "==";
        case TokenKind::BangEqual:     return "!=";
        case TokenKind::Arrow:         return "->";
        case TokenKind::At:            return "@";
        case TokenKind::LParen:        return "(";
        case TokenKind::RParen:        return ")";
        case TokenKind::LBrace:        return "{";
        case TokenKind::RBrace:        return "}";
        case TokenKind::Comma:         return ",";
        case TokenKind::Colon:         return ":";
        case TokenKind::Semicolon:     return ";";
        case TokenKind::Dot:           return ".";
        case TokenKind::EOF_:          return "EOF";
        default:                       return "?";
    }
}

std::string Token::toString() const {
    return "[" + kindName() + " | \"" + value + "\" | " + location.toString() + "]";
}

} // namespace cbalt
