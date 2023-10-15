#pragma warning(disable: 4100)

#include "CodeGenerator.h"

Result::CodeGeneratorResult CodeGenerator::GenerateCode(ProgramTree&& program_tree)
{
	SetUpGlobalVariables();

	for (std::unique_ptr<Statement>& statement : program_tree)
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
	}

	if (!m_errors.empty())
	{
		return std::unexpected(std::move(m_errors));
	}

#ifdef DEBUG
	return Result::GeneratedBytecodeBundle(std::move(m_module), std::move(m_traceable_constants), std::move(m_sub_modules), std::move(m_static_data), std::move(m_global_table));
#else
	return Result::GeneratedBytecodeBundle(std::move(m_module), std::move(m_traceable_constants), std::move(m_static_data), std::move(m_global_table));
#endif
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
	EmitByte(OpCode::POP, simple.m_semicolon.m_line);
}

void CodeGenerator::operator()(Let& let)
{
	bool is_global = !let.m_local_index.has_value();
	std::optional<int> index = std::nullopt;
	if (is_global)
	{
		std::string variable_name = let.m_name.m_lexeme;
		index.emplace(m_global_table.AddGlobalVariable(std::move(variable_name)));
		m_global_variables[let.m_name.m_lexeme] = index.value();
	}

	if (let.m_value.has_value())
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *let.m_value.value());
	}
	else
	{
		EmitByte(OpCode::NIL, let.m_name.m_line);
	}

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

	std::visit([this](auto&& arg) {(*this)(arg); }, *return_stmt.m_value);

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

void CodeGenerator::operator()(Binary& binary)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_left);
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_right);

	switch (binary.m_op.m_token_type)
	{
	case Token::Type::SINGLE_PLUS:
		EmitByte(OpCode::ADD, binary.m_op.m_line);
		break;
	case Token::Type::DOUBLE_PLUS:
		EmitByte(OpCode::CONCAT, binary.m_op.m_line);
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

void CodeGenerator::operator()(Logical& logical)
{
	int line = logical.m_op.m_line;
	if (logical.m_op.m_token_type == Token::Type::DOUBLE_BAR)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_left);
		int jump_if_true = EmitJump(OpCode::JUMP_IF_TRUE, line);
		EmitByte(OpCode::POP, line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_right);
		PatchJump(jump_if_true, line);
	}
	else if (logical.m_op.m_token_type == Token::Type::DOUBLE_AMPERSAND)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_left);
		int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
		EmitByte(OpCode::POP, line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *logical.m_right);
		PatchJump(jump_if_false, line);
	}
	else
	{
		m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Unrecognized logical operator.", line));
	}
}

void CodeGenerator::operator()(Group& group)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *group.m_expr_in);
}

void CodeGenerator::operator()(Unary& unary)
{
	std::visit([this](auto&& arg) {(*this)(arg); }, *unary.m_right);

	switch (unary.m_op.m_token_type)
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
		m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Too many function arguments (max 255).", call.m_paren.m_line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *call.m_callee);

	for (std::unique_ptr<Expression>& arg : call.m_arguments)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *arg);
	}

	EmitByte(OpCode::CALL, call.m_paren.m_line);
	EmitByte(static_cast<OpCode>(arity), call.m_paren.m_line);
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
	std::visit([&variable, this](auto&& arg) 
		{ 
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, VariableSemantic::Local>)
			{
				EmitVariable(arg.m_index, OpCode::GET_LOCAL, variable.m_name.m_line);
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Global>)
			{
				EmitVariable(m_global_variables[variable.m_name.m_lexeme], OpCode::GET_GLOBAL, variable.m_name.m_line);
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Cell>)
			{
				EmitVariable(arg.m_index, OpCode::GET_CELL, variable.m_name.m_line);
			}
			else
			{
				m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Bad Variable Expression.", variable.m_name.m_line));
				return;
			}
		}, variable.m_semantic);
}

void CodeGenerator::operator()(Assign& assign)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *assign.m_value);

	std::visit([&assign, this](auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, VariableSemantic::Local>)
			{
				EmitVariable(arg.m_index, OpCode::SET_LOCAL, assign.m_name.m_line);
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Global>)
			{
				EmitVariable(m_global_variables[assign.m_name.m_lexeme], OpCode::SET_GLOBAL, assign.m_name.m_line);
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Cell>)
			{
				EmitVariable(arg.m_index, OpCode::SET_CELL, assign.m_name.m_line);
			}
			else
			{
				m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Bad Assign Expression.", assign.m_name.m_line));
				return;
			}
		}, assign.m_semantic);
}

void CodeGenerator::operator()(String& string)
{
	EmitConstant(Object::AllocateObject(std::move(string.m_token.m_lexeme)), string.m_token.m_line);
}

void CodeGenerator::operator()(Bool& bool_expr)
{
	EmitByte(bool_expr.m_token.m_lexeme == "true" ? OpCode::TRUE : OpCode::FALSE, bool_expr.m_token.m_line);
}

void CodeGenerator::operator()(Number& number)
{
	EmitConstant(std::stod(number.m_token.m_lexeme), number.m_token.m_line);
}

void CodeGenerator::operator()(Unit& nil)
{
	EmitByte(OpCode::NIL, nil.m_token.m_line);
}

void CodeGenerator::operator()(Function& function)
{
	int line = function.m_function_keyword.m_line;
	int arity = static_cast<int>(function.m_params.size());
	if (arity > UINT8_MAX)
	{
		m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Too many function arguments (max 255).", line));
		return;
	}

	BytecodeStream prev_module = std::move(m_module);
	m_module = BytecodeStream();

	std::visit([this](auto&& arg) {(*this)(arg); }, *function.m_body);

	BytecodeStream bytecode(std::move(m_module));
	Object* function_ptr = Object::AllocateObject(Object::DefinedFunction(std::move(bytecode), Object::Closure(), std::move(arity)));
#ifdef DEBUG
	m_sub_modules.emplace_back(&function_ptr->GetDefinedFunction());
#endif
	m_module = std::move(prev_module);
	EmitConstant(function_ptr, line);
	EmitByte(OpCode::CREATE_CLOSURE, line);
}

void CodeGenerator::operator()(Array& array)
{
	int line = array.m_op.m_line;

	if (array.m_allocated_size.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *array.m_allocated_size.value());
		EmitByte(OpCode::RESERVE_ARRAY, line);
	}
	else
	{
		int length = static_cast<int>(array.m_elems.size());

		if (length > THREE_BYTE_MAX)
		{
			m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Too many array elements (max 16777215).", line));
			return;
		}

		for (std::unique_ptr<Expression>& elem : array.m_elems)
		{
			std::visit([this](auto&& arg) {(*this)(arg); }, *elem);
		}
		EmitByte(OpCode::CREATE_ARRAY, line);
		EmitThreeBytes(length, length >> 8, length >> 16, line);
	}
}

void CodeGenerator::operator()(ArrayGet& array_get)
{
	int line = array_get.m_op.m_line;

	if (array_get.m_indices.size() > UINT8_MAX)
	{
		m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Too many array indices (max 255).", line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_get.m_arr_var);

	for (std::unique_ptr<Expression>& index : array_get.m_indices)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *index);
	}

	EmitByte(OpCode::GET_ARRAY, line);
	EmitByte(static_cast<OpCode>(array_get.m_indices.size()), line);
}

void CodeGenerator::operator()(ArraySet& array_set)
{
	int line = array_set.m_op.m_line;

	if (array_set.m_indices.size() > UINT8_MAX)
	{
		m_errors.emplace_back(CompilerError::GenerateCodeGeneratorError("Too many array indices (max 255).", line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_set.m_arr_var);

	for (std::unique_ptr<Expression>& index : array_set.m_indices)
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *index);
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_set.m_value);

	EmitByte(OpCode::SET_ARRAY, line);
	EmitByte(static_cast<OpCode>(array_set.m_indices.size()), line);
}

void CodeGenerator::operator()(Ternary& ternary)
{
	int line = ternary.m_colon.m_line;
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_condition);
	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
	EmitByte(OpCode::POP, line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_true_branch);
	int jump = EmitJump(OpCode::JUMP, line);
	PatchJump(jump_if_false, line);
	EmitByte(OpCode::POP, line);
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_else_branch);
	PatchJump(jump, line);
}

void CodeGenerator::SetUpGlobalVariables()
{
	std::string variable_name = "Print";
	int index = m_global_table.AddGlobalVariable(std::move(variable_name));
	m_global_variables["Print"] = index;
}
