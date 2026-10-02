#pragma once

#include <string>
#include <vector>

constexpr char INTERPOLATE_OPEN = '\x01';
constexpr char INTERPOLATE_CLOSE = '\x02';

enum class TokenType {
    Number,
    String,
    Identifier,

    Let,
    Function,
    If,
    Else,
    While,
    For,
    In,
    Switch,
    Case,
    Default,
    Break,
    Continue,
    Return,
    True,
    False,
    Nil,
    And,
    Or,
    Not,

    Class,
    New,
    This,
    Super,
    Static,
    Extends,
    Try,
    Catch,
    Finally,
    Throw,
    Import,
    Export,
    From,
    As,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,

    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,

    Equal,
    EqualEqual,
    Bang,
    BangEqual,

    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,

    Comma,
    Semicolon,
    Dot,
    Rest,
    Colon,

    EndOfFile
};

struct Token {
    TokenType type;
    std::string text;
    double number = 0;
    int line;
    int column;
};

class Lexer {
public:
    explicit Lexer(
        const std::string& source,
        const std::string& name = "main"
    );

    std::vector<Token> tokenize();

private:
    char current() const;
    char peek() const;
    char peekNext() const;
    char advance();

    bool match(char expected);
    bool matchEndOfLine();

    void scanToken();
    void scanNumber();
    void scanString();
    std::string scanNestedString();
    void scanIdentifier();
    void skipLineComment();
    void skipBlockComment();
    char readEscape();

    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& text);

    bool isAtEnd() const;
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;

    TokenType identifierType(const std::string& text) const;

    [[noreturn]] void fail(const std::string& message) const;

    std::string source;
    std::string sourceName;
    std::vector<Token> tokens;

    size_t start = 0;
    size_t currentPos = 0;

    int line = 1;
    int column = 1;

    int tokenLine = 1;
    int tokenColumn = 1;
};

const char* tokenTypeName(TokenType type);
