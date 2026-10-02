#include "lexer.h"

#include "../errors/errors.h"

#include <cctype>
#include <unordered_map>

Lexer::Lexer(
    const std::string& source,
    const std::string& name
)
    : source(source), sourceName(name) {
}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        start = currentPos;
        tokenLine = line;
        tokenColumn = column;
        scanToken();
    }

    tokens.push_back({
        TokenType::EndOfFile,
        "",
        0,
        line,
        column
    });

    return tokens;
}

char Lexer::current() const {
    if (isAtEnd()) {
        return '\0';
    }

    return source[currentPos];
}

char Lexer::peek() const {
    if (currentPos + 1 >= source.size()) {
        return '\0';
    }

    return source[currentPos + 1];
}

char Lexer::peekNext() const {
    if (currentPos + 2 >= source.size()) {
        return '\0';
    }

    return source[currentPos + 2];
}

char Lexer::advance() {
    if (isAtEnd()) {
        return '\0';
    }

    char c = source[currentPos++];

    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }

    return c;
}

bool Lexer::match(char expected) {
    if (current() != expected) {
        return false;
    }

    advance();
    return true;
}

bool Lexer::matchEndOfLine() {
    if (current() == '\n') {
        advance();
        return true;
    }

    if (current() == '\r' && peek() == '\n') {
        advance();
        advance();
        return true;
    }

    return false;
}

void Lexer::scanToken() {
    char c = advance();

    switch (c) {
        case '(':
            addToken(TokenType::LeftParen);
            break;

        case ')':
            addToken(TokenType::RightParen);
            break;

        case '{':
            addToken(TokenType::LeftBrace);
            break;

        case '}':
            addToken(TokenType::RightBrace);
            break;

        case '[':
            addToken(TokenType::LeftBracket);
            break;

        case ']':
            addToken(TokenType::RightBracket);
            break;

        case ',':
            addToken(TokenType::Comma);
            break;

        case ';':
            addToken(TokenType::Semicolon);
            break;

        case '.':
            if (match('.') && match('.')) {
                addToken(TokenType::Rest);
                break;
            }

            addToken(TokenType::Dot);
            break;

        case ':':
            addToken(TokenType::Colon);
            break;

        case '+':
            addToken(
                match('=')
                    ? TokenType::PlusEqual
                    : TokenType::Plus
            );
            break;

        case '-':
            addToken(
                match('=')
                    ? TokenType::MinusEqual
                    : TokenType::Minus
            );
            break;

        case '*':
            addToken(
                match('=')
                    ? TokenType::StarEqual
                    : TokenType::Star
            );
            break;

        case '/':
            if (match('/')) {
                skipLineComment();
            } else if (match('*')) {
                skipBlockComment();
            } else {
                addToken(
                    match('=')
                        ? TokenType::SlashEqual
                        : TokenType::Slash
                );
            }
            break;

        case '%':
            addToken(
                match('=')
                    ? TokenType::PercentEqual
                    : TokenType::Percent
            );
            break;

        case '=':
            addToken(
                match('=')
                    ? TokenType::EqualEqual
                    : TokenType::Equal
            );
            break;

        case '!':
            addToken(
                match('=')
                    ? TokenType::BangEqual
                    : TokenType::Bang
            );
            break;

        case '<':
            addToken(
                match('=')
                    ? TokenType::LessEqual
                    : TokenType::Less
            );
            break;

        case '>':
            addToken(
                match('=')
                    ? TokenType::GreaterEqual
                    : TokenType::Greater
            );
            break;

        case '&':
            if (!match('&')) {
                fail("unexpected character '&', did you mean '&&'?");
            }

            addToken(TokenType::And, "&&");
            break;

        case '|':
            if (!match('|')) {
                fail("unexpected character '|', did you mean '||'?");
            }

            addToken(TokenType::Or, "||");
            break;

        case '"':
            scanString();
            break;

        case ' ':
        case '\r':
        case '\t':
        case '\n':
            break;

        default:
            if (isDigit(c)) {
                scanNumber();
            } else if (isAlpha(c)) {
                scanIdentifier();
            } else {
                std::string message = "unexpected character ";

                if (c >= 32 && c < 127) {
                    message += std::string("'") + c + "'";
                } else {
                    message +=
                        "with code " +
                        std::to_string(
                            static_cast<int>(
                                static_cast<unsigned char>(c)
                            )
                        );
                }

                fail(message);
            }

            break;
    }
}

void Lexer::skipLineComment() {
    while (current() != '\n' && !isAtEnd()) {
        advance();
    }
}

void Lexer::skipBlockComment() {
    while (!isAtEnd()) {
        if (current() == '*' && peek() == '/') {
            advance();
            advance();
            return;
        }

        advance();
    }

    fail("unfinished block comment");
}

void Lexer::scanNumber() {
    while (isDigit(current())) {
        advance();
    }

    if (current() == '.' && isDigit(peek())) {
        advance();

        while (isDigit(current())) {
            advance();
        }
    }

    if (
        current() == 'e' &&
        (isDigit(peek()) ||
            ((peek() == '+' || peek() == '-') &&
                isDigit(peekNext())))
    ) {
        advance();

        if (current() == '+' || current() == '-') {
            advance();
        }

        while (isDigit(current())) {
            advance();
        }
    }

    std::string text = source.substr(
        start,
        currentPos - start
    );

    Token token;
    token.type = TokenType::Number;
    token.text = text;
    token.number = std::stod(text);
    token.line = tokenLine;
    token.column = tokenColumn;

    tokens.push_back(token);
}

char Lexer::readEscape() {
    char c = advance();

    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return '\0';
        case '\\': return '\\';
        case '"': return '"';
        case '{': return '{';
        case '}': return '}';
        case '\'': return '\'';
        case 'e': return '\x1b';
        default:
            fail(
                std::string("unknown escape sequence \\") + c
            );
    }
}

std::string Lexer::scanNestedString() {
    std::string text;

    advance();

    while (current() != '"') {
        if (isAtEnd() || current() == '\n') {
            fail("unfinished string literal");
        }

        if (match('\\')) {
            text += readEscape();
            continue;
        }

        text += advance();
    }

    advance();

    return "\"" + text + "\"";
}

void Lexer::scanString() {
    std::string text;
    int depth = 0;
    bool filling = false;

    while (true) {
        if (isAtEnd() || current() == '\n') {
            fail("unfinished string literal");
        }

        if (!filling) {
            if (current() == '"') {
                break;
            }

            if (current() == '{') {
                text += INTERPOLATE_OPEN;
                filling = true;
                advance();
                continue;
            }

            if (current() == '}') {
                text += INTERPOLATE_CLOSE;
                advance();
                continue;
            }
        } else if (current() == '"') {
            text += scanNestedString();
            continue;
        } else if (current() == '{') {
            depth++;
            text += advance();
            continue;
        } else if (current() == '}') {
            if (depth == 0) {
                text += INTERPOLATE_CLOSE;
                filling = false;
                advance();
                continue;
            }

            depth--;
            text += advance();
            continue;
        }

        if (match('\\')) {
            text += readEscape();
            continue;
        }

        text += advance();
    }

    advance();

    Token token;
    token.type = TokenType::String;
    token.text = text;
    token.number = 0;
    token.line = tokenLine;
    token.column = tokenColumn;

    tokens.push_back(token);
}

void Lexer::scanIdentifier() {
    while (isAlphaNumeric(current())) {
        advance();
    }

    std::string text = source.substr(
        start,
        currentPos - start
    );

    addToken(identifierType(text), text);
}

void Lexer::addToken(TokenType type) {
    addToken(
        type,
        source.substr(start, currentPos - start)
    );
}

void Lexer::addToken(
    TokenType type,
    const std::string& text
) {
    Token token;
    token.type = type;
    token.text = text;
    token.number = 0;
    token.line = tokenLine;
    token.column = tokenColumn;

    tokens.push_back(token);
}

bool Lexer::isAtEnd() const {
    return currentPos >= source.size();
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_'
    );
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

TokenType Lexer::identifierType(
    const std::string& text
) const {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"let", TokenType::Let},
        {"function", TokenType::Function},
        {"if", TokenType::If},
        {"else", TokenType::Else},
        {"while", TokenType::While},
        {"for", TokenType::For},
        {"in", TokenType::In},
        {"switch", TokenType::Switch},
        {"case", TokenType::Case},
        {"default", TokenType::Default},
        {"break", TokenType::Break},
        {"continue", TokenType::Continue},
        {"return", TokenType::Return},
        {"true", TokenType::True},
        {"false", TokenType::False},
        {"nil", TokenType::Nil},
        {"null", TokenType::Nil},
        {"and", TokenType::And},
        {"or", TokenType::Or},
        {"not", TokenType::Not},
        {"class", TokenType::Class},
        {"new", TokenType::New},
        {"this", TokenType::This},
        {"super", TokenType::Super},
        {"static", TokenType::Static},
        {"extends", TokenType::Extends},
        {"try", TokenType::Try},
        {"catch", TokenType::Catch},
        {"finally", TokenType::Finally},
        {"throw", TokenType::Throw},
        {"import", TokenType::Import},
        {"export", TokenType::Export},
        {"from", TokenType::From},
        {"as", TokenType::As}
    };

    auto it = keywords.find(text);

    if (it != keywords.end()) {
        return it->second;
    }

    return TokenType::Identifier;
}

void Lexer::fail(const std::string& message) const {
    throw LexerError(message, {
        tokenLine,
        tokenColumn
    });
}

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::Number: return "number";
        case TokenType::String: return "string";
        case TokenType::Identifier: return "identifier";

        case TokenType::Let: return "let";
        case TokenType::Function: return "function";
        case TokenType::If: return "if";
        case TokenType::Else: return "else";
        case TokenType::While: return "while";
        case TokenType::For: return "for";
        case TokenType::In: return "in";
        case TokenType::Switch: return "switch";
        case TokenType::Case: return "case";
        case TokenType::Default: return "default";
        case TokenType::Break: return "break";
        case TokenType::Continue: return "continue";
        case TokenType::Return: return "return";
        case TokenType::True: return "true";
        case TokenType::False: return "false";
        case TokenType::Nil: return "nil";
        case TokenType::And: return "and";
        case TokenType::Or: return "or";
        case TokenType::Not: return "not";
        case TokenType::Class: return "class";
        case TokenType::New: return "new";
        case TokenType::This: return "this";
        case TokenType::Super: return "super";
        case TokenType::Static: return "static";
        case TokenType::Extends: return "extends";
        case TokenType::Try: return "try";
        case TokenType::Catch: return "catch";
        case TokenType::Finally: return "finally";
        case TokenType::Throw: return "throw";
        case TokenType::Import: return "import";
        case TokenType::Export: return "export";
        case TokenType::From: return "from";
        case TokenType::As: return "as";

        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";

        case TokenType::PlusEqual: return "+=";
        case TokenType::MinusEqual: return "-=";
        case TokenType::StarEqual: return "*=";
        case TokenType::SlashEqual: return "/=";
        case TokenType::PercentEqual: return "%=";

        case TokenType::Equal: return "=";
        case TokenType::EqualEqual: return "==";
        case TokenType::Bang: return "!";
        case TokenType::BangEqual: return "!=";

        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";

        case TokenType::LeftParen: return "(";
        case TokenType::RightParen: return ")";
        case TokenType::LeftBrace: return "{";
        case TokenType::RightBrace: return "}";
        case TokenType::LeftBracket: return "[";
        case TokenType::RightBracket: return "]";

        case TokenType::Comma: return ",";
        case TokenType::Semicolon: return ";";
        case TokenType::Dot: return ".";
        case TokenType::Rest: return "...";
        case TokenType::Colon: return ":";

        case TokenType::EndOfFile: return "end of file";
    }

    return "unknown";
}
