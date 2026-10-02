#pragma once

#include "bytecode.h"
#include "../parser/ast.h"
#include "../runtime/object_function.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Compiler {
public:
    Compiler() = default;

    std::shared_ptr<FunctionProto> compile(
        const std::vector<std::unique_ptr<Stmt>>& statements,
        const std::string& name = "main"
    );

private:
    struct Local {
        std::string name;
        int depth;
        bool captured;
    };

    struct UpvalueRef {
        bool isLocal;
        int index;
    };

    struct FunctionState {
        std::shared_ptr<FunctionProto> proto;
        int localCount = 0;
        int maxLocalCount = 0;
        int scopeDepth = 0;
        std::vector<Local> locals;
        std::vector<UpvalueRef> upvalues;
        std::string enclosingClass;
    };

    struct Slot {
        bool isLocal;
        int index;
    };

    enum class VariableKind {
        Local,
        Upvalue,
        Global
    };

    struct VariableRef {
        VariableKind kind;
        int index;
    };

    void compileStmt(const Stmt* statement);
    void compileExpr(const Expr* expression);

    void compileLet(const LetStmt* statement);
    void compileDestructure(const DestructureStmt* statement);
    void declarePattern(
        const Pattern* pattern,
        std::map<const Pattern*, Slot>& slots,
        int line
    );
    void convertForPattern(const Pattern* pattern, int line);
    void bindPattern(
        const Pattern* pattern,
        int source,
        std::map<const Pattern*, Slot>& slots,
        int line
    );
    void popLocal(int line);
    void compileIf(const IfStmt* statement);
    void compileWhile(const WhileStmt* statement);
    void compileFor(const ForStmt* statement);
    void compileForIn(const ForInStmt* statement);
    void compileSwitch(const SwitchStmt* statement);
    void compileClass(const ClassStmt* statement);
    void compileMethod(
        const ClassMethod& method,
        const std::string& className,
        int line
    );
    void compileTry(const TryStmt* statement);

    void compileCleanupBlock(
        const std::vector<std::unique_ptr<Stmt>>& statements,
        int line
    );
    void compileThrow(const ThrowStmt* statement);
    void compileImport(const ImportStmt* statement);
    void compileExport(const ExportStmt* statement);
    void flushExports();
    void compileFunction(const FunctionStmt* statement);
    void compileFunctionValue(
        const std::string& name,
        const std::vector<std::string>& parameters,
        const BlockStmt* body,
        int line,
        bool keepValue
    );

    void compileAssign(const AssignExpr* expression);
    void compileBinary(const BinaryExpr* expression);
    void compileLogical(const LogicalExpr* expression);
    void compileIndex(const IndexExpr* expression);
    void compileCall(const CallExpr* expression);
    void compileNew(const NewExpr* expression);
    void compileSuper(const SuperExpr* expression);
    void compileThis(const ThisExpr* expression);
    bool insideMethod();
    void compileProperty(const PropertyExpr* expression);

    void beginScope();
    void endScope(int line);

    Slot declareVariable(const std::string& name, int line);
    void defineVariable(const Slot& slot, int line, bool keepValue = false);

    int declareLocal(const std::string& name, int line);

    int findLocal(
        const FunctionState& state,
        const std::string& name
    ) const;

    int resolveUpvalueAt(
        size_t level,
        const std::string& name,
        int line
    );

    int addUpvalue(
        FunctionState& target,
        bool isLocal,
        int index
    );

    VariableRef resolveVariable(
        const std::string& name,
        int line
    );

    void emitGet(const VariableRef& ref, int line);
    void emitSet(const VariableRef& ref, int line);

    void beginFunction(
        const std::string& name,
        int arity,
        int line
    );

    void endFunction(const Slot& slot, int line, bool keepValue);
    void endMethod(int line);

    int nameConstant(const std::string& name);
    int addConstant(const Value& value);

    size_t emit(OpCode op, int line);
    size_t emit(OpCode op, int operand, int line);

    void patchJump(size_t position, int target);
    int next();

    FunctionState& state();
    Chunk& chunk();

    std::vector<FunctionState> functions;
    std::vector<int> pendingExportLines;
    std::vector<std::string> pendingExports;
    std::vector<size_t> loopStarts;
    std::vector<size_t> loopBreaks;
    std::vector<size_t> loopContinues;
    std::vector<size_t> switchBreaks;
    int switchDepth = 0;
};
