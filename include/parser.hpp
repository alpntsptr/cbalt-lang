#pragma once

#include "cbalt.hpp"
#include "lexer.hpp"

namespace cbalt {

// ---------------------------------------------------------------
//  Forward Declarations
// ---------------------------------------------------------------
struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;
using ASTNodeList = std::vector<ASTNodePtr>;

// ---------------------------------------------------------------
//  Jenis Node AST
// ---------------------------------------------------------------
enum class NodeKind {
    // Program
    Program,

    // Statements
    BlockStmt,
    VarDecl,        // let / var / const
    FuncDecl,       // func
    StructDecl,     // struct
    EnumDecl,       // enum
    TraitDecl,      // trait
    ImplDecl,       // impl
    ImportDecl,     // import
    ExportDecl,     // export
    ReturnStmt,
    IfStmt,
    WhileStmt,
    ForInStmt,
    BreakStmt,
    ContinueStmt,
    MatchStmt,
    ExprStmt,
    AsmBlock,       // asm { ... }
    UnsafeBlock,    // unsafe { ... }
    AsyncFunc,      // async func
    SpawnStmt,      // spawn expr

    // Decorator / Attribute
    Decorator,      // @route, @entity, @component, ...

    // Expressions
    BinaryExpr,
    UnaryExpr,
    CallExpr,
    IndexExpr,      // arr[i]
    FieldExpr,      // obj.field
    PathExpr,       // Modul::Item
    AssignExpr,
    LambdaExpr,     // (a, b) => a + b
    AwaitExpr,      // await expr
    MatchExpr,
    IfExpr,         // if sebagai ekspresi (ternary-style)
    CastExpr,       // expr as Type
    SizeofExpr,     // sizeof(T)
    TypeofExpr,     // typeof(x)
    RangeExpr,      // start..end

    // Literals
    IdentifierExpr,
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    BoolLiteral,
    NullLiteral,
    ArrayLiteral,
    StructLiteral,  // Struct { field: value }

    // Types
    TypeName,
    ArrayType,      // [T; N]  /  [T]
    PtrType,        // ptr<T>
    RefType,        // ref<T>
    FuncType,       // func(A, B) -> C
    OptionalType,   // T?
    TupleType,      // (A, B, C)

    // Pattern (buat match)
    WildcardPattern,
    LiteralPattern,
    IdentPattern,
    StructPattern,
    EnumPattern,
    TuplePattern,
};

// ---------------------------------------------------------------
//  Type Info — representasi tipe saat parse/sema
// ---------------------------------------------------------------
struct TypeInfo {
    std::string       name;
    bool              isArray     = false;
    bool              isPointer   = false;
    bool              isReference = false;
    bool              isOptional  = false;
    bool              isMutable   = false;
    i64               arraySize   = -1;  // -1 = dynamic
    std::vector<TypeInfo> genericArgs;   // buat generics

    static TypeInfo simple(const std::string& n) { TypeInfo t; t.name = n; return t; }
    std::string toString() const;
};

// ---------------------------------------------------------------
//  Parameter Function
// ---------------------------------------------------------------
struct Param {
    std::string  name;
    TypeInfo     type;
    ASTNodePtr   defaultValue;  // optional default
    bool         isVariadic = false;
    SourceLocation location;
};

// ---------------------------------------------------------------
//  Base AST Node
// ---------------------------------------------------------------
struct ASTNode {
    NodeKind       kind;
    SourceLocation location;
    virtual ~ASTNode() = default;
    virtual std::string dump(i32 indent = 0) const = 0;
};

// ---------------------------------------------------------------
//  Statement Nodes
// ---------------------------------------------------------------

struct ProgramNode : ASTNode {
    ASTNodeList body;
    std::string dump(i32 indent = 0) const override;
};

struct BlockStmtNode : ASTNode {
    ASTNodeList stmts;
    std::string dump(i32 indent = 0) const override;
};

struct VarDeclNode : ASTNode {
    std::string  name;
    TypeInfo     type;
    ASTNodePtr   initializer;
    bool         isConst   = false;
    bool         isMutable = true;
    std::string dump(i32 indent = 0) const override;
};

struct FuncDeclNode : ASTNode {
    std::string           name;
    std::vector<Param>    params;
    TypeInfo              returnType;
    ASTNodePtr            body;          // BlockStmtNode
    bool                  isAsync  = false;
    bool                  isExtern = false;
    bool                  isPublic = false;
    std::vector<ASTNodePtr> decorators;
    std::string dump(i32 indent = 0) const override;
};

struct StructDeclNode : ASTNode {
    std::string                              name;
    std::vector<std::pair<std::string, TypeInfo>> fields;
    std::vector<ASTNodePtr>                  methods;
    std::vector<ASTNodePtr>                  decorators;
    std::string dump(i32 indent = 0) const override;
};

struct EnumDeclNode : ASTNode {
    std::string                              name;
    std::vector<std::pair<std::string, std::optional<ASTNodePtr>>> variants;
    std::string dump(i32 indent = 0) const override;
};

struct ImportDeclNode : ASTNode {
    std::string path;   // "core", "gfx", "net"
    std::string alias;  // as X
    bool        isStdlib = false;
    std::string dump(i32 indent = 0) const override;
};

struct ReturnStmtNode : ASTNode {
    ASTNodePtr value;  // optional
    std::string dump(i32 indent = 0) const override;
};

struct IfStmtNode : ASTNode {
    ASTNodePtr  condition;
    ASTNodePtr  thenBranch;
    ASTNodePtr  elseBranch;  // optional
    std::string dump(i32 indent = 0) const override;
};

struct WhileStmtNode : ASTNode {
    ASTNodePtr  condition;
    ASTNodePtr  body;
    std::string dump(i32 indent = 0) const override;
};

struct ForInStmtNode : ASTNode {
    std::string varName;
    ASTNodePtr  iterable;
    ASTNodePtr  body;
    std::string dump(i32 indent = 0) const override;
};

struct AsmBlockNode : ASTNode {
    std::string rawCode;    // Isi raw assembly
    std::string dump(i32 indent = 0) const override;
};

struct BreakStmtNode : ASTNode {
    std::string dump(i32 indent = 0) const override;
};

struct ContinueStmtNode : ASTNode {
    std::string dump(i32 indent = 0) const override;
};

struct UnsafeBlockNode : ASTNode {
    ASTNodePtr body;
    std::string dump(i32 indent = 0) const override;
};

struct DecoratorNode : ASTNode {
    std::string name;                    // "route", "entity", "component"
    std::vector<ASTNodePtr> args;
    std::string dump(i32 indent = 0) const override;
};

struct SpawnStmtNode : ASTNode {
    ASTNodePtr expr;
    std::string dump(i32 indent = 0) const override;
};

// ---------------------------------------------------------------
//  Expression Nodes
// ---------------------------------------------------------------

struct BinaryExprNode : ASTNode {
    std::string op;
    ASTNodePtr  left;
    ASTNodePtr  right;
    std::string dump(i32 indent = 0) const override;
};

struct UnaryExprNode : ASTNode {
    std::string op;
    ASTNodePtr  operand;
    bool        prefix = true;
    std::string dump(i32 indent = 0) const override;
};

struct CallExprNode : ASTNode {
    ASTNodePtr              callee;
    std::vector<ASTNodePtr> args;
    std::string dump(i32 indent = 0) const override;
};

struct FieldExprNode : ASTNode {
    ASTNodePtr  object;
    std::string field;
    std::string dump(i32 indent = 0) const override;
};

struct IndexExprNode : ASTNode {
    ASTNodePtr object;
    ASTNodePtr index;
    std::string dump(i32 indent = 0) const override;
};

struct AssignExprNode : ASTNode {
    std::string op;    // =, +=, -=, *=, /=, ...
    ASTNodePtr  target;
    ASTNodePtr  value;
    std::string dump(i32 indent = 0) const override;
};

struct LambdaExprNode : ASTNode {
    std::vector<Param> params;
    TypeInfo           returnType;
    ASTNodePtr         body;
    std::string dump(i32 indent = 0) const override;
};

struct CastExprNode : ASTNode {
    ASTNodePtr expr;
    TypeInfo   targetType;
    std::string dump(i32 indent = 0) const override;
};

// ---------------------------------------------------------------
//  Literal Nodes
// ---------------------------------------------------------------

struct IntLiteralNode : ASTNode {
    i64 value;
    std::string dump(i32 indent = 0) const override;
};

struct FloatLiteralNode : ASTNode {
    f64 value;
    std::string dump(i32 indent = 0) const override;
};

struct StringLiteralNode : ASTNode {
    std::string value;
    std::string dump(i32 indent = 0) const override;
};

struct BoolLiteralNode : ASTNode {
    bool value;
    std::string dump(i32 indent = 0) const override;
};

struct NullLiteralNode : ASTNode {
    std::string dump(i32 indent = 0) const override;
};

struct IdentifierNode : ASTNode {
    std::string name;
    std::string dump(i32 indent = 0) const override;
};

struct ArrayLiteralNode : ASTNode {
    std::vector<ASTNodePtr> elements;
    std::string dump(i32 indent = 0) const override;
};

// ---------------------------------------------------------------
//  ExprStmt
// ---------------------------------------------------------------
struct ExprStmtNode : ASTNode {
    ASTNodePtr expr;
    std::string dump(i32 indent = 0) const override;
};

// ---------------------------------------------------------------
//  Parser — Membangun AST dari Token Stream
// ---------------------------------------------------------------
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    ASTNodePtr parse();

    const std::vector<CBaltError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::vector<Token> tokens_;
    size_t             pos_ = 0;
    std::vector<CBaltError> errors_;

    // --- Token helpers ---
    const Token& current() const;
    const Token& peek(i32 offset = 1) const;
    Token        consume();
    Token        expect(TokenKind kind, const std::string& msg = "");
    bool         check(TokenKind kind) const;
    bool         match(TokenKind kind);
    bool         matchAny(std::initializer_list<TokenKind> kinds);
    bool         isAtEnd() const;

    // --- Top-level parsers ---
    ASTNodePtr parseTopLevel();
    ASTNodePtr parseImport();
    ASTNodePtr parseFuncDecl(std::vector<ASTNodePtr> decorators = {});
    ASTNodePtr parseStructDecl(std::vector<ASTNodePtr> decorators = {});
    ASTNodePtr parseEnumDecl();
    ASTNodePtr parseVarDecl();
    std::vector<ASTNodePtr> parseDecorators();
    ASTNodePtr parseDecorator();

    // --- Statement parsers ---
    ASTNodePtr parseStatement();
    ASTNodePtr parseBlock();
    ASTNodePtr parseIf();
    ASTNodePtr parseWhile();
    ASTNodePtr parseForIn();
    ASTNodePtr parseReturn();
    ASTNodePtr parseAsmBlock();
    ASTNodePtr parseUnsafeBlock();
    ASTNodePtr parseSpawn();

    // --- Expression parsers (Pratt style) ---
    ASTNodePtr parseExpr(i32 minPrec = 0);
    ASTNodePtr parsePrimary();
    ASTNodePtr parseUnary();
    ASTNodePtr parseCall(ASTNodePtr callee);
    ASTNodePtr parseIndex(ASTNodePtr object);
    ASTNodePtr parseField(ASTNodePtr object);
    ASTNodePtr parseAssign(ASTNodePtr target);
    ASTNodePtr parseLambda();
    ASTNodePtr parseCast(ASTNodePtr expr);
    ASTNodePtr parseArrayLiteral();
    ASTNodePtr postfixExpr(ASTNodePtr expr);
    bool       isLambdaStart() const;

    // --- Type parsers ---
    TypeInfo parseType();
    TypeInfo parseArrayType();

    // --- Param parsers ---
    std::vector<Param> parseParams();
    Param parseParam();

    // --- Helpers ---
    i32  getInfixPrecedence(const Token& tok) const;
    bool isInfixOp(const Token& tok) const;
    void emitError(const std::string& msg, std::optional<SourceLocation> loc = std::nullopt);
    void synchronize(); // error recovery — skip ke statement berikutnya
};

} // namespace cbalt
