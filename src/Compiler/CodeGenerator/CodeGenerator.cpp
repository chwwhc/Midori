#pragma warning(disable: 4100)

#include "CodeGenerator.h"

CodeGenerator::CodeGeneratorResult CodeGenerator::GenerateCode(ProgramTree&& program_tree)
{

	for (std::unique_ptr<Statement>& statement : program_tree)
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
	}

	return CodeGenerator::CodeGeneratorResult(std::move(m_module), m_error);
}

void CodeGenerator::operator()(Block& block)
{
	for (std::unique_ptr<Statement>& statement : block.m_stmts)
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
	}

	while (block.m_local_count > 0)
	{
		int count_to_pop = std::min(block.m_local_count, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_MULTIPLE, block.m_right_brace.m_line);
		EmitByte(static_cast<OpCode>(count_to_pop), block.m_right_brace.m_line);
		block.m_local_count -= count_to_pop;
	}
}

void CodeGenerator::operator()(Simple& simple)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *simple.m_expr);
}

void CodeGenerator::operator()(Print& print)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *print.m_expr);
	EmitByte(OpCode::PRINT, print.m_keyword.m_line);
}

void CodeGenerator::operator()(Let& let)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *let.m_value);

	if (!let.m_local_index.has_value())
	{
		std::string variable_name = let.m_name.m_lexeme;
		int index = m_module.AddGlobalVariable(std::move(variable_name));
		m_global_variables[let.m_name.m_lexeme] = index;
		EmitVariable(index, OpCode::DEFINE_GLOBAL, let.m_name.m_line);
	}
}

void CodeGenerator::operator()(If& if_stmt)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_condition);
	int true_jump = EmitJump(OpCode::JUMP_IF_FALSE, if_stmt.m_if_keyword.m_line);
	EmitByte(OpCode::POP, if_stmt.m_if_keyword.m_line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_true_branch);

	int else_jump = EmitJump(OpCode::JUMP, if_stmt.m_if_keyword.m_line);
	PatchJump(true_jump);
	EmitByte(OpCode::POP, if_stmt.m_if_keyword.m_line);

	if (if_stmt.m_else_branch.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_else_branch.value());
	}

	PatchJump(else_jump);
}

void CodeGenerator::operator()(While& while_stmt)
{
	int loop_start = m_module.GetByteCodeSize();

	std::visit([this](auto&& arg) {(*this)(arg); }, *while_stmt.m_condition);

	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, while_stmt.m_while_keyword.m_line);
	EmitByte(OpCode::POP, while_stmt.m_while_keyword.m_line);

	std::visit([this](auto&& arg) {(*this)(arg); }, *while_stmt.m_body);

	EmitLoop(loop_start, while_stmt.m_while_keyword.m_line);

	PatchJump(jump_if_false);
}

void CodeGenerator::operator()(For& for_stmt)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition_intializer);

	int loop_start = m_module.GetByteCodeSize();

	if (for_stmt.m_condition.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition.value());
	}
	else
	{
		EmitByte(OpCode::TRUE, for_stmt.m_for_keyword.m_line);
	}

	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, for_stmt.m_for_keyword.m_line);
	EmitByte(OpCode::POP, for_stmt.m_for_keyword.m_line);

	std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_body);

	if (for_stmt.m_condition_incrementer.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition_incrementer.value());
	}

	EmitLoop(loop_start, for_stmt.m_for_keyword.m_line);

	PatchJump(jump_if_false);
	while (for_stmt.m_control_block_local_count > 0)
	{
		int count_to_pop = std::min(for_stmt.m_control_block_local_count, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_MULTIPLE, for_stmt.m_for_keyword.m_line);
		EmitByte(static_cast<OpCode>(count_to_pop), for_stmt.m_for_keyword.m_line);
		for_stmt.m_control_block_local_count -= count_to_pop;
	}
}

void CodeGenerator::operator()(Break&)
{
	return;
}

void CodeGenerator::operator()(Continue&)
{
	return;
}

void CodeGenerator::operator()(Function& function)
{
	return;
}

void CodeGenerator::operator()(Return& return_stmt)
{
	return;
}

void CodeGenerator::operator()(Import& import)
{
	return;
}

void CodeGenerator::operator()(Namespace& namespace_stmt)
{
	return;
}

void CodeGenerator::operator()(Halt&)
{
	return;
}

void CodeGenerator::operator()(Binary& binary)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_left);
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_right);

	switch (binary.m_op.m_type)
	{
	case Token::Type::PLUS:
		EmitByte(OpCode::ADD, binary.m_op.m_line);
		break;
	case Token::Type::MINUS:
		EmitByte(OpCode::SUBTRACT, binary.m_op.m_line);
		break;
	case Token::Type::STAR:
		EmitByte(OpCode::MULTIPLY, binary.m_op.m_line);
		break;
	case Token::Type::SLASH:
		EmitByte(OpCode::DIVIDE, binary.m_op.m_line);
		break;
	case Token::Type::PERCENT:
		EmitByte(OpCode::MODULO, binary.m_op.m_line);
		break;
	case Token::Type::LEFT_SHIFT:
		EmitByte(OpCode::LEFT_SHIFT, binary.m_op.m_line);
		break;
	case Token::Type::RIGHT_SHIFT:
		EmitByte(OpCode::RIGHT_SHIFT, binary.m_op.m_line);
		break;
	case Token::Type::LESS:
		EmitByte(OpCode::LESS, binary.m_op.m_line);
		break;
	case Token::Type::LESS_EQUAL:
		EmitByte(OpCode::LESS_EQUAL, binary.m_op.m_line);
		break;
	case Token::Type::GREATER:
		EmitByte(OpCode::GREATER, binary.m_op.m_line);
		break;
	case Token::Type::GREATER_EQUAL:
		EmitByte(OpCode::GREATER_EQUAL, binary.m_op.m_line);
		break;
	case Token::Type::BANG_EQUAL:
		EmitByte(OpCode::NOT_EQUAL, binary.m_op.m_line);
		break;
	case Token::Type::DOUBLE_EQUAL:
		EmitByte(OpCode::EQUAL, binary.m_op.m_line);
		break;
	case Token::Type::SINGLE_AMPERSAND:
		EmitByte(OpCode::BITWISE_AND, binary.m_op.m_line);
		break;
	case Token::Type::SINGLE_BAR:
		EmitByte(OpCode::BITWISE_OR, binary.m_op.m_line);
		break;
	case Token::Type::CARET:
		EmitByte(OpCode::BITWISE_XOR, binary.m_op.m_line);
		break;
	default:
		break; // Unreachable
	}
	return;
}

void CodeGenerator::operator()(Pipe& pipe)
{
	return;
}

void CodeGenerator::operator()(Logical& logical)
{
	if (logical.m_op.m_type == Token::Type::DOUBLE_BAR)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_left);
		int jump_if_true = EmitJump(OpCode::JUMP_IF_TRUE, logical.m_op.m_line);
		EmitByte(OpCode::POP, logical.m_op.m_line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_right);
		PatchJump(jump_if_true);
	}
	else if (logical.m_op.m_type == Token::Type::DOUBLE_AMPERSAND)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_left);
		int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, logical.m_op.m_line);
		EmitByte(OpCode::POP, logical.m_op.m_line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_right);
		PatchJump(jump_if_false);
	}
	else
	{
		CompilerError::PrintError("Unrecognized logical operator.");
		m_error = true;
	}
}

void CodeGenerator::operator()(Group& group)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *group.m_expr_in);
}

void CodeGenerator::operator()(Unary& unary)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *unary.m_right);

	switch (unary.m_op.m_type)
	{
	case Token::Type::MINUS:
		EmitByte(OpCode::NEGATE, unary.m_op.m_line);
		break;
	case Token::Type::BANG:
		EmitByte(OpCode::NOT, unary.m_op.m_line);
		break;
	default:
		break; // Unreachable
	}

	return;
}

void CodeGenerator::operator()(Call& call)
{
	return;
}

void CodeGenerator::operator()(Get& get)
{
	return;
}

void CodeGenerator::operator()(Set& set)
{
	return;
}

void CodeGenerator::operator()(Variable& variable)
{
	if (variable.m_stack_offset.has_value())
	{
		EmitVariable(variable.m_stack_offset.value(), OpCode::GET_LOCAL, variable.m_name.m_line);
	}
	else
	{
		EmitVariable(m_global_variables[variable.m_name.m_lexeme], OpCode::GET_GLOBAL, variable.m_name.m_line);
	}
}

void CodeGenerator::operator()(Assign& assign)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *assign.m_value);

	if (assign.m_stack_offset.has_value())
	{
		EmitVariable(assign.m_stack_offset.value(), OpCode::SET_LOCAL, assign.m_name.m_line);
	}
	else
	{
		EmitVariable(m_global_variables[assign.m_name.m_lexeme], OpCode::SET_GLOBAL, assign.m_name.m_line);
	}
	EmitByte(OpCode::POP, assign.m_name.m_line);
}

void CodeGenerator::operator()(String& string)
{
	EmitConstant(new Object(string.m_token.m_lexeme), string.m_token.m_line);
}

void CodeGenerator::operator()(Bool& bool_expr)
{
	EmitByte(bool_expr.m_token.m_lexeme == "true" ? OpCode::TRUE : OpCode::FALSE, bool_expr.m_token.m_line);
}

void CodeGenerator::operator()(Number& number)
{
	EmitConstant(std::stod(number.m_token.m_lexeme), number.m_token.m_line);
}

void CodeGenerator::operator()(Nil& nil)
{
	EmitByte(OpCode::NIL, nil.m_token.m_line);
}

void CodeGenerator::operator()(Lambda& lambda)
{
	return;
}

void CodeGenerator::operator()(Array& array)
{
	return;
}

void CodeGenerator::operator()(ArrayGet& array_get)
{
	return;
}

void CodeGenerator::operator()(ArraySet& array_set)
{
	return;
}

void CodeGenerator::operator()(Ternary& ternary)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_condition);
	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, ternary.m_colon.m_line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_true_branch);
	int jump = EmitJump(OpCode::JUMP, ternary.m_question.m_line);
	PatchJump(jump_if_false);
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_else_branch);
	PatchJump(jump);
}