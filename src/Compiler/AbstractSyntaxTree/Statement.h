#pragma once

#include "Expression.h"

struct Block;
struct Simple;
struct Print;
struct Var;
struct If;
struct While;
struct Break;
struct Continue;
struct Function;
struct Return;
struct Import;
struct Class;
struct Namespace;
struct Halt;

using Statement = std::variant<Block, Simple, Print, Var, If, While, Break, Continue, Function, Return, Import, Class, Namespace, Halt>;
using Program = std::vector<std::unique_ptr<Statement>>;

struct Block
{
    std::vector<std::unique_ptr<Statement>> m_stmts;
};

struct Simple
{
    std::unique_ptr<Expression> m_expr;
};

struct Print
{
    std::unique_ptr<Expression> m_expr;
};

struct Var
{
    struct VarContext
    {
        Token m_name;
        bool m_is_fixed;
        std::unique_ptr<Expression> m_value;
    };

    std::vector<VarContext> m_var_inits;
};

struct If
{
    std::unique_ptr<Expression> m_condition;
    std::unique_ptr<Statement> m_true_branch;
    std::unique_ptr<Statement> m_else_branch;
};

struct While
{
    std::unique_ptr<Expression> m_condition;
    std::unique_ptr<Statement> m_body;
};

struct Break 
{
    Token m_keyword;
};

struct Continue
{
    Token m_keyword;
};

struct Function
{
    Token m_name;
    std::vector<Token> m_params;
    std::unique_ptr<Statement> m_body;
    bool m_is_sig;
    bool m_is_fixed;
};

struct Return
{
    Token m_keyword;
    std::unique_ptr<Expression> m_value;
};

struct Import
{
    Token m_keyword;
    std::unique_ptr<Expression> m_path;
};

struct Class
{
    Token m_name;
    std::unique_ptr<Expression> m_superclass;
    std::vector<std::unique_ptr<Statement>> m_methods;
    bool m_is_fixed;
};

struct Namespace
{
    Token m_name;
    bool m_is_fixed;
    std::unique_ptr<Statement> m_stmts;
};

struct Halt
{
    Token m_keyword;
    std::unique_ptr<Expression> m_message;
};
