#pragma warning(disable: 4100)

#include "CodeGenerator.h"

CodeGenerator::CodeGeneratorResult CodeGenerator::GenerateCode(ProgramTree&& program_tree)
{
	SetUpGlobalVariables();

	for (std::unique_ptr<Statement>& statement : program_tree)
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
	}

	m_module.AddByteCode(OpCode::RETURN, -1);

	return CodeGenerator::CodeGeneratorResult(std::move(m_module), std::move(m_sub_modules), m_error);
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

void CodeGenerator::operator()(Let& let)
{
	bool is_global = !let.m_local_index.has_value();
	std::optional<int> index = std::nullopt;
	if (is_global)
	{
		std::string variable_name = let.m_name.m_lexeme;
		index.emplace(m_module.AddGlobalVariable(std::move(variable_name)));
		m_global_variables[let.m_name.m_lexeme] = index.value();
	}

	std::visit([this](auto&& arg) { (*this)(arg); }, *let.m_value);

	if (is_global)
	{
		EmitVariable(index.value(), OpCode::DEFINE_GLOBAL, let.m_name.m_line);
	}
}

void CodeGenerator::operator()(If& if_stmt)
{
	int line = if_stmt.m_if_keyword.m_line;
	std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_condition);
	int true_jump = EmitJump(OpCode::JUMP_IF_FALSE, line);
	EmitByte(OpCode::POP, line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_true_branch);

	int else_jump = EmitJump(OpCode::JUMP, line);
	PatchJump(true_jump, line);
	EmitByte(OpCode::POP, line);

	if (if_stmt.m_else_branch.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_else_branch.value());
	}

	PatchJump(else_jump, line);
}

void CodeGenerator::operator()(While& while_stmt)
{
	int loop_start = m_module.GetByteCodeSize();

	std::visit([this](auto&& arg) {(*this)(arg); }, *while_stmt.m_condition);

	int line = while_stmt.m_while_keyword.m_line;
	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
	EmitByte(OpCode::POP, line);

	std::visit([this](auto&& arg) {(*this)(arg); }, *while_stmt.m_body);

	EmitLoop(loop_start, line);

	PatchJump(jump_if_false, line);
	EmitByte(OpCode::POP, line);
}

void CodeGenerator::operator()(For& for_stmt)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition_intializer);

	int loop_start = m_module.GetByteCodeSize();

	int line = for_stmt.m_for_keyword.m_line;
	if (for_stmt.m_condition.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition.value());
	}
	else
	{
		EmitByte(OpCode::TRUE, line);
	}

	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
	EmitByte(OpCode::POP, line);

	std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_body);

	if (for_stmt.m_condition_incrementer.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition_incrementer.value());
	}

	EmitLoop(loop_start, line);

	PatchJump(jump_if_false, line);
	EmitByte(OpCode::POP, line);
	while (for_stmt.m_control_block_local_count > 0)
	{
		int count_to_pop = std::min(for_stmt.m_control_block_local_count, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_MULTIPLE, line);
		EmitByte(static_cast<OpCode>(count_to_pop), line);
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

void CodeGenerator::operator()(Return& return_stmt)
{
	int line = return_stmt.m_keyword.m_line;

	if (return_stmt.m_value.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *return_stmt.m_value.value());
	}
	else
	{
		EmitByte(OpCode::NIL, line);
	}

	EmitByte(OpCode::RETURN, line);
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
	int line = logical.m_op.m_line;
	if (logical.m_op.m_type == Token::Type::DOUBLE_BAR)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_left);
		int jump_if_true = EmitJump(OpCode::JUMP_IF_TRUE, line);
		EmitByte(OpCode::POP, line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_right);
		PatchJump(jump_if_true, line);
	}
	else if (logical.m_op.m_type == Token::Type::DOUBLE_AMPERSAND)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_left);
		int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
		EmitByte(OpCode::POP, line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_right);
		PatchJump(jump_if_false, line);
	}
	else
	{
		CompilerError::PrintError("Unrecognized logical operator.", line);
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
	int arity = static_cast<int>(call.m_arguments.size());
	if (arity > UINT8_MAX)
	{
		CompilerError::PrintError("Too many function arguments (max 255).", call.m_paren.m_line);
		m_error = true;
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *call.m_callee);

	for (std::unique_ptr<Expression>& arg : call.m_arguments)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *arg);
	}

	EmitByte(OpCode::CALL, call.m_paren.m_line);
	EmitByte(static_cast<OpCode>(arity), call.m_paren.m_line);
	EmitByte(OpCode::POP, call.m_paren.m_line);
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
	EmitConstant(new Object(std::move(string.m_token.m_lexeme)), string.m_token.m_line);
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
	int line = lambda.m_backslash.m_line;
	int arity = static_cast<int>(lambda.m_params.size());
	if (arity > UINT8_MAX)
	{
		CompilerError::PrintError("Too many function arguments (max 255).", lambda.m_backslash.m_line);
		m_error = true;
		return;
	}

	ExecutableModule prev_module = std::move(m_module);
	m_module = ExecutableModule();

	std::visit([this](auto&& arg) {(*this)(arg); }, *lambda.m_body);

	m_module.AddByteCode(OpCode::RETURN, line);

	std::shared_ptr<ExecutableModule> sub_module = std::make_shared<ExecutableModule>(std::move(m_module));
	m_sub_modules.emplace_back(sub_module);
	Object* defined_function_object = new Object(Object::DefinedFunction(std::move(sub_module), std::move(arity)));

	m_module = std::move(prev_module);
	EmitConstant(std::move(defined_function_object), line);
}

void CodeGenerator::operator()(Array& array)
{
	int line = array.m_op.m_line;

	if (array.m_allocated_size.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *array.m_allocated_size.value());
		EmitByte(OpCode::ARRAY_RESERVE, line);
	}
	else
	{
		int length = static_cast<int>(array.m_elems.size());

		if (length > THREE_BYTE_MAX)
		{
			CompilerError::PrintError("Too many array elements (max 16777215).", line);
			m_error = true;
			return;
		}

		for (std::unique_ptr<Expression>& elem : array.m_elems)
		{
			std::visit([this](auto&& arg) {(*this)(arg); }, *elem);
		}
		EmitByte(OpCode::ARRAY_CREATE, line);
		EmitThreeBytes(length, length >> 8, length >> 16, line);
	}
}

void CodeGenerator::operator()(ArrayGet& array_get)
{
	int line = array_get.m_op.m_line;

	if (array_get.m_indices.size() > UINT8_MAX)
	{
		CompilerError::PrintError("Too many array indices (max 255).", line);
		m_error = true;
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_get.m_arr_var);

	for (std::unique_ptr<Expression>& index : array_get.m_indices)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *index);
	}

	EmitByte(OpCode::ARRAY_GET, line);
	EmitByte(static_cast<OpCode>(array_get.m_indices.size()), line);
}

void CodeGenerator::operator()(ArraySet& array_set)
{
	int line = array_set.m_op.m_line;

	if (array_set.m_indices.size() > UINT8_MAX)
	{
		CompilerError::PrintError("Too many array indices (max 255).", line);
		m_error = true;
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_set.m_arr_var);

	for (std::unique_ptr<Expression>& index : array_set.m_indices)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *index);
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_set.m_value);

	EmitByte(OpCode::ARRAY_SET, line);
	EmitByte(static_cast<OpCode>(array_set.m_indices.size()), line);
}

void CodeGenerator::operator()(Ternary& ternary)
{
	int line = ternary.m_colon.m_line;
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_condition);
	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_true_branch);
	int jump = EmitJump(OpCode::JUMP, line);
	PatchJump(jump_if_false, line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_else_branch);
	PatchJump(jump, line);
}

void CodeGenerator::SetUpGlobalVariables()
{
	std::string variable_name = "Print";
	int index = m_module.AddGlobalVariable(std::move(variable_name));
	m_global_variables["Print"] = index;
}
