#include "CodeGenerator.h"

void CodeGenerator::EmitByte(OpCode byte, int line)
{ 
	m_procedures[m_current_procedure_index].AddByteCode(byte, line); 
}

void CodeGenerator::AddError(std::string&& error)
{
	m_errors.append(error);
	m_errors.push_back('\n');
}

void CodeGenerator::EmitTwoBytes(int byte1, int byte2, int line)
{
	EmitByte(static_cast<OpCode>(byte1), line);
	EmitByte(static_cast<OpCode>(byte2), line);
}

void CodeGenerator::EmitThreeBytes(int byte1, int byte2, int byte3, int line)
{
	EmitByte(static_cast<OpCode>(byte1), line);
	EmitByte(static_cast<OpCode>(byte2), line);
	EmitByte(static_cast<OpCode>(byte3), line);
}

void CodeGenerator::EmitConstant(MidoriValue&& value, int line)
{
	if (value.IsPointer())
	{
		MidoriTraceable* traceable = static_cast<MidoriTraceable*>(value.GetPointer());
		m_executable.AddConstantRoot(traceable);
		MidoriTraceable::s_static_bytes_allocated += traceable->GetSize();
	}

	int index = m_executable.AddConstant(std::move(value));

	if (index <= UINT8_MAX) // 1 byte
	{
		EmitByte(OpCode::CONSTANT, line);
		EmitByte(static_cast<OpCode>(index), line);
	}
	else if (index <= UINT16_MAX) // 2 bytes
	{
		EmitByte(OpCode::CONSTANT_LONG, line);
		EmitByte(static_cast<OpCode>(index & 0xff), line);
		EmitByte(static_cast<OpCode>((index >> 8) & 0xff), line);
	}
	else if (index <= ((UINT16_MAX << 8) | 0xffff)) // 3 bytes
	{
		EmitByte(OpCode::CONSTANT_LONG_LONG, line);
		EmitByte(static_cast<OpCode>(index & 0xff), line);
		EmitByte(static_cast<OpCode>((index >> 8) & 0xff), line);
		EmitByte(static_cast<OpCode>((index >> 16) & 0xff), line);
	}
	else
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Too many constants (max 16777215).", line));
	}
}

void CodeGenerator::EmitVariable(int variable_index, OpCode op, int line)
{
	if (variable_index <= UINT8_MAX)
	{
		EmitByte(op, line);
		EmitByte(static_cast<OpCode>(variable_index), line);
		return;
	}

	AddError(MidoriError::GenerateCodeGeneratorError("Too many variables (max 255).", line));
}

int CodeGenerator::EmitJump(OpCode op, int line)
{
	EmitByte(op, line);
	EmitByte(static_cast<OpCode>(0xff), line);
	EmitByte(static_cast<OpCode>(0xff), line);
	return m_procedures[m_current_procedure_index].GetByteCodeSize() - 2;
}

void CodeGenerator::PatchJump(int offset, int line)
{
	int jump = m_procedures[m_current_procedure_index].GetByteCodeSize() - offset - 2;
	if (jump > UINT16_MAX)
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Too much code to jump over (max 65535).", line));
		return;
	}

	m_procedures[m_current_procedure_index].SetByteCode(offset, static_cast<OpCode>(jump & 0xff));
	m_procedures[m_current_procedure_index].SetByteCode(offset + 1, static_cast<OpCode>((jump >> 8) & 0xff));
}

void CodeGenerator::EmitLoop(int loop_start, int line)
{
	EmitByte(OpCode::JUMP_BACK, line);

	int offset = m_procedures[m_current_procedure_index].GetByteCodeSize() - loop_start + 2;
	if (offset > UINT16_MAX)
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Loop body too large (max 65535).", line));
		return;
	}

	EmitByte(static_cast<OpCode>(offset & 0xff), line);
	EmitByte(static_cast<OpCode>((offset >> 8) & 0xff), line);
}

void CodeGenerator::BeginLoop(int loop_start) 
{ 
	m_loop_contexts.emplace(std::vector<int>(), loop_start); 
}

void CodeGenerator::EndLoop(int line)
{
	LoopContext loop = m_loop_contexts.top();
	m_loop_contexts.pop();
	std::for_each(loop.m_break_positions.begin(), loop.m_break_positions.end(), [line, this](int break_position) { PatchJump(break_position, line); });
}

MidoriResult::CodeGeneratorResult CodeGenerator::GenerateCode(MidoriProgramTree&& program_tree)
{
	std::for_each(program_tree.begin(), program_tree.end(), [this](std::unique_ptr<MidoriStatement>& statement)
		{
			std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
		});

	if (!m_main_module_ctx.has_value())
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Program entry (\"main\") not found.", 0));
	}

	if (!m_errors.empty())
	{
		return std::unexpected<std::string>(std::move(m_errors));
	}

	// invoke the program entry (main)
	const CodeGenerator::MainProcedureContext& main_module_ctx = m_main_module_ctx.value();
	m_current_procedure_index = main_module_ctx.m_main_procedure_index;
	EmitVariable(main_module_ctx.m_main_procedure_global_table_index, OpCode::GET_GLOBAL, main_module_ctx.m_main_procedure_line);
	EmitByte(OpCode::CALL_DEFINED, main_module_ctx.m_main_procedure_line);
	EmitByte(static_cast<OpCode>(0), main_module_ctx.m_main_procedure_line);
	EmitByte(OpCode::HALT, main_module_ctx.m_main_procedure_line);

#ifdef DEBUG
	m_executable.AttachProcedureNames(std::move(m_procedure_names));
#endif
	m_executable.AttachProcedures(std::move(m_procedures));

	MidoriTypeUtil::MidoriTypeUtilCleanUp();
	return m_executable;
}

void CodeGenerator::operator()(Block& block)
{
	std::for_each(block.m_stmts.begin(), block.m_stmts.end(), [this](std::unique_ptr<MidoriStatement>& statement)
		{
			std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
		});
	if (m_procedures[m_current_procedure_index].ReadByteCode(m_procedures[m_current_procedure_index].GetByteCodeSize() - 1) == OpCode::RETURN)
	{
		return;
	}

	while (block.m_local_count > 0)
	{
		int count_to_pop = std::min(block.m_local_count, static_cast<int>(UINT8_MAX));
		EmitByte(OpCode::POP_SCOPE, block.m_right_brace.m_line);
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
		index.emplace(m_executable.AddGlobalVariable(std::move(variable_name)));
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
				AddError(MidoriError::GenerateCodeGeneratorError("Cannot re-define program entry (\"main\").", line));
				return;
			}
			else
			{
				constexpr int main_module_arity = 0;
				m_main_module_ctx.emplace(m_current_procedure_index, index.value(), main_module_arity, line);
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

	if (if_stmt.m_else_branch.has_value())
	{
		std::visit([this](auto&& arg) {(*this)(arg); }, *if_stmt.m_else_branch.value());
	}

	PatchJump(else_jump, line);
}

void CodeGenerator::operator()(While& while_stmt)
{
	int loop_start = m_procedures[m_current_procedure_index].GetByteCodeSize();
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

	int loop_start = m_procedures[m_current_procedure_index].GetByteCodeSize();
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
		int incrementer_start = m_procedures[m_current_procedure_index].GetByteCodeSize();
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
		EmitByte(OpCode::POP_SCOPE, line);
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
		EmitByte(OpCode::POP_SCOPE, line);
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
		EmitByte(OpCode::POP_SCOPE, line);
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

void CodeGenerator::operator()(Foreign& foreign)
{
	int line = foreign.m_function_name.m_line;
	bool is_global = !foreign.m_local_index.has_value();
	std::optional<int> index = std::nullopt;
	if (is_global)
	{
		std::string foreign_function_name = foreign.m_function_name.m_lexeme;
		index.emplace(m_executable.AddGlobalVariable(std::move(foreign_function_name)));
		m_global_variables[foreign.m_function_name.m_lexeme] = index.value();
	}

	EmitConstant(MidoriTraceable::AllocateTraceable(std::move(foreign.m_function_name.m_lexeme)), line);

	if (is_global)
	{
		EmitVariable(index.value(), OpCode::DEFINE_GLOBAL, line);
	}
}

void CodeGenerator::operator()(Struct&)
{
	return;
}

void CodeGenerator::operator()(As& as)
{
	int line = as.m_as_keyword.m_line;

	std::visit([this](auto&& arg) {(*this)(arg); }, *as.m_expr);

	const MidoriType* target_type = as.m_target_type;

	if (MidoriTypeUtil::IsBoolType(target_type))
	{
		EmitByte(OpCode::CAST_TO_BOOL, line);
	}
	else if (MidoriTypeUtil::IsFractionType(target_type))
	{
		EmitByte(OpCode::CAST_TO_FRACTION, line);
	}
	else if (MidoriTypeUtil::IsIntegerType(target_type))
	{
		EmitByte(OpCode::CAST_TO_INTEGER, line);
	}
	else if (MidoriTypeUtil::IsUnitType(target_type))
	{
		EmitByte(OpCode::CAST_TO_UNIT, line);
	}
	else if (MidoriTypeUtil::IsTextType(target_type), line)
	{
		EmitByte(OpCode::CAST_TO_TEXT, line);
	}
	else
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Unsupported type casting instruction.", line));
	}
}

void CodeGenerator::operator()(Binary& binary)
{
	int line = binary.m_op.m_line;
	const MidoriType* expr_type = binary.m_type;
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_left);
	std::visit([this](auto&& arg) { (*this)(arg); }, *binary.m_right);

	switch (binary.m_op.m_token_name)
	{
	case Token::Name::SINGLE_PLUS:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::ADD_FRACTION, line) : EmitByte(OpCode::ADD_INTEGER, line);
		break;
	case Token::Name::DOUBLE_PLUS:
		MidoriTypeUtil::IsTextType(expr_type) ? EmitByte(OpCode::CONCAT_TEXT, line) : EmitByte(OpCode::CONCAT_ARRAY, line);
		break;
	case Token::Name::MINUS:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::SUBTRACT_FRACTION, line) : EmitByte(OpCode::SUBTRACT_INTEGER, line);
		break;
	case Token::Name::STAR:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::MULTIPLY_FRACTION, line) : EmitByte(OpCode::MULTIPLY_INTEGER, line);
		break;
	case Token::Name::SLASH:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::DIVIDE_FRACTION, line) : EmitByte(OpCode::DIVIDE_INTEGER, line);
		break;
	case Token::Name::PERCENT:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::MODULO_FRACTION, line) : EmitByte(OpCode::MODULO_INTEGER, line);
		break;
	case Token::Name::LEFT_SHIFT:
		EmitByte(OpCode::LEFT_SHIFT, line);
		break;
	case Token::Name::RIGHT_SHIFT:
		EmitByte(OpCode::RIGHT_SHIFT, line);
		break;
	case Token::Name::LEFT_ANGLE:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::LESS_FRACTION, line) : EmitByte(OpCode::LESS_INTEGER, line);
		break;
	case Token::Name::LESS_EQUAL:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::LESS_EQUAL_FRACTION, line) : EmitByte(OpCode::LESS_EQUAL_INTEGER, line);
		break;
	case Token::Name::RIGHT_ANGLE:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::GREATER_FRACTION, line) : EmitByte(OpCode::GREATER_INTEGER, line);
		break;
	case Token::Name::GREATER_EQUAL:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::GREATER_EQUAL_FRACTION, line) : EmitByte(OpCode::GREATER_EQUAL_INTEGER, line);
		break;
	case Token::Name::BANG_EQUAL:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::NOT_EQUAL_FRACTION, line) : EmitByte(OpCode::NOT_EQUAL_INTEGER, line);
		break;
	case Token::Name::DOUBLE_EQUAL:
		MidoriTypeUtil::IsFractionType(expr_type) ? EmitByte(OpCode::EQUAL_FRACTION, line) : EmitByte(OpCode::EQUAL_INTEGER, line);
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
		AddError(MidoriError::GenerateCodeGeneratorError("Unrecognized binary operator.", line));
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

	switch (unary.m_op.m_token_name)
	{
	case Token::Name::MINUS:
		MidoriTypeUtil::IsFractionType(unary.m_type) ? EmitByte(OpCode::NEGATE_FRACTION, unary.m_op.m_line) : EmitByte(OpCode::NEGATE_INTEGER, unary.m_op.m_line);
		break;
	case Token::Name::BANG:
		EmitByte(OpCode::NOT, unary.m_op.m_line);
		break;
	case Token::Name::TILDE:
		EmitByte(OpCode::BITWISE_NOT, unary.m_op.m_line);
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
		AddError(MidoriError::GenerateCodeGeneratorError("Too many arguments (max 255).", call.m_paren.m_line));
		return;
	}

	std::for_each(call.m_arguments.begin(), call.m_arguments.end(), [this](std::unique_ptr<MidoriExpression>& arg) { std::visit([this](auto&& arg) {(*this)(arg); }, *arg); });
	std::visit([this](auto&& arg) {(*this)(arg); }, *call.m_callee);

	if (call.m_is_foreign)
	{
		EmitByte(OpCode::CALL_FOREIGN, line);
	}
	else
	{
		EmitByte(OpCode::CALL_DEFINED, line);
	}

	EmitByte(static_cast<OpCode>(arity), line);
}

void CodeGenerator::operator()(Get& get)
{
	int line = get.m_member_name.m_line;

	std::visit([this](auto&& arg) {(*this)(arg); }, *get.m_struct);
	EmitByte(OpCode::GET_MEMBER, line);
	EmitByte(static_cast<OpCode>(get.m_index), line);
}

void CodeGenerator::operator()(Set& set)
{
	int line = set.m_member_name.m_line;

	std::visit([this](auto&& arg) {(*this)(arg); }, *set.m_struct);
	std::visit([this](auto&& arg) {(*this)(arg); }, *set.m_value);
	EmitByte(OpCode::SET_MEMBER, line);
	EmitByte(static_cast<OpCode>(set.m_index), line);
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
				AddError(MidoriError::GenerateCodeGeneratorError("Bad Variable MidoriExpression.", variable.m_name.m_line));
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
				AddError(MidoriError::GenerateCodeGeneratorError("Bad Bind MidoriExpression.", bind.m_name.m_line));
				return;
			}
		}, bind.m_semantic);
}

void CodeGenerator::operator()(TextLiteral& text)
{
	EmitConstant(MidoriTraceable::AllocateTraceable(std::move(text.m_token.m_lexeme)), text.m_token.m_line);
}

void CodeGenerator::operator()(BoolLiteral& bool_expr)
{
	EmitByte(bool_expr.m_token.m_lexeme == "true" ? OpCode::OP_TRUE : OpCode::OP_FALSE, bool_expr.m_token.m_line);
}

void CodeGenerator::operator()(FractionLiteral& fraction)
{
	EmitConstant(std::stod(fraction.m_token.m_lexeme), fraction.m_token.m_line);
}

void CodeGenerator::operator()(IntegerLiteral& integer)
{
	EmitConstant(std::stoll(integer.m_token.m_lexeme), integer.m_token.m_line);
}

void CodeGenerator::operator()(UnitLiteral& nil)
{
	EmitByte(OpCode::OP_UNIT, nil.m_token.m_line);
}

void CodeGenerator::operator()(Closure& closure)
{
	int line = closure.m_closure_keyword.m_line;
	int arity = static_cast<int>(closure.m_params.size());
	int captured_count = static_cast<int>(closure.m_captured_count);
	if (arity > UINT8_MAX)
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Too many arguments (max 255).", line));
		return;
	}
	if (captured_count > UINT8_MAX)
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Too many captured variables (max 255).", line));
		return;
	}

	int prev_index = m_current_procedure_index;
	m_current_procedure_index = static_cast<int>(m_procedures.size());
	m_procedures.emplace_back(BytecodeStream());
	std::visit([this](auto&& arg) {(*this)(arg); }, *closure.m_body);

	int closure_proc_index = m_current_procedure_index;
#ifdef DEBUG
	std::string closure_line = "Closure at line: " + std::to_string(line) + "(index: " + std::to_string(closure_proc_index) + ")";
	m_procedure_names.emplace_back(std::move(closure_line));
#endif

	m_current_procedure_index = prev_index;

	EmitByte(OpCode::CREATE_CLOSURE, line);
	EmitThreeBytes(static_cast<int>(closure.m_captured_count), static_cast<int>(arity), static_cast<int>(closure_proc_index), line);
}

void CodeGenerator::operator()(Construct& construct)
{
	int line = construct.m_type_name.m_line;
	OpCode size = static_cast<OpCode>(construct.m_params.size());

	EmitByte(OpCode::ALLOCATE_STRUCT, line);

	std::for_each(construct.m_params.begin(), construct.m_params.end(), [this](std::unique_ptr<MidoriExpression>& param)
		{
			std::visit([this](auto&& arg) {(*this)(arg); }, *param);
		});

	EmitByte(OpCode::CONSTRUCT_STRUCT, line);
	EmitByte(size, line);
}

void CodeGenerator::operator()(Array& array)
{
	int line = array.m_op.m_line;

	int length = static_cast<int>(array.m_elems.size());
	constexpr int three_byte_max = 16777215;

	if (length > three_byte_max)
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Too many array elements (max 16777215).", line));
		return;
	}

	std::for_each(array.m_elems.begin(), array.m_elems.end(), [this](std::unique_ptr<MidoriExpression>& elem)
		{
			std::visit([this](auto&& arg) {(*this)(arg); }, *elem);
		});
	EmitByte(OpCode::CREATE_ARRAY, line);
	EmitThreeBytes(length, length >> 8, length >> 16, line);
}

void CodeGenerator::operator()(ArrayGet& array_get)
{
	int line = array_get.m_op.m_line;

	if (array_get.m_indices.size() > UINT8_MAX)
	{
		AddError(MidoriError::GenerateCodeGeneratorError("Too many array indices (max 255).", line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_get.m_arr_var);

	std::for_each(array_get.m_indices.begin(), array_get.m_indices.end(), [this](std::unique_ptr<MidoriExpression>& index)
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
		AddError(MidoriError::GenerateCodeGeneratorError("Too many array indices (max 255).", line));
		return;
	}

	std::visit([this](auto&& arg) {(*this)(arg); }, *array_set.m_arr_var);

	std::for_each(array_set.m_indices.begin(), array_set.m_indices.end(), [this](std::unique_ptr<MidoriExpression>& index)
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
	std::visit([this](auto&& arg) {(*this)(arg); }, *ternary.m_else_branch);
	PatchJump(jump, line);
}