#pragma once

#include "cbalt.hpp"
#include "parser.hpp"

namespace cbalt {

class CodeGen {
public:
    explicit CodeGen(const CompilerConfig& config);
    ~CodeGen();

    bool generate(ASTNode* ast);

    void dumpIR(const std::string& path = "") const;

    bool emitObjectFile(const std::string& outputPath);
    bool emitExecutable(const std::string& outputPath);
    bool emitWasm(const std::string& outputPath);

    const std::vector<CBaltError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    CompilerConfig config_;
    std::vector<CBaltError> errors_;
    std::string generatedSource_;
    std::unordered_map<std::string, TypeInfo> functionTypes_;
    std::vector<std::unordered_map<std::string, TypeInfo>> variableScopes_;

    void emitError(const std::string& msg, SourceLocation loc = {});

    std::string emitProgram(ProgramNode* node);
    std::string emitTopLevel(ASTNode* node, i32 indent = 0);
    std::string emitBlock(BlockStmtNode* node, i32 indent = 0);
    std::string emitStatement(ASTNode* node, i32 indent = 0);
    std::string emitExpr(ASTNode* node);
    std::string emitPrintCall(CallExprNode* node, bool newline);
    std::string emitType(const TypeInfo& type) const;
    std::string escapeCString(const std::string& value) const;
    std::string indent(i32 level) const;
    TypeInfo inferExprType(const ASTNode* node) const;
    void pushVarScope();
    void popVarScope();
    void defineVarType(const std::string& name, const TypeInfo& type);
    std::optional<TypeInfo> lookupVarType(const std::string& name) const;
};

} // namespace cbalt
