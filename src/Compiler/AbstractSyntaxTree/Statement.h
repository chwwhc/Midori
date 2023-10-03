#pragma once

#include <optional>

#include "Expression.h"

struct Block;
struct Simple;
struct Print;
struct Let;
struct If;
struct While;
struct For;
struct Break;
struct Continue;
struct Function;
struct Return;
struct Import;
struct Namespace;
struct Halt;

using Statement = std::variant<Block, Simple, Print, Let, If, While, For, Break, Continue, Function, Return, Import, Namespace, Halt>;
using ProgramTree = std::vector<std::unique_ptr<Statement>>;

struct Block
{
    Token m_right_brace;
    std::vector<std::unique_ptr<Statement>> m_stmts;
    int m_local_count;
};

struct Simple
{
    std::unique_ptr<Expression> m_expr;
};

struct Print
{
    Token m_keyword;
    std::unique_ptr<Expression> m_expr;
};

struct Let
{
    Token m_name;
    std::unique_ptr<Expression> m_value;
    std::optional<int> m_local_index;
};

struct If
{
    Token m_if_keyword;
    std::optional<Token> m_else_keyword;
    std::unique_ptr<Expression> m_condition;
    std::unique_ptr<Statement> m_true_branch;
    std::optional<std::unique_ptr<Statement>> m_else_branch;
};

struct While
{
    Token m_while_keyword;
    std::unique_ptr<Expression> m_condition;
    std::unique_ptr<Statement> m_body;
};

struct For
{
    Token m_for_keyword;
    std::unique_ptr<Statement> m_condition_intializer;
    std::optional<std::unique_ptr<Expression>> m_condition;
    std::optional<std::unique_ptr<Statement>> m_condition_incrementer;
    std::unique_ptr<Statement> m_body;
    int m_control_block_local_count;
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

struct Namespace
{
    Token m_name;
    std::unique_ptr<Statement> m_stmts;
};

struct Halt
{
    Token m_keyword;
    std::unique_ptr<Expression> m_message;
};
