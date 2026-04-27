#include "parser.hpp"
#include <sstream>
#include <cassert>

namespace cbalt {

// ---------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------
Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)) {}

// ---------------------------------------------------------------
//  Public: parse
// ---------------------------------------------------------------
ASTNodePtr Parser::parse() {
    auto prog = std::make_unique<ProgramNode>();
    prog->kind     = NodeKind::Program;
    prog->location = tokens_.empty() ? SourceLocation{} : tokens_[0].location;

    while (!isAtEnd()) {
        if (check(TokenKind::EOF_)) break;
        try {
            auto node = parseTopLevel();
            if (node) prog->body.push_back(std::move(node));
        } catch (...) {
            synchronize();
        }
    }
    return prog;
}

// ---------------------------------------------------------------
//  Token helpers
// ---------------------------------------------------------------
const Token& Parser::current() const {
    if (pos_ >= tokens_.size()) return tokens_.back();
    return tokens_[pos_];
}

const Token& Parser::peek(i32 offset) const {
    size_t p = pos_ + offset;
    if (p >= tokens_.size()) return tokens_.back();
    return tokens_[p];
}

Token Parser::consume() {
    if (isAtEnd()) return tokens_.back();
    return tokens_[pos_++];
}

Token Parser::expect(TokenKind kind, const std::string& msg) {
    if (!check(kind)) {
        std::string errMsg = msg.empty()
            ? "Ekspektasi '" + Token{kind, "", {}}.kindName() + "', dapat '" + current().value + "'"
            : msg;
        emitError(errMsg);
        // Return dummy biar gak crash
        return Token{kind, "", current().location};
    }
    return consume();
}

bool Parser::check(TokenKind kind) const {
    return current().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) { consume(); return true; }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenKind> kinds) {
    for (auto k : kinds) if (check(k)) { consume(); return true; }
    return false;
}

bool Parser::isAtEnd() const {
    return pos_ >= tokens_.size() || tokens_[pos_].kind == TokenKind::EOF_;
}

// ---------------------------------------------------------------
//  Top-level parser
// ---------------------------------------------------------------
ASTNodePtr Parser::parseTopLevel() {
    // Decorator sebelum deklarasi
    std::vector<ASTNodePtr> decorators;
    while (check(TokenKind::At)) {
        decorators.push_back(parseDecorator());
    }

    if (check(TokenKind::KW_import))  return parseImport();
    if (check(TokenKind::KW_func))    return parseFuncDecl(std::move(decorators));
    if (check(TokenKind::KW_async) && peek().kind == TokenKind::KW_func)
                                      return parseFuncDecl(std::move(decorators));
    if (check(TokenKind::KW_struct))  return parseStructDecl(std::move(decorators));
    if (check(TokenKind::KW_enum))    return parseEnumDecl();
    if (check(TokenKind::KW_let) ||
        check(TokenKind::KW_var) ||
        check(TokenKind::KW_const))   return parseVarDecl();

    return parseStatement();
}

// ---------------------------------------------------------------
//  parseImport
// ---------------------------------------------------------------
ASTNodePtr Parser::parseImport() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_import);

    auto node = std::make_unique<ImportDeclNode>();
    node->kind     = NodeKind::ImportDecl;
    node->location = loc;

    // import "path" atau import identifier
    if (check(TokenKind::StringLiteral)) {
        node->path = consume().value;
    } else {
        node->path = expect(TokenKind::Identifier, "Ekspektasi nama modul setelah 'import'").value;
    }

    // as alias
    if (match(TokenKind::KW_as)) {
        node->alias = expect(TokenKind::Identifier, "Ekspektasi alias setelah 'as'").value;
    }

    match(TokenKind::Semicolon);
    return node;
}

// ---------------------------------------------------------------
//  parseDecorator  @name(args...)
// ---------------------------------------------------------------
ASTNodePtr Parser::parseDecorator() {
    SourceLocation loc = current().location;
    expect(TokenKind::At);
    auto node = std::make_unique<DecoratorNode>();
    node->kind     = NodeKind::Decorator;
    node->location = loc;
    node->name     = expect(TokenKind::Identifier, "Ekspektasi nama decorator setelah '@'").value;

    if (match(TokenKind::LParen)) {
        while (!check(TokenKind::RParen) && !isAtEnd()) {
            node->args.push_back(parseExpr());
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "Ekspektasi ')' setelah argumen decorator");
    }
    return node;
}

// ---------------------------------------------------------------
//  parseFuncDecl
// ---------------------------------------------------------------
ASTNodePtr Parser::parseFuncDecl(std::vector<ASTNodePtr> decorators) {
    SourceLocation loc = current().location;
    bool isAsync = false;
    if (check(TokenKind::KW_async)) { isAsync = true; consume(); }
    expect(TokenKind::KW_func, "Ekspektasi 'func'");

    auto node = std::make_unique<FuncDeclNode>();
    node->kind       = NodeKind::FuncDecl;
    node->location   = loc;
    node->isAsync    = isAsync;
    node->decorators = std::move(decorators);
    node->name       = expect(TokenKind::Identifier, "Ekspektasi nama fungsi").value;

    // Parameter
    expect(TokenKind::LParen, "Ekspektasi '(' setelah nama fungsi");
    node->params = parseParams();
    expect(TokenKind::RParen, "Ekspektasi ')' setelah parameter");

    // Return type
    if (match(TokenKind::Arrow)) {
        node->returnType = parseType();
    } else {
        node->returnType = TypeInfo::simple("void");
    }

    // Body atau extern ;
    if (check(TokenKind::Semicolon)) {
        node->isExtern = true;
        consume();
    } else {
        node->body = parseBlock();
    }

    return node;
}

// ---------------------------------------------------------------
//  parseStructDecl
// ---------------------------------------------------------------
ASTNodePtr Parser::parseStructDecl(std::vector<ASTNodePtr> decorators) {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_struct);

    auto node = std::make_unique<StructDeclNode>();
    node->kind       = NodeKind::StructDecl;
    node->location   = loc;
    node->decorators = std::move(decorators);
    node->name       = expect(TokenKind::Identifier, "Ekspektasi nama struct").value;

    expect(TokenKind::LBrace, "Ekspektasi '{' setelah nama struct");

    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // Method di dalam struct
        if (check(TokenKind::KW_func) || check(TokenKind::KW_async)) {
            node->methods.push_back(parseFuncDecl());
            continue;
        }
        // Field: name : Type
        std::string fieldName = expect(TokenKind::Identifier, "Ekspektasi nama field").value;
        expect(TokenKind::Colon, "Ekspektasi ':' setelah nama field");
        TypeInfo fieldType = parseType();
        node->fields.push_back({fieldName, fieldType});
        match(TokenKind::Comma);
        match(TokenKind::Semicolon);
    }

    expect(TokenKind::RBrace, "Ekspektasi '}' setelah struct body");
    return node;
}

// ---------------------------------------------------------------
//  parseEnumDecl
// ---------------------------------------------------------------
ASTNodePtr Parser::parseEnumDecl() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_enum);
    auto node = std::make_unique<EnumDeclNode>();
    node->kind     = NodeKind::EnumDecl;
    node->location = loc;
    node->name     = expect(TokenKind::Identifier, "Ekspektasi nama enum").value;

    expect(TokenKind::LBrace);
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        std::string variantName = expect(TokenKind::Identifier, "Ekspektasi nama variant").value;
        std::optional<ASTNodePtr> value;
        if (match(TokenKind::Equal)) {
            value = parseExpr();
        }
        node->variants.push_back({variantName, std::move(value)});
        match(TokenKind::Comma);
    }
    expect(TokenKind::RBrace);
    return node;
}

// ---------------------------------------------------------------
//  parseVarDecl
// ---------------------------------------------------------------
ASTNodePtr Parser::parseVarDecl() {
    SourceLocation loc = current().location;
    auto node = std::make_unique<VarDeclNode>();
    node->kind     = NodeKind::VarDecl;
    node->location = loc;

    if (check(TokenKind::KW_const)) {
        node->isConst   = true;
        node->isMutable = false;
        consume();
    } else if (check(TokenKind::KW_let)) {
        node->isMutable = false;
        consume();
    } else {
        node->isMutable = true;
        consume(); // var
    }

    node->name = expect(TokenKind::Identifier, "Ekspektasi nama variabel").value;

    if (match(TokenKind::Colon)) {
        node->type = parseType();
    }

    if (match(TokenKind::Equal)) {
        node->initializer = parseExpr();
    }

    match(TokenKind::Semicolon);
    return node;
}

// ---------------------------------------------------------------
//  parseBlock
// ---------------------------------------------------------------
ASTNodePtr Parser::parseBlock() {
    SourceLocation loc = current().location;
    expect(TokenKind::LBrace, "Ekspektasi '{'");
    auto node = std::make_unique<BlockStmtNode>();
    node->kind     = NodeKind::BlockStmt;
    node->location = loc;

    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) node->stmts.push_back(std::move(stmt));
    }

    expect(TokenKind::RBrace, "Ekspektasi '}'");
    return node;
}

// ---------------------------------------------------------------
//  parseStatement
// ---------------------------------------------------------------
ASTNodePtr Parser::parseStatement() {
    if (check(TokenKind::KW_return))   return parseReturn();
    if (check(TokenKind::KW_if))       return parseIf();
    if (check(TokenKind::KW_while))    return parseWhile();
    if (check(TokenKind::KW_for))      return parseForIn();
    if (check(TokenKind::KW_asm))      return parseAsmBlock();
    if (check(TokenKind::KW_unsafe))   return parseUnsafeBlock();
    if (check(TokenKind::KW_spawn))    return parseSpawn();
    if (check(TokenKind::KW_break)) {
        auto n = std::make_unique<BreakStmtNode>();
        n->kind = NodeKind::BreakStmt; n->location = consume().location;
        match(TokenKind::Semicolon);
        return n;
    }
    if (check(TokenKind::KW_continue)) {
        auto n = std::make_unique<ContinueStmtNode>();
        n->kind = NodeKind::ContinueStmt; n->location = consume().location;
        match(TokenKind::Semicolon);
        return n;
    }
    if (check(TokenKind::KW_let) || check(TokenKind::KW_var) || check(TokenKind::KW_const))
        return parseVarDecl();
    if (check(TokenKind::KW_func))
        return parseFuncDecl();

    // Expression statement
    SourceLocation loc = current().location;
    auto expr = parseExpr();
    match(TokenKind::Semicolon);
    auto stmtNode = std::make_unique<ExprStmtNode>();
    stmtNode->kind     = NodeKind::ExprStmt;
    stmtNode->location = loc;
    stmtNode->expr     = std::move(expr);
    return stmtNode;
}

// ---------------------------------------------------------------
//  parseIf
// ---------------------------------------------------------------
ASTNodePtr Parser::parseIf() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_if);
    auto node = std::make_unique<IfStmtNode>();
    node->kind      = NodeKind::IfStmt;
    node->location  = loc;
    node->condition = parseExpr();
    node->thenBranch = parseBlock();

    if (check(TokenKind::KW_elif)) {
        node->elseBranch = parseIf(); // rekursif
    } else if (match(TokenKind::KW_else)) {
        node->elseBranch = parseBlock();
    }
    return node;
}

// ---------------------------------------------------------------
//  parseWhile
// ---------------------------------------------------------------
ASTNodePtr Parser::parseWhile() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_while);
    auto node = std::make_unique<WhileStmtNode>();
    node->kind      = NodeKind::WhileStmt;
    node->location  = loc;
    node->condition = parseExpr();
    node->body      = parseBlock();
    return node;
}

// ---------------------------------------------------------------
//  parseForIn
// ---------------------------------------------------------------
ASTNodePtr Parser::parseForIn() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_for);
    auto node = std::make_unique<ForInStmtNode>();
    node->kind    = NodeKind::ForInStmt;
    node->location = loc;
    node->varName  = expect(TokenKind::Identifier, "Ekspektasi nama variabel loop").value;
    expect(TokenKind::KW_in, "Ekspektasi 'in' setelah nama variabel");
    node->iterable = parseExpr();
    node->body     = parseBlock();
    return node;
}

// ---------------------------------------------------------------
//  parseReturn
// ---------------------------------------------------------------
ASTNodePtr Parser::parseReturn() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_return);
    auto node = std::make_unique<ReturnStmtNode>();
    node->kind     = NodeKind::ReturnStmt;
    node->location = loc;

    if (!check(TokenKind::Semicolon) && !check(TokenKind::RBrace) && !isAtEnd()) {
        node->value = parseExpr();
    }
    match(TokenKind::Semicolon);
    return node;
}

// ---------------------------------------------------------------
//  parseAsmBlock — asm { raw code }
// ---------------------------------------------------------------
ASTNodePtr Parser::parseAsmBlock() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_asm);
    expect(TokenKind::LBrace, "Ekspektasi '{' setelah 'asm'");

    auto node = std::make_unique<AsmBlockNode>();
    node->kind     = NodeKind::AsmBlock;
    node->location = loc;

    // Kumpulkan semua sampai RBrace
    std::string raw;
    int depth = 1;
    while (!isAtEnd() && depth > 0) {
        if (check(TokenKind::LBrace)) depth++;
        else if (check(TokenKind::RBrace)) { depth--; if (depth == 0) break; }
        raw += current().value + " ";
        consume();
    }
    expect(TokenKind::RBrace, "Ekspektasi '}' untuk menutup asm block");
    node->rawCode = raw;
    return node;
}

// ---------------------------------------------------------------
//  parseUnsafeBlock
// ---------------------------------------------------------------
ASTNodePtr Parser::parseUnsafeBlock() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_unsafe);
    auto node = std::make_unique<UnsafeBlockNode>();
    node->kind     = NodeKind::UnsafeBlock;
    node->location = loc;
    node->body     = parseBlock();
    return node;
}

// ---------------------------------------------------------------
//  parseSpawn — spawn expr
// ---------------------------------------------------------------
ASTNodePtr Parser::parseSpawn() {
    SourceLocation loc = current().location;
    expect(TokenKind::KW_spawn);
    auto node = std::make_unique<SpawnStmtNode>();
    node->kind     = NodeKind::SpawnStmt;
    node->location = loc;
    node->expr     = parseExpr();
    match(TokenKind::Semicolon);
    return node;
}

// ---------------------------------------------------------------
//  parseExpr — Pratt Parser
// ---------------------------------------------------------------
ASTNodePtr Parser::parseExpr(i32 minPrec) {
    auto left = parseUnary();

    // Assign
    if (check(TokenKind::Equal) || check(TokenKind::PlusEqual) ||
        check(TokenKind::MinusEqual) || check(TokenKind::StarEqual) ||
        check(TokenKind::SlashEqual)) {
        return parseAssign(std::move(left));
    }

    // Cast: expr as Type
    if (check(TokenKind::KW_as)) {
        return parseCast(std::move(left));
    }

    // Infix operators
    while (isInfixOp(current())) {
        i32 prec = getInfixPrecedence(current());
        if (prec < minPrec) break;

        Token opTok = consume();
        auto right  = parseExpr(prec + 1);

        auto bin = std::make_unique<BinaryExprNode>();
        bin->kind     = NodeKind::BinaryExpr;
        bin->location = opTok.location;
        bin->op       = opTok.value;
        bin->left     = std::move(left);
        bin->right    = std::move(right);
        left = std::move(bin);
    }

    return left;
}

// ---------------------------------------------------------------
//  parseUnary
// ---------------------------------------------------------------
ASTNodePtr Parser::parseUnary() {
    if (check(TokenKind::Bang) || check(TokenKind::Minus) || check(TokenKind::Tilde)) {
        SourceLocation loc = current().location;
        std::string op = consume().value;
        auto operand = parseUnary();
        auto node = std::make_unique<UnaryExprNode>();
        node->kind     = NodeKind::UnaryExpr;
        node->location = loc;
        node->op       = op;
        node->operand  = std::move(operand);
        node->prefix   = true;
        return node;
    }
    return parsePrimary();
}

// ---------------------------------------------------------------
//  parsePrimary
// ---------------------------------------------------------------
ASTNodePtr Parser::parsePrimary() {
    Token tok = current();

    if (check(TokenKind::LParen) && isLambdaStart()) {
        return parseLambda();
    }

    // Literal int
    if (tok.kind == TokenKind::IntLiteral) {
        consume();
        auto n = std::make_unique<IntLiteralNode>();
        n->kind     = NodeKind::IntLiteral;
        n->location = tok.location;
        n->value    = std::stoll(tok.value, nullptr, 0);
        return postfixExpr(std::move(n));
    }

    // Literal float
    if (tok.kind == TokenKind::FloatLiteral) {
        consume();
        auto n = std::make_unique<FloatLiteralNode>();
        n->kind     = NodeKind::FloatLiteral;
        n->location = tok.location;
        n->value    = std::stod(tok.value);
        return postfixExpr(std::move(n));
    }

    // Literal string
    if (tok.kind == TokenKind::StringLiteral) {
        consume();
        auto n = std::make_unique<StringLiteralNode>();
        n->kind     = NodeKind::StringLiteral;
        n->location = tok.location;
        n->value    = tok.value;
        return postfixExpr(std::move(n));
    }

    // Literal bool
    if (tok.kind == TokenKind::BoolLiteral) {
        consume();
        auto n = std::make_unique<BoolLiteralNode>();
        n->kind     = NodeKind::BoolLiteral;
        n->location = tok.location;
        n->value    = (tok.value == "true");
        return n;
    }

    // Null
    if (tok.kind == TokenKind::NullLiteral) {
        consume();
        auto n = std::make_unique<NullLiteralNode>();
        n->kind = NodeKind::NullLiteral; n->location = tok.location;
        return n;
    }

    // Identifier
    if (tok.kind == TokenKind::Identifier) {
        consume();
        auto id = std::make_unique<IdentifierNode>();
        id->kind     = NodeKind::IdentifierExpr;
        id->location = tok.location;
        id->name     = tok.value;
        return postfixExpr(std::move(id));
    }

    // Grouped expr: ( expr )
    if (check(TokenKind::LParen)) {
        consume();
        auto expr = parseExpr();
        expect(TokenKind::RParen, "Ekspektasi ')' untuk menutup ekspresi");
        return postfixExpr(std::move(expr));
    }

    // Array literal
    if (check(TokenKind::LBracket)) {
        return parseArrayLiteral();
    }

    emitError("Ekspresi tidak valid: '" + current().value + "'");
    consume();
    auto dummy = std::make_unique<IntLiteralNode>();
    dummy->kind = NodeKind::IntLiteral;
    dummy->value = 0;
    return dummy;
}

// Postfix: calls, indexing, field access
ASTNodePtr Parser::postfixExpr(ASTNodePtr expr) {
    while (true) {
        if (check(TokenKind::LParen))   { expr = parseCall(std::move(expr));  continue; }
        if (check(TokenKind::LBracket)) { expr = parseIndex(std::move(expr)); continue; }
        if (check(TokenKind::Dot))      { expr = parseField(std::move(expr)); continue; }
        break;
    }
    return expr;
}

// ---------------------------------------------------------------
//  parseCall
// ---------------------------------------------------------------
ASTNodePtr Parser::parseCall(ASTNodePtr callee) {
    SourceLocation loc = current().location;
    expect(TokenKind::LParen);
    auto node = std::make_unique<CallExprNode>();
    node->kind     = NodeKind::CallExpr;
    node->location = loc;
    node->callee   = std::move(callee);

    while (!check(TokenKind::RParen) && !isAtEnd()) {
        node->args.push_back(parseExpr());
        if (!match(TokenKind::Comma)) break;
    }
    expect(TokenKind::RParen, "Ekspektasi ')' setelah argumen");
    return node;
}

// ---------------------------------------------------------------
//  parseIndex
// ---------------------------------------------------------------
ASTNodePtr Parser::parseIndex(ASTNodePtr object) {
    SourceLocation loc = current().location;
    expect(TokenKind::LBracket);
    auto node = std::make_unique<IndexExprNode>();
    node->kind     = NodeKind::IndexExpr;
    node->location = loc;
    node->object   = std::move(object);
    node->index    = parseExpr();
    expect(TokenKind::RBracket, "Ekspektasi ']'");
    return node;
}

// ---------------------------------------------------------------
//  parseField
// ---------------------------------------------------------------
ASTNodePtr Parser::parseField(ASTNodePtr object) {
    SourceLocation loc = current().location;
    expect(TokenKind::Dot);
    auto node = std::make_unique<FieldExprNode>();
    node->kind     = NodeKind::FieldExpr;
    node->location = loc;
    node->object   = std::move(object);
    node->field    = expect(TokenKind::Identifier, "Ekspektasi nama field setelah '.'").value;
    return node;
}

// ---------------------------------------------------------------
//  parseAssign
// ---------------------------------------------------------------
ASTNodePtr Parser::parseAssign(ASTNodePtr target) {
    SourceLocation loc = current().location;
    std::string op = consume().value;
    auto value = parseExpr();
    auto node = std::make_unique<AssignExprNode>();
    node->kind     = NodeKind::AssignExpr;
    node->location = loc;
    node->op       = op;
    node->target   = std::move(target);
    node->value    = std::move(value);
    return node;
}

// ---------------------------------------------------------------
//  parseCast — expr as Type
// ---------------------------------------------------------------
ASTNodePtr Parser::parseCast(ASTNodePtr expr) {
    SourceLocation loc = current().location;
    consume(); // 'as'
    auto node = std::make_unique<CastExprNode>();
    node->kind       = NodeKind::CastExpr;
    node->location   = loc;
    node->expr       = std::move(expr);
    node->targetType = parseType();
    return node;
}

// ---------------------------------------------------------------
//  parseArrayLiteral
// ---------------------------------------------------------------
ASTNodePtr Parser::parseArrayLiteral() {
    SourceLocation loc = current().location;
    expect(TokenKind::LBracket);
    auto node = std::make_unique<ArrayLiteralNode>();
    node->kind     = NodeKind::ArrayLiteral;
    node->location = loc;

    while (!check(TokenKind::RBracket) && !isAtEnd()) {
        node->elements.push_back(parseExpr());
        if (!match(TokenKind::Comma)) break;
    }
    expect(TokenKind::RBracket, "Ekspektasi ']'");
    return node;
}

// ---------------------------------------------------------------
//  parseLambda — (params) => body
// ---------------------------------------------------------------
ASTNodePtr Parser::parseLambda() {
    SourceLocation loc = current().location;
    auto node = std::make_unique<LambdaExprNode>();
    node->kind     = NodeKind::LambdaExpr;
    node->location = loc;

    expect(TokenKind::LParen);
    node->params = parseParams();
    expect(TokenKind::RParen);

    if (match(TokenKind::Arrow)) {
        node->returnType = parseType();
    }

    expect(TokenKind::FatArrow, "Ekspektasi '=>' untuk lambda");
    if (check(TokenKind::LBrace)) {
        node->body = parseBlock();
    } else {
        auto retNode = std::make_unique<ReturnStmtNode>();
        retNode->kind  = NodeKind::ReturnStmt;
        retNode->value = parseExpr();
        node->body     = std::move(retNode);
    }
    return node;
}

bool Parser::isLambdaStart() const {
    // Lookahead: ( ident : Type ) => ...
    // Simplified detection — hanya cek apakah ada => sebelum {
    for (size_t i = pos_; i < tokens_.size() && i < pos_ + 20; i++) {
        if (tokens_[i].kind == TokenKind::FatArrow) return true;
        if (tokens_[i].kind == TokenKind::LBrace)   return false;
    }
    return false;
}

// ---------------------------------------------------------------
//  parseType
// ---------------------------------------------------------------
TypeInfo Parser::parseType() {
    TypeInfo t;

    if (check(TokenKind::LBracket)) {
        return parseArrayType();
    }

    if (check(TokenKind::KW_ptr)) {
        consume();
        expect(TokenKind::Less);
        auto inner = parseType();
        expect(TokenKind::Greater);
        t.name = "ptr"; t.isPointer = true;
        t.genericArgs.push_back(inner);
        return t;
    }

    if (check(TokenKind::KW_ref)) {
        consume();
        expect(TokenKind::Less);
        auto inner = parseType();
        expect(TokenKind::Greater);
        t.name = "ref"; t.isReference = true;
        t.genericArgs.push_back(inner);
        return t;
    }

    // Nama tipe dasar / custom
    if (check(TokenKind::Identifier) ||
        check(TokenKind::KW_int) ||
        check(TokenKind::KW_i8) ||
        check(TokenKind::KW_i16) ||
        check(TokenKind::KW_i32) ||
        check(TokenKind::KW_i64) ||
        check(TokenKind::KW_u8) ||
        check(TokenKind::KW_u16) ||
        check(TokenKind::KW_u32) ||
        check(TokenKind::KW_u64) ||
        check(TokenKind::KW_float) ||
        check(TokenKind::KW_f32) ||
        check(TokenKind::KW_f64) ||
        check(TokenKind::KW_bool) ||
        check(TokenKind::KW_string) ||
        check(TokenKind::KW_void) ||
        check(TokenKind::KW_any) ||
        check(TokenKind::KW_never)) {
        t.name = consume().value;
    } else {
        emitError("Ekspektasi nama tipe");
        t.name = "unknown";
    }

    // Optional: T?
    if (match(TokenKind::QuestionMark)) {
        t.isOptional = true;
    }

    return t;
}

TypeInfo Parser::parseArrayType() {
    TypeInfo t;
    expect(TokenKind::LBracket);
    t.isArray = true;
    t.genericArgs.push_back(parseType());
    if (match(TokenKind::Semicolon)) {
        // Array berukuran tetap: [T; N]
        if (check(TokenKind::IntLiteral)) {
            t.arraySize = std::stoll(consume().value);
        }
    }
    expect(TokenKind::RBracket, "Ekspektasi ']'");
    t.name = "array";
    return t;
}

// ---------------------------------------------------------------
//  parseParams
// ---------------------------------------------------------------
std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    while (!check(TokenKind::RParen) && !isAtEnd()) {
        params.push_back(parseParam());
        if (!match(TokenKind::Comma)) break;
    }
    return params;
}

Param Parser::parseParam() {
    Param p;
    p.location = current().location;

    // Variadic: ...name
    if (check(TokenKind::DotDotDot)) {
        consume();
        p.isVariadic = true;
    }

    p.name = expect(TokenKind::Identifier, "Ekspektasi nama parameter").value;

    if (match(TokenKind::Colon)) {
        p.type = parseType();
    }

    if (match(TokenKind::Equal)) {
        p.defaultValue = parseExpr();
    }

    return p;
}

// ---------------------------------------------------------------
//  Operator precedence
// ---------------------------------------------------------------
i32 Parser::getInfixPrecedence(const Token& tok) const {
    switch (tok.kind) {
        case TokenKind::PipePipe:    return 1;
        case TokenKind::AmpAmp:      return 2;
        case TokenKind::Pipe:        return 3;
        case TokenKind::Caret:       return 4;
        case TokenKind::Ampersand:   return 5;
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:   return 6;
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::LessEqual:
        case TokenKind::GreaterEqual: return 7;
        case TokenKind::ShiftLeft:
        case TokenKind::ShiftRight:  return 8;
        case TokenKind::Plus:
        case TokenKind::Minus:       return 9;
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:     return 10;
        case TokenKind::StarStar:    return 11;
        default:                     return -1;
    }
}

bool Parser::isInfixOp(const Token& tok) const {
    return getInfixPrecedence(tok) >= 0;
}

// ---------------------------------------------------------------
//  Error handling
// ---------------------------------------------------------------
void Parser::emitError(const std::string& msg, std::optional<SourceLocation> loc) {
    SourceLocation l = loc.value_or(current().location);
    errors_.push_back({CBaltError::Kind::Parser, msg, l});
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (check(TokenKind::Semicolon)) { consume(); return; }
        switch (current().kind) {
            case TokenKind::KW_func:
            case TokenKind::KW_struct:
            case TokenKind::KW_let:
            case TokenKind::KW_var:
            case TokenKind::KW_if:
            case TokenKind::KW_while:
            case TokenKind::KW_for:
            case TokenKind::KW_return:
                return;
            default: consume();
        }
    }
}

// ---------------------------------------------------------------
//  AST dump helpers
// ---------------------------------------------------------------
static std::string indent(i32 n) { return std::string(n * 2, ' '); }

std::string ProgramNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Program\n";
    for (const auto& n : body) if (n) ss << n->dump(i+1);
    return ss.str();
}

std::string BlockStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Block\n";
    for (const auto& n : stmts) if (n) ss << n->dump(i+1);
    return ss.str();
}

std::string VarDeclNode::dump(i32 i) const {
    return indent(i) + "VarDecl(" + name + ": " + type.name + ")\n";
}

std::string FuncDeclNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "FuncDecl(" << name << ") -> " << returnType.name << "\n";
    if (body) ss << body->dump(i+1);
    return ss.str();
}

std::string StructDeclNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "StructDecl(" << name << ")\n";
    for (auto& [fn, ft] : fields) ss << indent(i+1) << fn << ": " << ft.name << "\n";
    return ss.str();
}

std::string EnumDeclNode::dump(i32 i) const {
    return indent(i) + "EnumDecl(" + name + ")\n";
}

std::string ImportDeclNode::dump(i32 i) const {
    return indent(i) + "Import(" + path + ")\n";
}

std::string ReturnStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Return\n";
    if (value) ss << value->dump(i+1);
    return ss.str();
}

std::string IfStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "If\n";
    if (condition)   ss << indent(i+1) << "Cond:\n" << condition->dump(i+2);
    if (thenBranch)  ss << indent(i+1) << "Then:\n" << thenBranch->dump(i+2);
    if (elseBranch)  ss << indent(i+1) << "Else:\n" << elseBranch->dump(i+2);
    return ss.str();
}

std::string WhileStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "While\n";
    if (condition) ss << condition->dump(i+1);
    if (body)      ss << body->dump(i+1);
    return ss.str();
}

std::string ForInStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "ForIn(" << varName << ")\n";
    if (body) ss << body->dump(i+1);
    return ss.str();
}

std::string AsmBlockNode::dump(i32 i) const {
    return indent(i) + "AsmBlock { " + rawCode.substr(0, 40) + "... }\n";
}

std::string BreakStmtNode::dump(i32 i) const {
    return indent(i) + "Break\n";
}

std::string ContinueStmtNode::dump(i32 i) const {
    return indent(i) + "Continue\n";
}

std::string UnsafeBlockNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Unsafe\n";
    if (body) ss << body->dump(i+1);
    return ss.str();
}

std::string DecoratorNode::dump(i32 i) const {
    return indent(i) + "@" + name + "\n";
}

std::string SpawnStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Spawn\n";
    if (expr) ss << expr->dump(i+1);
    return ss.str();
}

std::string BinaryExprNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Binary(" << op << ")\n";
    if (left)  ss << left->dump(i+1);
    if (right) ss << right->dump(i+1);
    return ss.str();
}

std::string UnaryExprNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Unary(" << op << ")\n";
    if (operand) ss << operand->dump(i+1);
    return ss.str();
}

std::string CallExprNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Call\n";
    if (callee) ss << callee->dump(i+1);
    for (auto& a : args) if (a) ss << a->dump(i+1);
    return ss.str();
}

std::string FieldExprNode::dump(i32 i) const {
    return indent(i) + "Field(." + field + ")\n";
}

std::string IndexExprNode::dump(i32 i) const {
    return indent(i) + "Index\n";
}

std::string AssignExprNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Assign(" << op << ")\n";
    if (target) ss << target->dump(i+1);
    if (value)  ss << value->dump(i+1);
    return ss.str();
}

std::string LambdaExprNode::dump(i32 i) const {
    return indent(i) + "Lambda\n";
}

std::string CastExprNode::dump(i32 i) const {
    return indent(i) + "Cast(as " + targetType.name + ")\n";
}

std::string IntLiteralNode::dump(i32 i) const {
    return indent(i) + "Int(" + std::to_string(value) + ")\n";
}

std::string FloatLiteralNode::dump(i32 i) const {
    return indent(i) + "Float(" + std::to_string(value) + ")\n";
}

std::string StringLiteralNode::dump(i32 i) const {
    return indent(i) + "String(\"" + value + "\")\n";
}

std::string BoolLiteralNode::dump(i32 i) const {
    return indent(i) + std::string("Bool(") + (value ? "true" : "false") + ")\n";
}

std::string NullLiteralNode::dump(i32 i) const {
    return indent(i) + "Null\n";
}

std::string IdentifierNode::dump(i32 i) const {
    return indent(i) + "Ident(" + name + ")\n";
}

std::string ArrayLiteralNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "Array[" << elements.size() << "]\n";
    return ss.str();
}

std::string ExprStmtNode::dump(i32 i) const {
    std::ostringstream ss;
    ss << indent(i) << "ExprStmt\n";
    if (expr) ss << expr->dump(i+1);
    return ss.str();
}

std::string TypeInfo::toString() const {
    std::string s = name;
    if (isOptional) s += "?";
    if (isPointer)  s = "ptr<" + s + ">";
    if (isArray)    s = "[" + s + "]";
    return s;
}

} // namespace cbalt
