#include "codegen.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace cbalt {

CodeGen::CodeGen(const CompilerConfig& config)
    : config_(config) {}

CodeGen::~CodeGen() = default;

bool CodeGen::generate(ASTNode* ast) {
    if (!ast) {
        emitError("AST kosong");
        return false;
    }

    if (ast->kind != NodeKind::Program) {
        emitError("Root AST harus berupa program", ast->location);
        return false;
    }

    generatedSource_ = emitProgram(static_cast<ProgramNode*>(ast));
    return !hasErrors();
}

void CodeGen::dumpIR(const std::string& path) const {
    if (path.empty()) {
        std::cout << generatedSource_;
        return;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[Error] Gagal tulis output: " << path << "\n";
        return;
    }
    out << generatedSource_;
}

bool CodeGen::emitObjectFile(const std::string& outputPath) {
    if (generatedSource_.empty()) {
        emitError("Belum ada source hasil codegen");
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        emitError("Gagal buka output object/source: " + outputPath);
        return false;
    }

    out << generatedSource_;
    return true;
}

bool CodeGen::emitExecutable(const std::string& outputPath) {
    if (generatedSource_.empty()) {
        emitError("Belum ada source hasil codegen");
        return false;
    }

    std::string cPath = outputPath + ".generated.c";
    if (!emitObjectFile(cPath)) return false;

    std::string command = "gcc -mconsole \"" + cPath + "\" -o \"" + outputPath + "\"";
    int ret = std::system(command.c_str());
    std::remove(cPath.c_str());

    if (ret != 0) {
        emitError("Gagal memanggil gcc untuk membuat executable");
        return false;
    }

    return true;
}

bool CodeGen::emitWasm(const std::string& /*outputPath*/) {
    emitError("Target WebAssembly belum didukung backend C saat ini");
    return false;
}

void CodeGen::emitError(const std::string& msg, SourceLocation loc) {
    errors_.push_back({CBaltError::Kind::CodeGen, msg, loc});
}

std::string CodeGen::emitProgram(ProgramNode* node) {
    functionTypes_.clear();
    variableScopes_.clear();

    for (const auto& child : node->body) {
        if (!child || child->kind != NodeKind::FuncDecl) continue;
        auto* fn = static_cast<FuncDeclNode*>(child.get());
        functionTypes_[fn->name] = fn->returnType.name.empty() ? TypeInfo::simple("void") : fn->returnType;
    }

    std::ostringstream out;
    out << "#include <stdbool.h>\n";
    out << "#include <stdint.h>\n";
    out << "#include <stdio.h>\n";
    out << "\n";
    out << "\n";

    for (const auto& child : node->body) {
        if (!child) continue;
        out << emitTopLevel(child.get()) << "\n";
    }

    return out.str();
}

std::string CodeGen::emitTopLevel(ASTNode* node, i32 level) {
    if (!node) return "";

    if (node->kind == NodeKind::FuncDecl) {
        auto* fn = static_cast<FuncDeclNode*>(node);
        std::ostringstream out;
        out << emitType(fn->returnType) << " " << fn->name << "(";
        for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i > 0) out << ", ";
            const auto& param = fn->params[i];
            TypeInfo paramType = param.type.name.empty() ? TypeInfo::simple("int") : param.type;
            out << emitType(paramType) << " " << param.name;
        }
        out << ")";

        if (!fn->body || fn->isExtern) {
            out << ";";
            return out.str();
        }

        pushVarScope();
        for (const auto& param : fn->params) {
            TypeInfo paramType = param.type.name.empty() ? TypeInfo::simple("int") : param.type;
            defineVarType(param.name, paramType);
        }
        out << " " << emitBlock(static_cast<BlockStmtNode*>(fn->body.get()), level);
        popVarScope();
        return out.str();
    }

    if (node->kind == NodeKind::StructDecl) {
        auto* st = static_cast<StructDeclNode*>(node);
        std::ostringstream out;
        out << "typedef struct " << st->name << " {\n";
        for (const auto& field : st->fields) {
            out << indent(level + 1) << emitType(field.second) << " " << field.first << ";\n";
        }
        out << "} " << st->name << ";";
        return out.str();
    }

    if (node->kind == NodeKind::ImportDecl) {
        return "/* import " + static_cast<ImportDeclNode*>(node)->path + " */";
    }

    return emitStatement(node, level);
}

std::string CodeGen::emitBlock(BlockStmtNode* node, i32 level) {
    std::ostringstream out;
    pushVarScope();
    out << "{\n";
    for (const auto& stmt : node->stmts) {
        if (!stmt) continue;
        out << emitStatement(stmt.get(), level + 1);
    }
    out << indent(level) << "}";
    popVarScope();
    return out.str();
}

std::string CodeGen::emitStatement(ASTNode* node, i32 level) {
    if (!node) return "";

    std::ostringstream out;
    std::string pad = indent(level);

    switch (node->kind) {
        case NodeKind::BlockStmt:
            out << pad << emitBlock(static_cast<BlockStmtNode*>(node), level) << "\n";
            break;
        case NodeKind::VarDecl: {
            auto* var = static_cast<VarDeclNode*>(node);
            TypeInfo ty = var->type.name.empty() ? inferExprType(var->initializer.get()) : var->type;
            if (ty.name.empty()) ty = TypeInfo::simple("int");
            defineVarType(var->name, ty);
            out << pad << emitType(ty) << " " << var->name;
            if (var->initializer) out << " = " << emitExpr(var->initializer.get());
            out << ";\n";
            break;
        }
        case NodeKind::ReturnStmt: {
            auto* ret = static_cast<ReturnStmtNode*>(node);
            out << pad << "return";
            if (ret->value) out << " " << emitExpr(ret->value.get());
            out << ";\n";
            break;
        }
        case NodeKind::ExprStmt:
            out << pad << emitExpr(static_cast<ExprStmtNode*>(node)->expr.get()) << ";\n";
            break;
        case NodeKind::IfStmt: {
            auto* ifNode = static_cast<IfStmtNode*>(node);
            out << pad << "if (" << emitExpr(ifNode->condition.get()) << ") ";
            out << emitStatement(ifNode->thenBranch.get(), level);
            if (ifNode->elseBranch) {
                std::string thenText = out.str();
                if (!thenText.empty() && thenText.back() == '\n') thenText.pop_back();
                out.str("");
                out.clear();
                out << thenText << pad << "else ";
                out << emitStatement(ifNode->elseBranch.get(), level);
            }
            break;
        }
        case NodeKind::WhileStmt: {
            auto* whileNode = static_cast<WhileStmtNode*>(node);
            out << pad << "while (" << emitExpr(whileNode->condition.get()) << ") ";
            out << emitStatement(whileNode->body.get(), level);
            break;
        }
        case NodeKind::ForInStmt: {
            auto* forNode = static_cast<ForInStmtNode*>(node);
            out << pad << "for (int " << forNode->varName << " = 0; "
                << forNode->varName << " < " << emitExpr(forNode->iterable.get()) << "; "
                << "++" << forNode->varName << ") ";
            out << emitStatement(forNode->body.get(), level);
            break;
        }
        case NodeKind::BreakStmt:
            out << pad << "break;\n";
            break;
        case NodeKind::ContinueStmt:
            out << pad << "continue;\n";
            break;
        case NodeKind::UnsafeBlock:
            out << pad << emitStatement(static_cast<UnsafeBlockNode*>(node)->body.get(), level);
            break;
        case NodeKind::SpawnStmt:
            out << pad << "/* spawn */ " << emitExpr(static_cast<SpawnStmtNode*>(node)->expr.get()) << ";\n";
            break;
        case NodeKind::AsmBlock:
            out << pad << "/* asm block omitted */\n";
            break;
        case NodeKind::FuncDecl:
        case NodeKind::StructDecl:
        case NodeKind::ImportDecl:
            out << pad << emitTopLevel(node, level) << "\n";
            break;
        default:
            emitError("Statement belum didukung backend C", node->location);
            out << pad << "/* unsupported statement */\n";
            break;
    }

    return out.str();
}

std::string CodeGen::emitExpr(ASTNode* node) {
    if (!node) return "0";

    switch (node->kind) {
        case NodeKind::IdentifierExpr:
            return static_cast<IdentifierNode*>(node)->name;
        case NodeKind::IntLiteral:
            return std::to_string(static_cast<IntLiteralNode*>(node)->value);
        case NodeKind::FloatLiteral: {
            std::ostringstream out;
            out << static_cast<FloatLiteralNode*>(node)->value;
            return out.str();
        }
        case NodeKind::StringLiteral:
            return "\"" + escapeCString(static_cast<StringLiteralNode*>(node)->value) + "\"";
        case NodeKind::BoolLiteral:
            return static_cast<BoolLiteralNode*>(node)->value ? "true" : "false";
        case NodeKind::NullLiteral:
            return "NULL";
        case NodeKind::UnaryExpr: {
            auto* expr = static_cast<UnaryExprNode*>(node);
            return "(" + expr->op + emitExpr(expr->operand.get()) + ")";
        }
        case NodeKind::BinaryExpr: {
            auto* expr = static_cast<BinaryExprNode*>(node);
            return "(" + emitExpr(expr->left.get()) + " " + expr->op + " " + emitExpr(expr->right.get()) + ")";
        }
        case NodeKind::CallExpr: {
            auto* call = static_cast<CallExprNode*>(node);
            if (auto* id = dynamic_cast<IdentifierNode*>(call->callee.get())) {
                if (id->name == "printc") return emitPrintCall(call, false);
                if (id->name == "print") return emitPrintCall(call, false);
                if (id->name == "println") return emitPrintCall(call, true);
            }
            std::ostringstream out;
            out << emitExpr(call->callee.get()) << "(";
            for (size_t i = 0; i < call->args.size(); ++i) {
                if (i > 0) out << ", ";
                out << emitExpr(call->args[i].get());
            }
            out << ")";
            return out.str();
        }
        case NodeKind::AssignExpr: {
            auto* expr = static_cast<AssignExprNode*>(node);
            return "(" + emitExpr(expr->target.get()) + " " + expr->op + " " + emitExpr(expr->value.get()) + ")";
        }
        case NodeKind::CastExpr: {
            auto* expr = static_cast<CastExprNode*>(node);
            return "((" + emitType(expr->targetType) + ")(" + emitExpr(expr->expr.get()) + "))";
        }
        case NodeKind::FieldExpr: {
            auto* expr = static_cast<FieldExprNode*>(node);
            return "(" + emitExpr(expr->object.get()) + "." + expr->field + ")";
        }
        case NodeKind::IndexExpr: {
            auto* expr = static_cast<IndexExprNode*>(node);
            return "(" + emitExpr(expr->object.get()) + "[" + emitExpr(expr->index.get()) + "])";
        }
        case NodeKind::ArrayLiteral:
            emitError("Array literal belum didukung backend C", node->location);
            return "0";
        case NodeKind::LambdaExpr:
            emitError("Lambda belum didukung backend C", node->location);
            return "0";
        default:
            emitError("Ekspresi belum didukung backend C", node->location);
            return "0";
    }
}

std::string CodeGen::emitPrintCall(CallExprNode* node, bool newline) {
    if (node->args.empty()) {
        return newline ? "printf(\"\\n\")" : "printf(\"\")";
    }

    std::ostringstream out;
    out << "(";

    bool first = true;
    for (const auto& argNode : node->args) {
        ASTNode* arg = argNode.get();
        TypeInfo type = inferExprType(arg);
        std::string expr = emitExpr(arg);
        std::string piece;

        if (type.name == "string") piece = "printf(\"%s\", " + expr + ")";
        else if (type.name == "bool") piece = "printf(\"%s\", (" + expr + " ? \"true\" : \"false\"))";
        else if (type.name == "f32" || type.name == "float") piece = "printf(\"%f\", " + expr + ")";
        else if (type.name == "f64") piece = "printf(\"%g\", " + expr + ")";
        else if (type.name == "i64") piece = "printf(\"%lld\", (long long)" + expr + ")";
        else if (type.name == "u64") piece = "printf(\"%llu\", (unsigned long long)" + expr + ")";
        else if (type.name == "u32" || type.name == "u16" || type.name == "u8") piece = "printf(\"%u\", " + expr + ")";
        else if (type.name == "any") piece = "printf(\"%p\", " + expr + ")";
        else piece = "printf(\"%d\", " + expr + ")";

        if (!first) out << ", ";
        out << piece;
        first = false;
    }

    if (newline) {
        if (!first) out << ", ";
        out << "printf(\"\\n\")";
    }

    out << ")";
    return out.str();
}

std::string CodeGen::emitType(const TypeInfo& type) const {
    std::string name = type.name.empty() ? "int" : type.name;

    if (type.isArray && !type.genericArgs.empty()) {
        return emitType(type.genericArgs.front()) + "*";
    }
    if (type.isPointer && !type.genericArgs.empty()) {
        return emitType(type.genericArgs.front()) + "*";
    }

    if (name == "void") return "void";
    if (name == "bool") return "bool";
    if (name == "string") return "const char*";
    if (name == "i8") return "int8_t";
    if (name == "i16") return "int16_t";
    if (name == "i32" || name == "int") return "int";
    if (name == "i64") return "int64_t";
    if (name == "u8") return "uint8_t";
    if (name == "u16") return "uint16_t";
    if (name == "u32") return "uint32_t";
    if (name == "u64") return "uint64_t";
    if (name == "f32" || name == "float") return "float";
    if (name == "f64") return "double";
    if (name == "any") return "void*";

    return name;
}

std::string CodeGen::escapeCString(const std::string& value) const {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

std::string CodeGen::indent(i32 level) const {
    return std::string(static_cast<size_t>(level) * 4, ' ');
}

TypeInfo CodeGen::inferExprType(const ASTNode* node) const {
    if (!node) return TypeInfo::simple("int");

    switch (node->kind) {
        case NodeKind::IdentifierExpr: {
            const auto* id = static_cast<const IdentifierNode*>(node);
            if (auto type = lookupVarType(id->name)) return *type;
            auto fn = functionTypes_.find(id->name);
            if (fn != functionTypes_.end()) return fn->second;
            return TypeInfo::simple("int");
        }
        case NodeKind::IntLiteral:
            return TypeInfo::simple("i32");
        case NodeKind::FloatLiteral:
            return TypeInfo::simple("f64");
        case NodeKind::StringLiteral:
            return TypeInfo::simple("string");
        case NodeKind::BoolLiteral:
            return TypeInfo::simple("bool");
        case NodeKind::UnaryExpr:
            return inferExprType(static_cast<const UnaryExprNode*>(node)->operand.get());
        case NodeKind::BinaryExpr: {
            const auto* expr = static_cast<const BinaryExprNode*>(node);
            TypeInfo left = inferExprType(expr->left.get());
            TypeInfo right = inferExprType(expr->right.get());
            if (expr->op == "==" || expr->op == "!=" || expr->op == "<" || expr->op == ">" ||
                expr->op == "<=" || expr->op == ">=" || expr->op == "&&" || expr->op == "||") {
                return TypeInfo::simple("bool");
            }
            if (left.name == "f64" || right.name == "f64") return TypeInfo::simple("f64");
            if (left.name == "f32" || right.name == "f32" || left.name == "float" || right.name == "float") {
                return TypeInfo::simple("f32");
            }
            return left.name.empty() ? TypeInfo::simple("i32") : left;
        }
        case NodeKind::CallExpr: {
            const auto* call = static_cast<const CallExprNode*>(node);
            const auto* id = dynamic_cast<const IdentifierNode*>(call->callee.get());
            if (id) {
                if (id->name == "print" || id->name == "println" || id->name == "printc") {
                    return TypeInfo::simple("void");
                }
                auto fn = functionTypes_.find(id->name);
                if (fn != functionTypes_.end()) return fn->second;
            }
            return TypeInfo::simple("i32");
        }
        case NodeKind::AssignExpr:
            return inferExprType(static_cast<const AssignExprNode*>(node)->value.get());
        case NodeKind::CastExpr:
            return static_cast<const CastExprNode*>(node)->targetType;
        case NodeKind::NullLiteral: {
            TypeInfo t = TypeInfo::simple("any");
            t.isPointer = true;
            t.genericArgs.push_back(TypeInfo::simple("void"));
            return t;
        }
        default:
            return TypeInfo::simple("int");
    }
}

void CodeGen::pushVarScope() {
    variableScopes_.push_back({});
}

void CodeGen::popVarScope() {
    if (!variableScopes_.empty()) variableScopes_.pop_back();
}

void CodeGen::defineVarType(const std::string& name, const TypeInfo& type) {
    if (variableScopes_.empty()) pushVarScope();
    variableScopes_.back()[name] = type;
}

std::optional<TypeInfo> CodeGen::lookupVarType(const std::string& name) const {
    for (auto it = variableScopes_.rbegin(); it != variableScopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return std::nullopt;
}

} // namespace cbalt
