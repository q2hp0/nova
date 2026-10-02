#include "compiler.h"

#include "../errors/errors.h"

static const size_t MAX_LOCALS = 256;

static const char* SEQUENCE_SLOT = "@sequence";
static const char* INDEX_SLOT = "@index";

std::shared_ptr<FunctionProto> Compiler::compile(
    const std::vector<std::unique_ptr<Stmt>>& statements,
    const std::string& name
) {
    FunctionState main;
    main.proto = std::make_shared<FunctionProto>(name, 0, 0);
    main.localCount = 0;
    main.scopeDepth = 0;

    functions.push_back(main);

    for (const auto& statement : statements) {
        compileStmt(statement.get());
    }

    flushExports();

    emit(OpCode::Nil, 0);
    emit(OpCode::Halt, 0);

    FunctionState& top = functions.back();

    top.proto->setSlotCount(top.maxLocalCount);

    for (const auto& upvalue : top.upvalues) {
        top.proto->addUpvalue(
            upvalue.isLocal,
            upvalue.index
        );
    }

    std::shared_ptr<FunctionProto> proto = top.proto;

    functions.clear();

    return proto;
}

void Compiler::compileStmt(const Stmt* statement) {
    if (auto let = dynamic_cast<const LetStmt*>(statement)) {
        compileLet(let);
        return;
    }

    if (auto destructure =
        dynamic_cast<const DestructureStmt*>(statement)) {
        compileDestructure(destructure);
        return;
    }

    if (auto expression = dynamic_cast<const ExpressionStmt*>(statement)) {
        compileExpr(expression->expression.get());
        emit(OpCode::Pop, expression->line);
        return;
    }

    if (auto block = dynamic_cast<const BlockStmt*>(statement)) {
        beginScope();

        for (const auto& inner : block->statements) {
            compileStmt(inner.get());
        }

        endScope(statement->line);
        return;
    }

    if (auto branch = dynamic_cast<const IfStmt*>(statement)) {
        compileIf(branch);
        return;
    }

    if (auto loop = dynamic_cast<const WhileStmt*>(statement)) {
        compileWhile(loop);
        return;
    }

    if (auto loop = dynamic_cast<const ForStmt*>(statement)) {
        compileFor(loop);
        return;
    }

    if (auto loop = dynamic_cast<const ForInStmt*>(statement)) {
        compileForIn(loop);
        return;
    }

    if (auto classNode = dynamic_cast<const ClassStmt*>(statement)) {
        compileClass(classNode);
        return;
    }

    if (auto tryNode = dynamic_cast<const TryStmt*>(statement)) {
        compileTry(tryNode);
        return;
    }

    if (auto throwNode = dynamic_cast<const ThrowStmt*>(statement)) {
        compileThrow(throwNode);
        return;
    }

    if (auto importNode = dynamic_cast<const ImportStmt*>(statement)) {
        compileImport(importNode);
        return;
    }

    if (auto exportNode = dynamic_cast<const ExportStmt*>(statement)) {
        compileExport(exportNode);
        return;
    }

    if (auto choice = dynamic_cast<const SwitchStmt*>(statement)) {
        compileSwitch(choice);
        return;
    }

    if (auto function = dynamic_cast<const FunctionStmt*>(statement)) {
        compileFunction(function);
        return;
    }

    if (auto returnStatement =
            dynamic_cast<const ReturnStmt*>(statement)) {
        if (returnStatement->value) {
            compileExpr(returnStatement->value.get());
        } else {
            emit(OpCode::Nil, returnStatement->line);
        }

        emit(OpCode::Return, returnStatement->line);
        return;
    }

    if (dynamic_cast<const BreakStmt*>(statement)) {
        if (switchDepth > 0) {
            switchBreaks.push_back(
                emit(OpCode::Jump, 0, statement->line)
            );

            return;
        }

        if (loopStarts.empty()) {
            throw CompileError(
                "'break' is only allowed inside a loop or a switch",
                { statement->line, 0 }
            );
        }

        loopBreaks.push_back(
            emit(OpCode::Jump, 0, statement->line)
        );

        return;
    }

    if (dynamic_cast<const ContinueStmt*>(statement)) {
        if (loopStarts.empty()) {
            throw CompileError(
                "'continue' is only allowed inside a loop",
                { statement->line, 0 }
            );
        }

        loopContinues.push_back(
            emit(OpCode::Jump, 0, statement->line)
        );

        return;
    }

    throw CompileError(
        "unknown statement",
        { statement->line, 0 }
    );
}

void Compiler::popLocal(int line) {
    FunctionState& current = state();

    if (!current.locals.empty()) {
        if (current.locals.back().captured) {
            emit(
                OpCode::CloseUpvalue,
                current.localCount - 1,
                line
            );
        }

        current.locals.pop_back();
        current.localCount--;
    }
}

void Compiler::compileLet(const LetStmt* statement) {
    Slot slot = declareVariable(
        statement->name,
        statement->line
    );

    if (statement->initializer) {
        compileExpr(statement->initializer.get());
    } else {
        emit(OpCode::Nil, statement->line);
    }

    defineVariable(slot, statement->line);
}

void Compiler::compileDestructure(const DestructureStmt* statement) {
    std::map<const Pattern*, Slot> slots;

    declarePattern(statement->pattern.get(), slots, statement->line);

    const int root = declareLocal("$pattern", statement->line);

    compileExpr(statement->initializer.get());
    convertForPattern(statement->pattern.get(), statement->line);

    emit(OpCode::SetLocal, root, statement->line);
    emit(OpCode::Pop, statement->line);

    bindPattern(statement->pattern.get(), root, slots, statement->line);

    popLocal(statement->line);
}

void Compiler::declarePattern(
    const Pattern* pattern,
    std::map<const Pattern*, Slot>& slots,
    int line
) {
    if (pattern->kind == PatternKind::Name) {
        slots.emplace(pattern, declareVariable(pattern->name, line));
        return;
    }

    for (const auto& part : pattern->parts) {
        declarePattern(part.get(), slots, line);
    }
}

void Compiler::convertForPattern(const Pattern* pattern, int line) {
    if (pattern->kind != PatternKind::List) {
        return;
    }

    emit(OpCode::ToList, line);

    for (const auto& part : pattern->parts) {
        convertForPattern(part.get(), line);
    }
}

void Compiler::bindPattern(
    const Pattern* pattern,
    int source,
    std::map<const Pattern*, Slot>& slots,
    int line
) {
    if (pattern->kind == PatternKind::Name) {
        emit(OpCode::GetLocal, source, line);
        defineVariable(slots.at(pattern), line);
        return;
    }

    const bool list = pattern->kind == PatternKind::List;

    for (const auto& entry : pattern->parts) {
        const Pattern* part = entry.get();

        emit(OpCode::GetLocal, source, line);

        if (list && pattern->rest >= 0 &&
            part->position == pattern->rest) {
            emit(OpCode::GetProperty, nameConstant("slice"), line);
            emit(
                OpCode::Constant,
                addConstant(Value(static_cast<double>(part->position))),
                line
            );
            emit(OpCode::Call, 1, line);

            defineVariable(slots.at(part), line);
            continue;
        }

        if (list) {
            emit(OpCode::GetProperty, nameConstant("at"), line);
            emit(
                OpCode::Constant,
                addConstant(Value(static_cast<double>(part->position))),
                line
            );
            emit(OpCode::Call, 1, line);
        } else {
            emit(
                OpCode::Constant,
                addConstant(Value(part->key)),
                line
            );

            emit(OpCode::Index, line);
        }

        if (part->kind == PatternKind::Name) {
            defineVariable(slots.at(part), line);
            continue;
        }

        const int child = declareLocal("$part", line);

        emit(OpCode::SetLocal, child, line);
        emit(OpCode::Pop, line);

        bindPattern(part, child, slots, line);

        popLocal(line);
    }
}

void Compiler::compileClass(const ClassStmt* statement) {
    emit(
        OpCode::DefineClass,
        nameConstant(statement->name),
        statement->line
    );

    if (!statement->baseName.empty()) {
        emitGet(resolveVariable(statement->baseName, statement->line),
                statement->line);

        emit(
            OpCode::InheritClass,
            nameConstant(statement->name),
            statement->line
        );
    }

    for (size_t i = 0; i < statement->fields.size(); ++i) {
        emit(
            OpCode::Constant,
            nameConstant(statement->fields[i]),
            statement->line
        );

        if (statement->fieldDefaults[i]) {
            compileExpr(statement->fieldDefaults[i].get());
        } else {
            emit(OpCode::Nil, statement->line);
        }
    }

    emit(
        OpCode::Method,
        static_cast<int>(statement->fields.size()),
        statement->line
    );

    for (const ClassMethod& method : statement->methods) {
        compileMethod(method, statement->name, statement->line);
    }

    Slot slot = declareVariable(statement->name, statement->line);
    defineVariable(slot, statement->line);
}

void Compiler::compileMethod(
    const ClassMethod& method,
    const std::string& className,
    int line
) {
    beginFunction(
        method.name,
        static_cast<int>(method.parameters.size()) + 1,
        line
    );

    state().proto->setMethod(true);
    state().enclosingClass = className;

    beginScope();
    declareLocal("this", line);

    for (const auto& parameter : method.parameters) {
        declareLocal(parameter, line);
    }

    for (const auto& inner : method.body->statements) {
        compileStmt(inner.get());
    }

    if (method.name == "constructor") {
        emit(OpCode::GetLocal, 0, line);
    } else {
        emit(OpCode::Nil, line);
    }

    emit(OpCode::Return, line);

    endMethod(line);

    emit(
        method.isStatic ? OpCode::DefineStatic : OpCode::BindMethod,
        nameConstant(method.name),
        line
    );
}

bool Compiler::insideMethod() {
    return state().proto != nullptr && state().proto->isMethod();
}

void Compiler::compileThis(const ThisExpr* expression) {
    if (!insideMethod()) {
        throw CompileError(
            "'this' only works inside a method",
            { expression->line, 0 }
        );
    }

    emit(OpCode::GetLocal, 0, expression->line);
}

void Compiler::compileNew(const NewExpr* expression) {
    compileExpr(expression->callee.get());

    for (const auto& argument : expression->arguments) {
        compileExpr(argument.get());
    }

    emit(
        OpCode::Construct,
        static_cast<int>(expression->arguments.size()),
        expression->line
    );
}

void Compiler::compileSuper(const SuperExpr* expression) {
    if (!insideMethod()) {
        throw CompileError(
            "'super' only works inside a method",
            { expression->line, 0 }
        );
    }

    const std::string className = state().enclosingClass;

    if (className.empty()) {
        throw CompileError(
            "'super' needs a class",
            { expression->line, 0 }
        );
    }

    auto nameExpr =
        dynamic_cast<const IdentifierExpr*>(expression->callee.get());

    if (nameExpr == nullptr) {
        throw CompileError(
            "'super' can only reach a method by name",
            { expression->line, 0 }
        );
    }

    const IdentifierExpr* name = nameExpr;

    emitGet(resolveVariable(className, expression->line), expression->line);
    emit(OpCode::GetLocal, 0, expression->line);

    for (const auto& argument : expression->arguments) {
        compileExpr(argument.get());
    }

    if (name->name == "constructor") {
        emit(
            OpCode::SuperCall,
            static_cast<int>(expression->arguments.size()),
            expression->line
        );
        return;
    }

    emit(
        OpCode::SuperInvoke,
        nameConstant(name->name) * 256 +
        static_cast<int>(expression->arguments.size()),
        expression->line
    );
}

void Compiler::compileThrow(const ThrowStmt* statement) {
    if (statement->value) {
        compileExpr(statement->value.get());
    } else {
        emit(OpCode::Nil, statement->line);
    }

    emit(OpCode::Throw, statement->line);
}

void Compiler::compileCleanupBlock(
    const std::vector<std::unique_ptr<Stmt>>& statements,
    int line
) {
    beginScope();

    for (const auto& inner : statements) {
        compileStmt(inner.get());
    }

    endScope(line);
}

void Compiler::compileTry(const TryStmt* statement) {
    const size_t pushHandler = emit(
        OpCode::PushHandler,
        0,
        statement->line
    );

    const size_t setCleanup = statement->cleanup != nullptr
        ? emit(OpCode::SetCleanup, 0, statement->line)
        : 0;

    compileCleanupBlock(statement->body->statements, statement->line);

    emit(OpCode::PopHandler, statement->line);

    const size_t jumpOverCatch = emit(OpCode::Jump, 0, statement->line);

    const int catchTarget = next();

    if (statement->handler != nullptr) {
        patchJump(
            pushHandler,
            catchTarget | 0x40000000
        );

        beginScope();

        if (!statement->caught.empty()) {
            declareLocal(statement->caught, statement->line);
            emit(
                OpCode::SetLocal,
                state().localCount - 1,
                statement->line
            );
            emit(OpCode::Pop, statement->line);
        }

        for (const auto& inner : statement->handler->statements) {
            compileStmt(inner.get());
        }

        endScope(statement->line);
    }

    const int finallyTarget = next();

    if (statement->handler == nullptr) {
        patchJump(pushHandler, finallyTarget);
    }

    if (statement->cleanup != nullptr) {
        patchJump(setCleanup, finallyTarget);
        patchJump(jumpOverCatch, finallyTarget);

        compileCleanupBlock(statement->cleanup->statements, statement->line);
        emit(OpCode::EndFinally, statement->line);
    } else {
        patchJump(jumpOverCatch, next());
    }
}

void Compiler::compileImport(const ImportStmt* statement) {
    emit(
        OpCode::Constant,
        addConstant(Value(statement->path)),
        statement->line
    );

    emit(OpCode::LoadModule, statement->line);

    if (statement->alias.empty()) {
        emit(OpCode::Pop, statement->line);
        return;
    }

    Slot slot = declareVariable(statement->alias, statement->line);
    defineVariable(slot, statement->line);
}

void Compiler::compileExport(const ExportStmt* statement) {
    for (const std::string& name : statement->names) {
        pendingExports.push_back(name);
        pendingExportLines.push_back(statement->line);
    }
}

void Compiler::flushExports() {
    if (pendingExports.empty()) {
        return;
    }

    for (size_t i = 0; i < pendingExports.size(); ++i) {
        emitGet(
            resolveVariable(pendingExports[i], pendingExportLines[i]),
            pendingExportLines[i]
        );
        emit(
            OpCode::Constant,
            addConstant(Value(pendingExports[i])),
            pendingExportLines[i]
        );
    }

    emit(
        OpCode::Export,
        static_cast<int>(pendingExports.size()),
        pendingExportLines.back()
    );

    pendingExports.clear();
    pendingExportLines.clear();
}

void Compiler::compileIf(const IfStmt* statement) {
    compileExpr(statement->condition.get());

    size_t jumpIfFalse = emit(
        OpCode::JumpIfFalse,
        0,
        statement->line
    );

    emit(OpCode::Pop, statement->line);

    compileStmt(statement->thenBranch.get());

    size_t jumpOverElse = emit(
        OpCode::Jump,
        0,
        statement->line
    );

    patchJump(
        jumpIfFalse,
        static_cast<int>(chunk().count())
    );

    emit(OpCode::Pop, statement->line);

    if (statement->elseBranch) {
        compileStmt(statement->elseBranch.get());
    }

    patchJump(
        jumpOverElse,
        static_cast<int>(chunk().count())
    );
}

void Compiler::compileWhile(const WhileStmt* statement) {
    size_t loopStart = chunk().count();

    size_t breaksMark = loopBreaks.size();
    size_t continuesMark = loopContinues.size();

    compileExpr(statement->condition.get());

    size_t exitJump = emit(
        OpCode::JumpIfFalse,
        0,
        statement->line
    );

    emit(OpCode::Pop, statement->line);

    loopStarts.push_back(loopStart);
    compileStmt(statement->body.get());
    loopStarts.pop_back();

    emit(OpCode::Loop, static_cast<int>(loopStart), statement->line);

    for (size_t i = continuesMark; i < loopContinues.size(); ++i) {
        patchJump(loopContinues[i], static_cast<int>(loopStart));
    }

    loopContinues.resize(continuesMark);

    patchJump(
        exitJump,
        static_cast<int>(chunk().count())
    );

    emit(OpCode::Pop, statement->line);

    int endTarget = static_cast<int>(chunk().count());

    for (size_t i = breaksMark; i < loopBreaks.size(); ++i) {
        patchJump(loopBreaks[i], endTarget);
    }

    loopBreaks.resize(breaksMark);
}

void Compiler::compileFor(const ForStmt* statement) {
    beginScope();

    if (statement->initializer) {
        compileStmt(statement->initializer.get());
    }

    size_t loopStart = chunk().count();

    size_t breaksMark = loopBreaks.size();
    size_t continuesMark = loopContinues.size();

    size_t exitJump = 0;
    bool hasCondition = statement->condition != nullptr;

    if (hasCondition) {
        compileExpr(statement->condition.get());

        exitJump = emit(
            OpCode::JumpIfFalse,
            0,
            statement->line
        );

        emit(OpCode::Pop, statement->line);
    }

    loopStarts.push_back(loopStart);
    compileStmt(statement->body.get());
    loopStarts.pop_back();

    if (statement->initializer) {
        auto* declared = dynamic_cast<const LetStmt*>(
            statement->initializer.get()
        );

        int slot = declared != nullptr ?
            findLocal(state(), declared->name) : -1;

        if (slot >= 0 && state().locals[slot].captured) {
            emit(
                OpCode::CloseUpvalue,
                slot,
                statement->line
            );
        }
    }

    int continueTarget = static_cast<int>(chunk().count());

    for (size_t i = continuesMark; i < loopContinues.size(); ++i) {
        patchJump(loopContinues[i], continueTarget);
    }

    loopContinues.resize(continuesMark);

    if (statement->increment) {
        compileExpr(statement->increment.get());
        emit(OpCode::Pop, statement->line);
    }

    emit(OpCode::Loop, static_cast<int>(loopStart), statement->line);

    if (hasCondition) {
        patchJump(
            exitJump,
            static_cast<int>(chunk().count())
        );

        emit(OpCode::Pop, statement->line);
    }

    int endTarget = static_cast<int>(chunk().count());

    for (size_t i = breaksMark; i < loopBreaks.size(); ++i) {
        patchJump(loopBreaks[i], endTarget);
    }

    loopBreaks.resize(breaksMark);

    endScope(statement->line);
}

void Compiler::compileForIn(const ForInStmt* statement) {
    beginScope();

    int sequence = declareLocal(SEQUENCE_SLOT, statement->line);
    int index = declareLocal(INDEX_SLOT, statement->line);
    int item = declareLocal(statement->name, statement->line);

    int zero = addConstant(Value(0.0));
    int one = addConstant(Value(1.0));

    compileExpr(statement->sequence.get());
    emit(OpCode::ToList, statement->line);
    emit(OpCode::SetLocal, sequence, statement->line);
    emit(OpCode::Pop, statement->line);

    emit(OpCode::Constant, zero, statement->line);
    emit(OpCode::SetLocal, index, statement->line);
    emit(OpCode::Pop, statement->line);

    int length = nameConstant("len");

    size_t loopStart = chunk().count();

    size_t breaksMark = loopBreaks.size();
    size_t continuesMark = loopContinues.size();

    emit(OpCode::GetLocal, index, statement->line);
    emit(OpCode::GetGlobal, length, statement->line);
    emit(OpCode::GetLocal, sequence, statement->line);
    emit(OpCode::Call, 1, statement->line);
    emit(OpCode::Less, statement->line);

    size_t exitJump = emit(
        OpCode::JumpIfFalse,
        0,
        statement->line
    );

    emit(OpCode::Pop, statement->line);

    emit(OpCode::GetLocal, sequence, statement->line);
    emit(OpCode::GetLocal, index, statement->line);
    emit(OpCode::Index, statement->line);
    emit(OpCode::SetLocal, item, statement->line);
    emit(OpCode::Pop, statement->line);

    loopStarts.push_back(loopStart);
    compileStmt(statement->body.get());
    loopStarts.pop_back();

    if (item >= 0 && state().locals[item].captured) {
        emit(OpCode::CloseUpvalue, item, statement->line);
    }

    int continueTarget = static_cast<int>(chunk().count());

    for (size_t i = continuesMark; i < loopContinues.size(); ++i) {
        patchJump(loopContinues[i], continueTarget);
    }

    loopContinues.resize(continuesMark);

    emit(OpCode::GetLocal, index, statement->line);
    emit(OpCode::Constant, one, statement->line);
    emit(OpCode::Add, statement->line);
    emit(OpCode::SetLocal, index, statement->line);
    emit(OpCode::Pop, statement->line);

    emit(OpCode::Loop, static_cast<int>(loopStart), statement->line);

    patchJump(
        exitJump,
        static_cast<int>(chunk().count())
    );

    emit(OpCode::Pop, statement->line);

    int endTarget = static_cast<int>(chunk().count());

    for (size_t i = breaksMark; i < loopBreaks.size(); ++i) {
        patchJump(loopBreaks[i], endTarget);
    }

    loopBreaks.resize(breaksMark);

    endScope(statement->line);
}

void Compiler::compileSwitch(const SwitchStmt* statement) {
    beginScope();

    int subject = declareLocal("$switch", statement->line);

    compileExpr(statement->subject.get());
    emit(OpCode::SetLocal, subject, statement->line);
    emit(OpCode::Pop, statement->line);

    std::vector<size_t> entryJumps(statement->cases.size(), 0);
    size_t defaultCase = statement->cases.size();

    for (size_t i = 0; i < statement->cases.size(); ++i) {
        const SwitchCase& branch = statement->cases[i];

        if (!branch.value) {
            defaultCase = i;

            continue;
        }

        emit(OpCode::GetLocal, subject, statement->line);
        compileExpr(branch.value.get());
        emit(OpCode::Equal, statement->line);

        size_t noMatch = emit(
            OpCode::JumpIfFalse,
            0,
            statement->line
        );

        emit(OpCode::Pop, statement->line);

        entryJumps[i] = emit(OpCode::Jump, 0, statement->line);

        patchJump(noMatch, static_cast<int>(chunk().count()));
    }

    size_t skipBodies = emit(OpCode::Jump, 0, statement->line);

    size_t breaksMark = switchBreaks.size();

    switchDepth++;

    for (size_t i = 0; i < statement->cases.size(); ++i) {
        const SwitchCase& branch = statement->cases[i];

        if (i == defaultCase) {
            patchJump(skipBodies, static_cast<int>(chunk().count()));
        } else {
            patchJump(
                entryJumps[i],
                static_cast<int>(chunk().count())
            );
        }

        compileStmt(branch.body.get());
    }

    if (defaultCase == statement->cases.size()) {
        patchJump(skipBodies, static_cast<int>(chunk().count()));
    }

    switchDepth--;

    int endTarget = static_cast<int>(chunk().count());

    for (size_t i = breaksMark; i < switchBreaks.size(); ++i) {
        patchJump(switchBreaks[i], endTarget);
    }

    switchBreaks.resize(breaksMark);

    endScope(statement->line);
}

void Compiler::compileFunction(const FunctionStmt* statement) {
    compileFunctionValue(
        statement->name,
        statement->parameters,
        statement->body.get(),
        statement->line,
        false
    );
}

void Compiler::compileFunctionValue(
    const std::string& name,
    const std::vector<std::string>& parameters,
    const BlockStmt* body,
    int line,
    bool keepValue
) {
    Slot slot = declareVariable(
        name,
        line
    );

    beginFunction(
        name,
        static_cast<int>(parameters.size()),
        line
    );

    beginScope();

    for (const auto& parameter : parameters) {
        declareLocal(parameter, line);
    }

    for (const auto& inner : body->statements) {
        compileStmt(inner.get());
    }

    endFunction(slot, line, keepValue);
}

void Compiler::compileAssign(const AssignExpr* expression) {
    if (auto target = dynamic_cast<const IdentifierExpr*>(
            expression->target.get())) {
        compileExpr(expression->value.get());

        emitSet(
            resolveVariable(target->name, target->line),
            expression->line
        );

        return;
    }

    if (auto target = dynamic_cast<const IndexExpr*>(
            expression->target.get())) {
        compileExpr(target->target.get());
        compileExpr(target->index.get());
        compileExpr(expression->value.get());

        emit(OpCode::SetIndex, expression->line);
        return;
    }

    if (auto target = dynamic_cast<const PropertyExpr*>(
            expression->target.get())) {
        compileExpr(target->target.get());
        compileExpr(expression->value.get());

        emit(
            OpCode::SetProperty,
            addConstant(Value(target->name)),
            expression->line
        );

        return;
    }

    throw CompileError(
        "invalid assignment target",
        { expression->line, 0 }
    );
}

void Compiler::compileBinary(const BinaryExpr* expression) {
    if (expression->op == "==") {
        compileExpr(expression->left.get());
        compileExpr(expression->right.get());
        emit(OpCode::Equal, expression->line);
        return;
    }

    if (expression->op == "!=") {
        compileExpr(expression->left.get());
        compileExpr(expression->right.get());
        emit(OpCode::NotEqual, expression->line);
        return;
    }

    compileExpr(expression->left.get());
    compileExpr(expression->right.get());

    OpCode op = OpCode::Add;

    if (expression->op == "+") {
        op = OpCode::Add;
    } else if (expression->op == "-") {
        op = OpCode::Subtract;
    } else if (expression->op == "*") {
        op = OpCode::Multiply;
    } else if (expression->op == "/") {
        op = OpCode::Divide;
    } else if (expression->op == "%") {
        op = OpCode::Modulo;
    } else if (expression->op == ">") {
        op = OpCode::Greater;
    } else if (expression->op == ">=") {
        op = OpCode::GreaterEqual;
    } else if (expression->op == "<") {
        op = OpCode::Less;
    } else if (expression->op == "<=") {
        op = OpCode::LessEqual;
    } else {
        throw CompileError(
            "unknown operator '" + expression->op + "'",
            { expression->line, 0 }
        );
    }

    emit(op, expression->line);
}

void Compiler::compileLogical(const LogicalExpr* expression) {
    compileExpr(expression->left.get());

    bool isAnd = expression->op == "and" || expression->op == "&&";

    size_t jump = emit(
        isAnd ? OpCode::JumpIfFalse : OpCode::JumpIfTrue,
        0,
        expression->line
    );

    emit(OpCode::Pop, expression->line);

    compileExpr(expression->right.get());

    patchJump(jump, static_cast<int>(chunk().count()));
}

void Compiler::compileIndex(const IndexExpr* expression) {
    compileExpr(expression->target.get());
    compileExpr(expression->index.get());
    emit(OpCode::Index, expression->line);
}

void Compiler::compileCall(const CallExpr* expression) {
    if (auto property =
        dynamic_cast<const PropertyExpr*>(expression->callee.get())) {
        compileExpr(property->target.get());

        for (const auto& argument : expression->arguments) {
            compileExpr(argument.get());
        }

        emit(
            OpCode::Invoke,
            nameConstant(property->name) * 256 +
            static_cast<int>(expression->arguments.size()),
            expression->line
        );

        return;
    }

    compileExpr(expression->callee.get());

    for (const auto& argument : expression->arguments) {
        compileExpr(argument.get());
    }

    emit(
        OpCode::Call,
        static_cast<int>(expression->arguments.size()),
        expression->line
    );
}

void Compiler::compileProperty(const PropertyExpr* expression) {
    compileExpr(expression->target.get());
    emit(OpCode::GetProperty, nameConstant(expression->name), expression->line);
}

void Compiler::compileExpr(const Expr* expression) {
    if (auto number = dynamic_cast<const NumberExpr*>(expression)) {
        emit(OpCode::Constant, addConstant(Value(number->value)), expression->line);
        return;
    }

    if (auto text = dynamic_cast<const StringExpr*>(expression)) {
        emit(OpCode::Constant, addConstant(Value(text->value)), expression->line);
        return;
    }

    if (auto flag = dynamic_cast<const BooleanExpr*>(expression)) {
        emit(
            flag->value ? OpCode::True : OpCode::False,
            expression->line
        );
        return;
    }

    if (dynamic_cast<const NilExpr*>(expression)) {
        emit(OpCode::Nil, expression->line);
        return;
    }

    if (auto name = dynamic_cast<const IdentifierExpr*>(expression)) {
        emitGet(resolveVariable(name->name, name->line), name->line);
        return;
    }

    if (auto list = dynamic_cast<const ListExpr*>(expression)) {
        for (const auto& item : list->items) {
            compileExpr(item.get());
        }

        emit(
            OpCode::BuildList,
            static_cast<int>(list->items.size()),
            expression->line
        );

        return;
    }

    if (auto map = dynamic_cast<const MapExpr*>(expression)) {
        for (const auto& entry : map->entries) {
            compileExpr(entry.first.get());
            compileExpr(entry.second.get());
        }

        emit(
            OpCode::BuildMap,
            static_cast<int>(map->entries.size()),
            expression->line
        );

        return;
    }

    if (auto unary = dynamic_cast<const UnaryExpr*>(expression)) {
        compileExpr(unary->right.get());

        if (unary->op == "-") {
            emit(OpCode::Negate, expression->line);
            return;
        }

        if (unary->op == "!") {
            emit(OpCode::Not, expression->line);
            return;
        }

        throw CompileError(
            "unknown unary operator '" + unary->op + "'",
            { expression->line, 0 }
        );
    }

    if (auto binary = dynamic_cast<const BinaryExpr*>(expression)) {
        compileBinary(binary);
        return;
    }

    if (auto logical = dynamic_cast<const LogicalExpr*>(expression)) {
        compileLogical(logical);
        return;
    }

    if (auto assign = dynamic_cast<const AssignExpr*>(expression)) {
        compileAssign(assign);
        return;
    }

    if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
        compileIndex(index);
        return;
    }

    if (auto property = dynamic_cast<const PropertyExpr*>(expression)) {
        compileProperty(property);
        return;
    }

    if (auto self = dynamic_cast<const ThisExpr*>(expression)) {
        compileThis(self);
        return;
    }

    if (auto made = dynamic_cast<const NewExpr*>(expression)) {
        compileNew(made);
        return;
    }

    if (auto above = dynamic_cast<const SuperExpr*>(expression)) {
        compileSuper(above);
        return;
    }

    if (auto function = dynamic_cast<const FunctionExpr*>(expression)) {
        beginScope();

        compileFunctionValue(
            "function",
            function->parameters,
            function->body.get(),
            expression->line,
            true
        );

        endScope(expression->line);
        return;
    }

    if (auto call = dynamic_cast<const CallExpr*>(expression)) {
        compileCall(call);
        return;
    }

    throw CompileError(
        "unknown expression",
        { expression->line, 0 }
    );
}

void Compiler::beginScope() {
    state().scopeDepth++;
}

void Compiler::endScope(int line) {
    FunctionState& current = state();

    current.scopeDepth--;

    for (int i = static_cast<int>(current.locals.size()) - 1;
         i >= 0;
         --i) {
        if (current.locals[i].depth <= current.scopeDepth) {
            break;
        }

        if (current.locals[i].captured) {
            emit(OpCode::CloseUpvalue, i, line);
        }

        current.locals.pop_back();
        current.localCount--;
    }
}

Compiler::Slot Compiler::declareVariable(
    const std::string& name,
    int line
) {
    if (state().scopeDepth > 0) {
        return {
            true,
            declareLocal(name, line)
        };
    }

    return {
        false,
        nameConstant(name)
    };
}

void Compiler::defineVariable(
    const Slot& slot,
    int line,
    bool keepValue
) {
    if (slot.isLocal) {
        emit(OpCode::SetLocal, slot.index, line);
    } else {
        emit(OpCode::SetGlobal, slot.index, line);
    }

    if (!keepValue) {
        emit(OpCode::Pop, line);
    }
}

int Compiler::declareLocal(
    const std::string& name,
    int line
) {
    FunctionState& current = state();

    if (static_cast<size_t>(current.localCount) >= MAX_LOCALS) {
        throw CompileError(
            "too many local variables in this function",
            { line, 0 }
        );
    }

    Local local;
    local.name = name;
    local.depth = current.scopeDepth;
    local.captured = false;

    current.locals.push_back(local);
    current.localCount++;

    if (current.localCount > current.maxLocalCount) {
        current.maxLocalCount = current.localCount;
    }

    return current.localCount - 1;
}

int Compiler::findLocal(
    const FunctionState& target,
    const std::string& name
) const {
    for (int i = static_cast<int>(target.locals.size()) - 1;
         i >= 0;
         --i) {
        if (target.locals[i].name == name) {
            return i;
        }
    }

    return -1;
}

int Compiler::addUpvalue(
    FunctionState& target,
    bool isLocal,
    int index
) {
    for (size_t i = 0; i < target.upvalues.size(); ++i) {
        if (
            target.upvalues[i].isLocal == isLocal &&
            target.upvalues[i].index == index
        ) {
            return static_cast<int>(i);
        }
    }

    target.upvalues.push_back({
        isLocal,
        index
    });

    return static_cast<int>(target.upvalues.size()) - 1;
}

int Compiler::resolveUpvalueAt(
    size_t level,
    const std::string& name,
    int line
) {
    if (level == 0) {
        return -1;
    }

    FunctionState& outer = functions[level - 1];

    int local = findLocal(outer, name);

    if (local != -1) {
        outer.locals[local].captured = true;
        return addUpvalue(functions[level], true, local);
    }

    int upvalue = resolveUpvalueAt(level - 1, name, line);

    if (upvalue != -1) {
        return addUpvalue(functions[level], false, upvalue);
    }

    return -1;
}

Compiler::VariableRef Compiler::resolveVariable(
    const std::string& name,
    int line
) {
    int local = findLocal(state(), name);

    if (local != -1) {
        return {
            VariableKind::Local,
            local
        };
    }

    int upvalue = resolveUpvalueAt(
        functions.size() - 1,
        name,
        line
    );

    if (upvalue != -1) {
        return {
            VariableKind::Upvalue,
            upvalue
        };
    }

    return {
        VariableKind::Global,
        nameConstant(name)
    };
}

void Compiler::emitGet(
    const VariableRef& ref,
    int line
) {
    switch (ref.kind) {
        case VariableKind::Local:
            emit(OpCode::GetLocal, ref.index, line);
            break;

        case VariableKind::Upvalue:
            emit(OpCode::GetUpvalue, ref.index, line);
            break;

        case VariableKind::Global:
            emit(OpCode::GetGlobal, ref.index, line);
            break;
    }
}

void Compiler::emitSet(
    const VariableRef& ref,
    int line
) {
    switch (ref.kind) {
        case VariableKind::Local:
            emit(OpCode::SetLocal, ref.index, line);
            break;

        case VariableKind::Upvalue:
            emit(OpCode::SetUpvalue, ref.index, line);
            break;

        case VariableKind::Global:
            emit(OpCode::SetGlobal, ref.index, line);
            break;
    }
}

void Compiler::beginFunction(
    const std::string& name,
    int arity,
    int line
) {
    if (functions.size() > 100) {
        throw CompileError(
            "functions are nested too deeply",
            { line, 0 }
        );
    }

    FunctionState next;
    next.proto = std::make_shared<FunctionProto>(
        name,
        arity,
        0
    );
    next.localCount = 0;
    next.scopeDepth = 0;

    functions.push_back(next);
}

void Compiler::endMethod(int line) {
    emit(OpCode::Nil, line);
    emit(OpCode::Return, line);

    endScope(line);

    std::shared_ptr<FunctionProto> proto = state().proto;

    proto->setSlotCount(state().maxLocalCount);

    for (const auto& upvalue : state().upvalues) {
        proto->addUpvalue(upvalue.isLocal, upvalue.index);
    }

    functions.pop_back();

    emit(
        OpCode::Constant,
        chunk().addConstant(
            Value(std::static_pointer_cast<Object>(proto))
        ),
        line
    );
}

void Compiler::endFunction(
    const Slot& slot,
    int line,
    bool keepValue
) {
    emit(OpCode::Nil, line);
    emit(OpCode::Return, line);

    endScope(line);

    std::shared_ptr<FunctionProto> proto = state().proto;

    proto->setSlotCount(state().maxLocalCount);

    for (const auto& upvalue : state().upvalues) {
        proto->addUpvalue(
            upvalue.isLocal,
            upvalue.index
        );
    }

    functions.pop_back();

    int index = chunk().addConstant(
        Value(
            std::static_pointer_cast<Object>(proto)
        )
    );

    emit(OpCode::Constant, index, line);

    defineVariable(slot, line, keepValue);
}

int Compiler::nameConstant(const std::string& name) {
    return addConstant(Value(name));
}

int Compiler::addConstant(const Value& value) {
    return chunk().addConstant(value);
}

size_t Compiler::emit(OpCode op, int line) {
    return chunk().emit(op, 0, line);
}

size_t Compiler::emit(
    OpCode op,
    int operand,
    int line
) {
    return chunk().emit(op, operand, line);
}

int Compiler::next() {
    return static_cast<int>(chunk().count());
}

void Compiler::patchJump(
    size_t position,
    int target
) {
    chunk().patch(position, target);
}

Compiler::FunctionState& Compiler::state() {
    return functions.back();
}

Chunk& Compiler::chunk() {
    return state().proto->chunk();
}
