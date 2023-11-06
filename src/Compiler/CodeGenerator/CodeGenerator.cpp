#include "CodeGenerator.h"

MidoriResult::CodeGeneratorResult CodeGenerator::GenerateCode(ProgramTree&& program_tree)
{
	SetUpNativeFunctions();

	std::for_each(program_tree.begin(), program_tree.end(), [this](std::unique_ptr<Statement>& statement)
		{
			std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
		});

	if (!m_main_module_ctx.has_value())
	{
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Program entry (\"main\") not found.", 0));
	}

	if (!m_errors.empty())
	{
		return std::unexpected<std::vector<std::string>>(std::move(m_errors));
	}

	// invoke the program entry (main)
	const CodeGenerator::MainModuleContext& main_module_ctx = m_main_module_ctx.value();
	m_current_module_index = main_module_ctx.m_main_module_index;
	EmitVariable(main_module_ctx.m_main_module_global_table_index, OpCode::GET_GLOBAL, main_module_ctx.m_main_module_line);
	EmitByte(OpCode::CALL, main_module_ctx.m_main_module_line);
	EmitByte(static_cast<OpCode>(0), main_module_ctx.m_main_module_line);

#ifdef DEBUG
	return MidoriResult::ExecutableModule(std::move(m_modules), std::move(m_module_names), std::move(m_traceable_constants), std::move(m_static_data), std::move(m_global_table));
#else
	return MidoriResult::ExecutableModule(std::move(m_modules), std::move(m_traceable_constants), std::move(m_static_data), std::move(m_global_table));
#endif
}

void CodeGenerator::operator()(Block& block)
{
	std::for_each(block.m_stmts.begin(), block.m_stmts.end(), [this](std::unique_ptr<Statement>& statement)
		{
			std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
		});
	if (m_modules[m_current_module_index].ReadByteCode(m_modules[m_current_module_index].GetByteCodeSize() - 1) == OpCode::RETURN)
	{
		return;
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

void CodeGenerator::operator()(Define& def)
{
	int line = def.m_name.m_line;
	bool is_global = !def.m_local_index.has_value();
	std::optional<int> index = std::nullopt;
	if (is_global)
	{
		std::string variable_name = def.m_name.m_lexeme;
		index.emplace(m_global_table.AddGlobalVariable(std::move(variable_name)));
		m_global_variables[def.m_name.m_lexeme] = index.value();
	}

	std::visit([this](auto&& arg) { (*this)(arg); }, *def.m_value);

	if (is_global)
	{
		EmitVariable(index.value(), OpCode::DEFINE_GLOBAL, line);
		if (def.m_name.m_lexeme == "main")
		{
			if (m_main_module_ctx.has_value())
			{
				m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Cannot re-define program entry (\"main\").", line));
			}
			else
			{
				constexpr int main_module_arity = 0;
				m_main_module_ctx.emplace(m_current_module_index, index.value(), main_module_arity, line);
			}
		}
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
	int loop_start = m_modules[m_current_module_index].GetByteCodeSize();
	BeginLoop(loop_start);

	std::visit([this](auto&& arg) {(*this)(arg); }, *while_stmt.m_condition);

	int line = while_stmt.m_while_keyword.m_line;
	int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
	EmitByte(OpCode::POP, line);

	std::visit([this](auto&& arg) {(*this)(arg); }, *while_stmt.m_body);

	EmitLoop(loop_start, line);
	PatchJump(jump_if_false, line);

	EmitByte(OpCode::POP, line);

	EndLoop(line);
}

void CodeGenerator::operator()(For& for_stmt)
{
	if (for_stmt.m_condition_intializer.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition_intializer.value());
	}

	int loop_start = m_modules[m_current_module_index].GetByteCodeSize();
	int line = for_stmt.m_for_keyword.m_line;

	int exit_jump = -1;
	if (for_stmt.m_condition.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition.value());
		exit_jump = EmitJump(OpCode::JUMP_IF_FALSE, line);
		EmitByte(OpCode::POP, line);
	}
	if (for_stmt.m_condition_incrementer.has_value())
	{
		int body_jump = EmitJump(OpCode::JUMP, line);
		int incrementer_start = m_modules[m_current_module_index].GetByteCodeSize();
		std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_condition_incrementer.value());

		EmitLoop(loop_start, line);
		loop_start = incrementer_start;
		PatchJump(body_jump, line);
	}

	BeginLoop(loop_start);
	std::visit([this](auto&& arg) {(*this)(arg); }, *for_stmt.m_body);

	EmitLoop(loop_start, line);
	if (exit_jump != -1)
	{
		PatchJump(exit_jump, line);
		EmitByte(OpCode::POP, line);
	}

	while (for_stmt.m_control_block_local_count > 0)
	{
		int count_to_pop = std::min(for_stmt.m_control_block_local_count, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_MULTIPLE, line);
		EmitByte(static_cast<OpCode>(count_to_pop), line);
		for_stmt.m_control_block_local_count -= count_to_pop;
	}
	EndLoop(line);
}

void CodeGenerator::operator()(Break& break_stmt)
{
	int line = break_stmt.m_keyword.m_line;

	while (break_stmt.m_number_to_pop > 0)
	{
		int count_to_pop = std::min(break_stmt.m_number_to_pop, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_MULTIPLE, line);
		EmitByte(static_cast<OpCode>(count_to_pop), line);
		break_stmt.m_number_to_pop -= count_to_pop;
	}

	m_loop_contexts.top().m_break_positions.emplace_back(EmitJump(OpCode::JUMP, line));
}

void CodeGenerator::operator()(Continue& continue_stmt)
{
	int line = continue_stmt.m_keyword.m_line;

	while (continue_stmt.m_number_to_pop > 0)
	{
		int count_to_pop = std::min(continue_stmt.m_number_to_pop, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_MULTIPLE, line);
		EmitByte(static_cast<OpCode>(count_to_pop), line);
		continue_stmt.m_number_to_pop -= count_to_pop;
	}

	EmitLoop(m_loop_contexts.top().m_loop_start, line);
}

void CodeGenerator::operator()(Return& return_stmt)
{
	int line = return_stmt.m_keyword.m_line;

	std::visit([this](auto&& arg) {(*this)(arg); }, *return_stmt.m_value);

	EmitByte(OpCode::RETURN, line);
}

void CodeGenerator::operator()(Import & import)
{
	return;
}

void CodeGenerator::operator()(Namespace& namespace_stmt)
{
	return;
}

void CodeGenerator::operator()(Binary& binary)
{
	int line = binary.m_op.m_line;
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_left);
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_right);

	switch (binary.m_op.m_token_type)
	{
	case Token::Name::SINGLE_PLUS:
		EmitByte(OpCode::ADD, line);
		break;
	case Token::Name::DOUBLE_PLUS:
		EmitByte(OpCode::CONCAT, line);
		break;
	case Token::Name::MINUS:
		EmitByte(OpCode::SUBTRACT, line);
		break;
	case Token::Name::STAR:
		EmitByte(OpCode::MULTIPLY, line);
		break;
	case Token::Name::SLASH:
		EmitByte(OpCode::DIVIDE, line);
		break;
	case Token::Name::PERCENT:
		EmitByte(OpCode::MODULO, line);
		break;
	case Token::Name::LEFT_SHIFT:
		EmitByte(OpCode::LEFT_SHIFT, line);
		break;
	case Token::Name::RIGHT_SHIFT:
		EmitByte(OpCode::RIGHT_SHIFT, line);
		break;
	case Token::Name::LESS:
		EmitByte(OpCode::LESS, line);
		break;
	case Token::Name::LESS_EQUAL:
		EmitByte(OpCode::LESS_EQUAL, line);
		break;
	case Token::Name::GREATER:
		EmitByte(OpCode::GREATER, line);
		break;
	case Token::Name::GREATER_EQUAL:
		EmitByte(OpCode::GREATER_EQUAL, line);
		break;
	case Token::Name::BANG_EQUAL:
		EmitByte(OpCode::NOT_EQUAL, line);
		break;
	case Token::Name::DOUBLE_EQUAL:
		EmitByte(OpCode::EQUAL, line);
		break;
	case Token::Name::SINGLE_AMPERSAND:
		EmitByte(OpCode::BITWISE_AND, line);
		break;
	case Token::Name::SINGLE_BAR:
		EmitByte(OpCode::BITWISE_OR, line);
		break;
	case Token::Name::CARET:
		EmitByte(OpCode::BITWISE_XOR, line);
		break;
	case Token::Name::DOUBLE_BAR:
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *binary.m_left);
		int jump_if_true = EmitJump(OpCode::JUMP_IF_TRUE, line);
		EmitByte(OpCode::POP, line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *binary.m_right);
		PatchJump(jump_if_true, line);
		break;
	}
	case Token::Name::DOUBLE_AMPERSAND:
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *binary.m_left);
		int jump_if_false = EmitJump(OpCode::JUMP_IF_FALSE, line);
		EmitByte(OpCode::POP, line);
		std::visit([this](auto&& arg) {(*this)(arg); }, *binary.m_right);
		PatchJump(jump_if_false, line);
		break;
	}
	default:
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Unrecognized binary operator.", line));
		break; // Unreachable
	}
	return;
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
	case Token::Name::MINUS:
		EmitByte(OpCode::NEGATE, unary.m_op.m_line);
		break;
	case Token::Name::BANG:
		EmitByte(OpCode::NOT, unary.m_op.m_line);
		break;
	default:
		break; // Unreachable
	}

	return;
}

void CodeGenerator::operator()(Call& call)
{
	int line = call.m_paren.m_line;
	int arity = static_cast<int>(call.m_arguments.size());
	if (arity > UINT8_MAX)
	{
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many arguments (max 255).", call.m_paren.m_line));
		return;
	}

	std::for_each(call.m_arguments.begin(), call.m_arguments.end(), [&line, this](std::unique_ptr<Expression>& arg) { std::visit([this](auto&& arg) {(*this)(arg); }, *arg); });
	std::visit([this](auto&& arg) {(*this)(arg); }, *call.m_callee);

	EmitByte(OpCode::CALL, line);
	EmitByte(static_cast<OpCode>(arity), line);
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
				m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Bad Variable Expression.", variable.m_name.m_line));
				return;
			}
		}, variable.m_semantic);
}

void CodeGenerator::operator()(Bind& bind)
{
	std::visit([this](auto&& arg) { (*this)(arg); }, *bind.m_value);

	std::visit([&bind, this](auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, VariableSemantic::Local>)
			{
				EmitVariable(arg.m_index, OpCode::SET_LOCAL, bind.m_name.m_line);
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Global>)
			{
				EmitVariable(m_global_variables[bind.m_name.m_lexeme], OpCode::SET_GLOBAL, bind.m_name.m_line);
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Cell>)
			{
				EmitVariable(arg.m_index, OpCode::SET_CELL, bind.m_name.m_line);
			}
			else
			{
				m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Bad Bind Expression.", bind.m_name.m_line));
				return;
			}
		}, bind.m_semantic);
}

void CodeGenerator::operator()(String& string)
{
	EmitConstant(Traceable::AllocateObject(std::move(string.m_token.m_lexeme)), string.m_token.m_line);
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
	EmitByte(OpCode::UNIT, nil.m_token.m_line);
}

void CodeGenerator::operator()(Closure& closure)
{
	int line = closure.m_closure_keyword.m_line;
	int arity = static_cast<int>(closure.m_params.size());
	int captured_count = static_cast<int>(closure.m_captured_count);
	if (arity > UINT8_MAX)
	{
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many arguments (max 255).", line));
		return;
	}
	if (captured_count > UINT8_MAX)
	{
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many captured variables (max 255).", line));
		return;
	}

	int prev_index = m_current_module_index;
	m_modules.emplace_back(BytecodeStream());
	m_current_module_index = static_cast<int>(m_modules.size() - 1u);
	std::visit([this](auto&& arg) {(*this)(arg); }, *closure.m_body);

	int closure_module_index = m_current_module_index;
#ifdef DEBUG
	std::string closure_line = "Closure at line: " + std::to_string(line) + "(index: " + std::to_string(closure_module_index) + ")";
	m_module_names.emplace_back(std::move(closure_line));
#endif

	m_current_module_index = prev_index;

	EmitByte(OpCode::CREATE_CLOSURE, line);
	EmitThreeBytes(static_cast<int>(closure.m_captured_count), static_cast<int>(arity), static_cast<int>(closure_module_index), line);
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
			m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many array elements (max 16777215).", line));
			return;
		}

		std::for_each(array.m_elems.begin(), array.m_elems.end(), [this](std::unique_ptr<Expression>& elem)
			{
				std::visit([this](auto&& arg) {(*this)(arg); }, *elem);
			});
		EmitByte(OpCode::CREATE_ARRAY, line);
		EmitThreeBytes(length, length >> 8, length >> 16, line);
	}
}

void CodeGenerator::operator()(ArrayGet& array_get)
{
	int line = array_get.m_op.m_line;

	if (array_get.m_indices.size() > UINT8_MAX)
	{
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many array indices (max 255).", line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_get.m_arr_var);

	std::for_each(array_get.m_indices.begin(), array_get.m_indices.end(), [this](std::unique_ptr<Expression>& index)
		{
			std::visit([this](auto&& arg) {(*this)(arg); }, *index);
		});

	EmitByte(OpCode::GET_ARRAY, line);
	EmitByte(static_cast<OpCode>(array_get.m_indices.size()), line);
}

void CodeGenerator::operator()(ArraySet& array_set)
{
	int line = array_set.m_op.m_line;

	if (array_set.m_indices.size() > UINT8_MAX)
	{
		m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many array indices (max 255).", line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_set.m_arr_var);

	std::for_each(array_set.m_indices.begin(), array_set.m_indices.end(), [this](std::unique_ptr<Expression>& index)
		{
			std::visit([this](auto&& arg) {(*this)(arg); }, *index);
		});

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

void CodeGenerator::SetUpNativeFunctions()
{
	std::string variable_name = "PrintLine";
	int index = m_global_table.AddGlobalVariable(std::move(variable_name));
	m_global_variables["PrintLine"] = index;
}
