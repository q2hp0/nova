#include "ast.h"

FunctionExpr::~FunctionExpr() = default;

static std::unique_ptr<BlockStmt> cloneBlock(const BlockStmt* block) {
    auto copy = std::make_unique<BlockStmt>(block->line);

    for (const auto& inner : block->statements) {
        copy->statements.push_back(cloneStmt(inner.get()));
    }

    return copy;
}

std::unique_ptr<Expr> cloneExpr(const Expr* expression) {
    if (expression == nullptr) {
        return nullptr;
    }

    if (auto number = dynamic_cast<const NumberExpr*>(expression)) {
        return std::make_unique<NumberExpr>(
            number->line,
            number->value
        );
    }

    if (auto text = dynamic_cast<const StringExpr*>(expression)) {
        return std::make_unique<StringExpr>(
            text->line,
            text->value
        );
    }

    if (auto flag = dynamic_cast<const BooleanExpr*>(expression)) {
        return std::make_unique<BooleanExpr>(
            flag->line,
            flag->value
        );
    }

    if (auto nothing = dynamic_cast<const NilExpr*>(expression)) {
        return std::make_unique<NilExpr>(nothing->line);
    }

    if (auto name = dynamic_cast<const IdentifierExpr*>(expression)) {
        return std::make_unique<IdentifierExpr>(
            name->line,
            name->name
        );
    }

    if (auto list = dynamic_cast<const ListExpr*>(expression)) {
        std::vector<std::unique_ptr<Expr>> items;

        for (const auto& item : list->items) {
            items.push_back(cloneExpr(item.get()));
        }

        return std::make_unique<ListExpr>(
            list->line,
            std::move(items)
        );
    }

    if (auto map = dynamic_cast<const MapExpr*>(expression)) {
        std::vector<
            std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>
        > entries;

        for (const auto& entry : map->entries) {
            entries.emplace_back(
                cloneExpr(entry.first.get()),
                cloneExpr(entry.second.get())
            );
        }

        return std::make_unique<MapExpr>(
            map->line,
            std::move(entries)
        );
    }

    if (auto unary = dynamic_cast<const UnaryExpr*>(expression)) {
        return std::make_unique<UnaryExpr>(
            unary->line,
            unary->op,
            cloneExpr(unary->right.get())
        );
    }

    if (auto binary = dynamic_cast<const BinaryExpr*>(expression)) {
        return std::make_unique<BinaryExpr>(
            binary->line,
            cloneExpr(binary->left.get()),
            binary->op,
            cloneExpr(binary->right.get())
        );
    }

    if (auto logical = dynamic_cast<const LogicalExpr*>(expression)) {
        return std::make_unique<LogicalExpr>(
            logical->line,
            cloneExpr(logical->left.get()),
            logical->op,
            cloneExpr(logical->right.get())
        );
    }

    if (auto assign = dynamic_cast<const AssignExpr*>(expression)) {
        return std::make_unique<AssignExpr>(
            assign->line,
            cloneExpr(assign->target.get()),
            cloneExpr(assign->value.get())
        );
    }

    if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
        return std::make_unique<IndexExpr>(
            index->line,
            cloneExpr(index->target.get()),
            cloneExpr(index->index.get())
        );
    }

    if (auto property = dynamic_cast<const PropertyExpr*>(expression)) {
        return std::make_unique<PropertyExpr>(
            property->line,
            cloneExpr(property->target.get()),
            property->name
        );
    }

    if (auto function = dynamic_cast<const FunctionExpr*>(expression)) {
        return std::make_unique<FunctionExpr>(
            function->line,
            function->parameters,
            cloneBlock(function->body.get())
        );
    }

    if (auto call = dynamic_cast<const CallExpr*>(expression)) {
        std::vector<std::unique_ptr<Expr>> arguments;

        for (const auto& argument : call->arguments) {
            arguments.push_back(cloneExpr(argument.get()));
        }

        return std::make_unique<CallExpr>(
            call->line,
            cloneExpr(call->callee.get()),
            std::move(arguments)
        );
    }

    return nullptr;
}

std::unique_ptr<Stmt> cloneStmt(const Stmt* statement) {
    if (statement == nullptr) {
        return nullptr;
    }

    if (auto expression =
            dynamic_cast<const ExpressionStmt*>(statement)) {
        return std::make_unique<ExpressionStmt>(
            expression->line,
            cloneExpr(expression->expression.get())
        );
    }

    if (auto let = dynamic_cast<const LetStmt*>(statement)) {
        return std::make_unique<LetStmt>(
            let->line,
            let->name,
            cloneExpr(let->initializer.get())
        );
    }

    if (auto block = dynamic_cast<const BlockStmt*>(statement)) {
        return cloneBlock(block);
    }

    if (auto branch = dynamic_cast<const IfStmt*>(statement)) {
        return std::make_unique<IfStmt>(
            branch->line,
            cloneExpr(branch->condition.get()),
            cloneStmt(branch->thenBranch.get()),
            cloneStmt(branch->elseBranch.get())
        );
    }

    if (auto loop = dynamic_cast<const WhileStmt*>(statement)) {
        return std::make_unique<WhileStmt>(
            loop->line,
            cloneExpr(loop->condition.get()),
            cloneStmt(loop->body.get())
        );
    }

    if (auto loop = dynamic_cast<const ForStmt*>(statement)) {
        return std::make_unique<ForStmt>(
            loop->line,
            cloneStmt(loop->initializer.get()),
            cloneExpr(loop->condition.get()),
            cloneExpr(loop->increment.get()),
            cloneStmt(loop->body.get())
        );
    }

    if (auto loop = dynamic_cast<const ForInStmt*>(statement)) {
        return std::make_unique<ForInStmt>(
            loop->line,
            loop->name,
            cloneExpr(loop->sequence.get()),
            cloneStmt(loop->body.get())
        );
    }

    if (dynamic_cast<const BreakStmt*>(statement)) {
        return std::make_unique<BreakStmt>(statement->line);
    }

    if (dynamic_cast<const ContinueStmt*>(statement)) {
        return std::make_unique<ContinueStmt>(statement->line);
    }

    if (auto returnStatement =
            dynamic_cast<const ReturnStmt*>(statement)) {
        return std::make_unique<ReturnStmt>(
            returnStatement->line,
            cloneExpr(returnStatement->value.get())
        );
    }

    if (auto function =
            dynamic_cast<const FunctionStmt*>(statement)) {
        return std::make_unique<FunctionStmt>(
            function->line,
            function->name,
            function->parameters,
            cloneBlock(function->body.get())
        );
    }

    return nullptr;
}
