#include "Parser.h"

#include <iostream>

std::unique_ptr<Expression> Parser::ParseFactor() 
{ 
    return ParseBinary(&Parser::ParseUnary, Token::Type::STAR, Token::Type::SLASH, Token::Type::PERCENT); 
}

std::unique_ptr<Expression> Parser::ParseShift() 
{
    return ParseBinary(&Parser::ParseTerm, Token::Type::LEFT_SHIFT, Token::Type::RIGHT_SHIFT); 
}

std::unique_ptr<Expression> Parser::ParseTerm() 
{
    return ParseBinary(&Parser::ParseFactor, Token::Type::PLUS, Token::Type::MINUS); 
}

std::unique_ptr<Expression> Parser::ParseComparison() 
{
    return ParseBinary(&Parser::ParseShift, Token::Type::LESS, Token::Type::LESS_EQUAL, Token::Type::GREATER, Token::Type::GREATER_EQUAL);
}

std::unique_ptr<Expression> Parser::ParseEquality()
{
    return ParseBinary(&Parser::ParseComparison, Token::Type::BANG_EQUAL, Token::Type::DOUBLE_EQUAL); 
}

std::unique_ptr<Expression> Parser::ParseBitwiseAnd()
{
    return ParseBinary(&Parser::ParseEquality, Token::Type::SINGLE_AMPERSAND); 
}

std::unique_ptr<Expression> Parser::ParseBitwiseXor() 
{
    return ParseBinary(&Parser::ParseBitwiseAnd, Token::Type::CARET); 
}

std::unique_ptr<Expression> Parser::ParseBitwiseOr() 
{
    return ParseBinary(&Parser::ParseBitwiseXor, Token::Type::SINGLE_BAR); 
}

std::unique_ptr<Expression> Parser::ParseAssignment()
{
    std::unique_ptr<Expression> expr = ParseArrayLiteral();

    if (Match(Token::Type::SINGLE_EQUAL))
    {
        Token& equal = Previous();
        std::unique_ptr<Expression> value = ParseAssignment();

        if (std::holds_alternative<Variable>(*expr))
        {
            Variable& variable_expr = std::get<Variable>(*expr);
            return std::make_unique<Expression>(Assign(std::move(variable_expr.m_name), std::move(value)));
        }
        else if (std::holds_alternative<Get>(*expr))
        {
            Get& get_expr = std::get<Get>(*expr);
            return std::make_unique<Expression>(Set(std::move(get_expr.m_name), std::move(get_expr.m_object), std::move(value)));
        }
        else if (std::holds_alternative<ArrayGet>(*expr))
        {
			ArrayGet& access_expr = std::get<ArrayGet>(*expr);
			return std::make_unique<Expression>(ArraySet(std::move(access_expr.m_op), std::move(access_expr.m_arr_var), std::move(access_expr.m_indices), std::move(value)));
		}

        CompilerError::PrintError(CompilerError::Type::PARSER, "Invalid assignment target.", equal);
        m_error = true;
        Synchronize();
    }

    return expr;
}

std::unique_ptr<Expression> Parser::ParseUnary()
{
    if (Match(Token::Type::BANG, Token::Type::MINUS))
    {
		Token& op = Previous();
		std::unique_ptr<Expression> right = ParseUnary();
		return std::make_unique<Expression>(Unary(std::move(op), std::move(right)));
	}

    return ParseArrayAccess();
}

std::unique_ptr<Expression> Parser::ParseTernary()
{
    std::unique_ptr<Expression> condition = ParseLogicalOr();

    if (Match(Token::Type::QUESTION))
    {
        Token& question = Previous();
        std::unique_ptr<Expression> true_branch = ParseTernary();
        if (Match(Token::Type::SINGLE_COLON))
        {
            Token& colon = Previous();
            std::unique_ptr<Expression> else_branch = ParseTernary();
            return std::make_unique<Expression>(Ternary(std::move(question), std::move(colon), std::move(condition), std::move(true_branch), std::move(else_branch)));
        }
        else
        {
            CompilerError::PrintError(CompilerError::Type::PARSER, "Expected ':' for ternary expression.", question);
            m_error = true;
            Synchronize();
        }
    }

    return condition;
}

std::unique_ptr<Expression> Parser::ParseExpression()
{
    return ParseAssignment();
}

std::unique_ptr<Expression> Parser::ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var)
{
    Token& op = Previous();
    std::vector<std::unique_ptr<Expression>> indices;

    while (Match(Token::Type::LEFT_BRACKET))
    {
		indices.emplace_back(ParseAssignment());
        Consume(Token::Type::RIGHT_BRACKET, "Expected ']' after index.");
	}

    return std::make_unique<Expression>(ArrayGet(std::move(op), std::move(arr_var), std::move(indices)));
}

std::unique_ptr<Expression> Parser::ParseArrayAccess()
{
    std::unique_ptr<Expression> arr_var = ParseCall();

    if (Check(Token::Type::LEFT_BRACKET, 0u))
    {
		return ParseArrayAccessHelper(std::move(arr_var));
	}

    return arr_var;
}

std::unique_ptr<Expression> Parser::ParseArrayLiteral()
{
    if (Match(Token::Type::LEFT_BRACKET))
    {
        Token& op = Previous();
        std::vector<std::unique_ptr<Expression>> expr_vector;

        if (Match(Token::Type::RIGHT_BRACKET))
        {
            return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
        }
        else if (Match(Token::Type::AT))
        {
            std::unique_ptr<Expression> allocated_size = ParseTernary();
            Consume(Token::Type::RIGHT_BRACKET, "Expected ']' for array expression.");
            return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::move(allocated_size)));
        }

        do
        {
            expr_vector.emplace_back(ParseArrayLiteral());
        } while (Match(Token::Type::COMMA));

        Consume(Token::Type::RIGHT_BRACKET, "Expected ']' for array expression.");

        return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
    }

    return ParseTernary();
}

std::unique_ptr<Expression> Parser::ParseCall()
{
    std::unique_ptr<Expression> expr = ParsePrimary();

    while (true)
    {
        if (Match(Token::Type::LEFT_PAREN))
        {
            expr = FinishCall(std::move(expr));
        }
        else if (Match(Token::Type::DOT))
        {
            Token name = Consume(Token::Type::IDENTIFIER, "Expected identifier after '.'.");
            expr = std::make_unique<Expression>(Get(std::move(name), std::move(expr)));
        }
        else
        {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Expression> Parser::FinishCall(std::unique_ptr<Expression>&& callee)
{
    std::vector<std::unique_ptr<Expression>> arguments;
    if (!Check(Token::Type::RIGHT_PAREN, 0u))
    {
        do
        {
            arguments.emplace_back(ParseExpression());
        } while (Match(Token::Type::COMMA));
    }

    Token paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' after arguments.");

    return std::make_unique<Expression>(Call(std::move(paren), std::move(callee), std::move(arguments)));
}

std::unique_ptr<Expression> Parser::ParsePrimary()
{
    if (Match(Token::Type::LEFT_PAREN))
    {
        std::unique_ptr<Expression> expr_in = ParseExpression();
        Consume(Token::Type::RIGHT_PAREN, "Expected right parentheses.");
        return std::make_unique<Expression>(Group(std::move(expr_in)));
    }
    else if (Match(Token::Type::IDENTIFIER))
    {
        return std::make_unique<Expression>(Variable(std::move(Previous()), false));
    }
    else if (Match(Token::Type::BACKSLASH))
    {
        Token& backslash = Previous();
        std::vector<Token> params;
        do
        {
            if (Match(Token::Type::IDENTIFIER))
            {
                Token param = Previous();
                params.emplace_back(std::move(param));
            }
        } while (!Check(Token::Type::RIGHT_ARROW, 0u));

        Consume(Token::Type::RIGHT_ARROW, "Expected '->' after bound variables.");

        std::unique_ptr<Expression> body = ParseExpression();

        return std::make_unique<Expression>(Lambda(std::move(backslash), std::move(params), std::move(body)));
    }
    else if (Match(Token::Type::THIS))
    {
        return std::make_unique<Expression>(This(std::move(Previous())));
    }
    else if (Match(Token::Type::SUPER))
    {
        Token keyword = Previous();
        Consume(Token::Type::DOT, "Expected '.' after \"super\".");
        Token method = Consume(Token::Type::IDENTIFIER, "Expected superclass method name.");
        return std::make_unique<Expression>(Super(std::move(keyword), std::move(method)));
    }
    else if (Match(Token::Type::TRUE))
    {
        return std::make_unique<Expression>(Bool(true));
    }
    else if (Match(Token::Type::FALSE))
    {
        return std::make_unique<Expression>(Bool(false));
    }
    else if (Match(Token::Type::NIL))
    {
        return std::make_unique<Expression>(Nil());
    }
    else if (Match(Token::Type::NUMBER))
    {
        return std::make_unique<Expression>(Number(std::stod(Previous().m_lexeme)));
    }
    else if (Match(Token::Type::STRING))
    {
        return std::make_unique<Expression>(String(std::move(Previous().m_lexeme)));
    }
    else
    {
        CompilerError::PrintError(CompilerError::Type::PARSER, "Expected expression.", Previous());
        m_error = true;
        Synchronize();
        return nullptr;
    }
}

std::unique_ptr<Expression> Parser::ParseLogicalAnd()
{
    std::unique_ptr<Expression> expr = ParsePipe();

    while (Match(Token::Type::DOUBLE_AMPERSAND))
    {
        Token& op = Previous();
        std::unique_ptr<Expression> right = ParsePipe();
        expr = std::make_unique<Expression>(Logical(std::move(op), std::move(expr), std::move(right)));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::ParseLogicalOr()
{
    std::unique_ptr<Expression> expr = ParseLogicalAnd();

    while (Match(Token::Type::DOUBLE_BAR))
    {
        Token& op = Previous();
        std::unique_ptr<Expression> right = ParseLogicalAnd();
        expr = std::make_unique<Expression>(Logical(std::move(op), std::move(expr), std::move(right)));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::ParsePipe()
{
    std::unique_ptr<Expression> expr = ParseBitwiseOr();

    while (Match(Token::Type::PIPE))
    {
        Token& op = Previous();
        std::unique_ptr<Expression> right = ParseBitwiseOr();
        expr = std::make_unique<Expression>(Pipe(std::move(op), std::move(expr), std::move(right)));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::ParseComma()
{
    std::vector<std::unique_ptr<Expression>> expressions;
    do
    {
        expressions.emplace_back(ParseExpression());
    } while (Match(Token::Type::COMMA));

    return std::make_unique<Expression>(Comma(std::move(expressions)));
}

std::unique_ptr<Statement> Parser::ParseDeclaration()
{
        bool is_fixed = Match(Token::Type::FIXED);

        if (Match(Token::Type::FUN))
        {
            return ParseFunctionStatement(false, is_fixed);
        }
        else if (Match(Token::Type::LET))
        {
            return ParseVarDeclaration(is_fixed);
        }
        else if (Match(Token::Type::CLASS))
        {
            return ParseClassDeclaration(is_fixed);
        }
        else if (Match(Token::Type::NAMESPACE))
        {
            return ParseNamespaceDeclaration(is_fixed);
        }
        return ParseStatement();
}

std::unique_ptr<Statement> Parser::ParseBlockStatement()
{
    std::vector<std::unique_ptr<Statement>> statements;

    while (!IsAtEnd() && !Check(Token::Type::RIGHT_BRACE, 0))
    {
        statements.emplace_back(ParseDeclaration());
    }

    Consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");

    return std::make_unique<Statement>(Block(std::move(statements)));
}

std::unique_ptr<Statement> Parser::ParseVarDeclaration(bool is_fixed)
{
    std::vector<Let::LetContext> let_inits;
    do
    {
        Token name = Consume(Token::Type::IDENTIFIER, "Expected variable name.");

        if (Match(Token::Type::SINGLE_EQUAL))
        {
            std::unique_ptr<Expression> initializer = ParseExpression();
            let_inits.emplace_back(std::move(name), std::move(is_fixed), std::move(initializer));
        }
        else
        {
            let_inits.emplace_back(std::move(name), std::move(is_fixed), nullptr);
        }
    } while (Match(Token::Type::COMMA));
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after variable declaration.");

    return std::make_unique<Statement>(Let(std::move(let_inits)));
}

std::unique_ptr<Statement> Parser::ParseNamespaceDeclaration(bool is_fixed)
{
    Token name = Consume(Token::Type::IDENTIFIER, "Expected module name.");
    Consume(Token::Type::LEFT_BRACE, "Expected '{' before module body.");
    return std::make_unique<Statement>(Namespace(std::move(name), std::move(is_fixed), ParseBlockStatement()));
}

std::unique_ptr<Statement> Parser::ParseClassDeclaration(bool is_fixed)
{
    Token name = Consume(Token::Type::IDENTIFIER, "Expected class name.");

    std::unique_ptr<Expression> superclass;
    if (Match(Token::Type::LESS))
    {
        superclass = ParseExpression();
    }

    Consume(Token::Type::LEFT_BRACE, "Expected '{' before class body.");

    std::vector<std::unique_ptr<Statement>> instance_methods;
    while (!IsAtEnd() && !Check(Token::Type::RIGHT_BRACE, 0u))
    {
        bool is_sig = Match(Token::Type::SIG);

        instance_methods.emplace_back(ParseFunctionStatement(is_sig, is_fixed));
    }

    Consume(Token::Type::RIGHT_BRACE, "Expected '}' after class body.");

    return std::make_unique<Statement>(Class(std::move(name), std::move(superclass), std::move(instance_methods), std::move(is_fixed)));
}

std::unique_ptr<Statement> Parser::ParseFunctionStatement(bool is_sig, bool is_fixed)
{
    Token name = Consume(Token::Type::IDENTIFIER, "Expected function name.");

    if (is_sig)
    {
        Consume(Token::Type::LEFT_PAREN, "Expected '(' after declaring a function signature.");
        Consume(Token::Type::RIGHT_PAREN, "Expected ')' after declaring a function signature.");
        Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after declaring a function signature.");
        return std::make_unique<Statement>(Function(std::move(name), std::vector<Token>(), nullptr, is_sig, is_fixed));
     
    }

    Consume(Token::Type::LEFT_PAREN, "Expected '(' after function name.");
    std::vector<Token> params;
    if (!Check(Token::Type::RIGHT_PAREN, 0u))
    {
        do
        {
            params.emplace_back(Consume(Token::Type::IDENTIFIER, "Expected parameter name."));
        } while (Match(Token::Type::COMMA));
    }
    Consume(Token::Type::RIGHT_PAREN, "Expected ')' after parameters.");

    Consume(Token::Type::LEFT_BRACE, "Expected '{' before function body.");

    return std::make_unique<Statement>(Function(std::move(name), std::move(params), ParseBlockStatement(), is_sig, is_fixed));
}

std::unique_ptr<Statement> Parser::ParseIfStatement()
{
    Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"if\".");
    std::unique_ptr<Expression> condition = ParseExpression();
    Consume(Token::Type::RIGHT_PAREN, "Expected ')' after if condition.");

    std::unique_ptr<Statement> then_branch = ParseStatement();
    std::unique_ptr<Statement> else_branch;
    if (Match(Token::Type::ELSE))
    {
        else_branch = ParseStatement();
    }

    return std::make_unique<Statement>(If(std::move(condition), std::move(then_branch), std::move(else_branch)));
}

std::unique_ptr<Statement> Parser::ParseWhileStatement()
{
    Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"while\".");
    std::unique_ptr<Expression> condition = ParseExpression();

    Consume(Token::Type::RIGHT_PAREN, "Expected ')' after condition.");
    std::unique_ptr<Statement> body = ParseStatement();

    return std::make_unique<Statement>(While(std::move(condition), std::move(body)));
}

std::unique_ptr<Statement> Parser::ParseForStatement()
{
    Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"for\".");
    std::unique_ptr<Statement> initializer;
    if (Match(Token::Type::LET))
    {
        initializer = ParseVarDeclaration(false);
    }
    else if (!Match(Token::Type::SINGLE_SEMICOLON))
    {
        initializer = ParseSimpleStatement();
    }

    std::unique_ptr<Expression> condition;
    if (!Check(Token::Type::SINGLE_SEMICOLON, 0u))
    {
        condition = ParseExpression();
    }
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"for\" condition.");

    std::unique_ptr<Expression> increment;
    if (!Check(Token::Type::RIGHT_PAREN, 0u))
    {
        increment = ParseExpression();
    }
    Consume(Token::Type::RIGHT_PAREN, "Expected ')' after \"for\" clauses.");

    std::unique_ptr<Statement> body = ParseStatement();

    if (increment != nullptr)
    {
        std::get<Block>(*body).m_stmts.emplace_back(std::make_unique<Statement>(Simple(std::move(increment))));
    }
    if (condition == nullptr)
    {
        condition = std::make_unique<Expression>(Bool(true));
    }

    if (initializer != nullptr)
    {
        std::get<Block>(*body).m_stmts.insert(std::get<Block>(*body).m_stmts.begin(), std::move(initializer));
    }
    body = std::make_unique<Statement>(While(std::move(condition), std::move(body)));

    return body;
}

std::unique_ptr<Statement> Parser::ParseBreakStatement()
{
    Token& keyword = Previous();
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"break\".");
    return std::make_unique<Statement>(Break(std::move(keyword)));
}

std::unique_ptr<Statement> Parser::ParseContinueStatement()
{
    Token& keyword = Previous();
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"continue\".");
    return std::make_unique<Statement>(Continue(std::move(keyword)));
}

std::unique_ptr<Statement> Parser::ParsePrintStatement()
{
    std::unique_ptr<Expression> value = ParseExpression();
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after value.");
    return std::make_unique<Statement>(Print(std::move(value)));
}

std::unique_ptr<Statement> Parser::ParseSimpleStatement()
{
    std::unique_ptr<Expression> expr = ParseComma();
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after expression.");
    return std::make_unique<Statement>(Simple(std::move(expr)));
}

std::unique_ptr<Statement> Parser::ParseHaltStatement()
{
    Token& keyword = Previous();
    std::unique_ptr<Expression> message = ParseExpression();

    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"halt\".");

    return std::make_unique<Statement>(Halt(std::move(keyword), std::move(message)));
}

std::unique_ptr<Statement> Parser::ParseImportStatement()
{
    Token& keyword = Previous();
    std::unique_ptr<Expression> path = ParseComma();
    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after script path.");
    return std::make_unique<Statement>(Import(std::move(keyword), std::move(path)));
}

std::unique_ptr<Statement> Parser::ParseReturnStatement()
{
    Token& keyword = Previous();
    std::unique_ptr<Expression> value;
    if (!Check(Token::Type::SINGLE_SEMICOLON, 0u))
    {
        value = ParseExpression();
    }

    Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after return value.");

    return std::make_unique<Statement>(Return(std::move(keyword), std::move(value)));
}


std::unique_ptr<Statement> Parser::ParseStatement()
{
    if (Match(Token::Type::PRINT))
    {
        return ParsePrintStatement();
    }
    else if (Match(Token::Type::LEFT_BRACE))
    {
        return ParseBlockStatement();
    }
    else if (Match(Token::Type::IF))
    {
        return ParseIfStatement();
    }
    else if (Match(Token::Type::WHILE))
    {
        return ParseWhileStatement();
    }
    else if (Match(Token::Type::FOR))
    {
        return ParseForStatement();
    }
    else if (Match(Token::Type::BREAK))
    {
        return ParseBreakStatement();
    }
    else if (Match(Token::Type::CONTINUE))
    {
        return ParseContinueStatement();
    }
    else if (Match(Token::Type::RETURN))
    {
        return ParseReturnStatement();
    }
    else if (Match(Token::Type::IMPORT))
    {
        return ParseImportStatement();
    }
    else if (Match(Token::Type::HALT))
    {
        return ParseHaltStatement();
    }
    else
    {
        return ParseSimpleStatement();
    }
}

Parser::ParserResult Parser::Parse()
{
    Program program;
    while (!IsAtEnd())
    {
		program.emplace_back(ParseDeclaration());
	}

    return ParserResult(std::move(program), m_error);
}

void Parser::Synchronize()
{
    Advance();

    while (!IsAtEnd())
    {
        if (Previous().m_type == Token::Type::SINGLE_SEMICOLON)
        {
            return;
        }
        switch (Peek(0u).m_type)
        {
        case Token::Type::CLASS:
        case Token::Type::FUN:
        case Token::Type::LET:
        case Token::Type::FOR:
        case Token::Type::IF:
        case Token::Type::WHILE:
        case Token::Type::PRINT:
        case Token::Type::RETURN:
            return;
        default:
            break;
        }

        Advance();
    }
}