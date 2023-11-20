#include "TypeChecker.h"
#include "Common/Error/Error.h"

#include <algorithm>
#include <iterator>
#include <format>

MidoriResult::StaticAnalyzerResult TypeChecker::TypeCheck(ProgramTree& program_tree)
{
	TypeChecker::TypeEnvironment&& global_env = { {"PrintLine", std::make_shared<MidoriType>(FunctionType({std::make_shared<MidoriType>(IntegerType())}, std::make_shared<MidoriType>(UnitType()), true))} };
	m_name_type_table.emplace_back(global_env);
	std::for_each(program_tree.begin(), program_tree.end(), [this](std::unique_ptr<Statement>& statement)
		{
			std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
		});
	EndScope();

	if (m_errors.empty())
	{
		return std::nullopt;
	}
	else
	{
		return m_errors;
	}
}

void TypeChecker::operator()(Block& block)
{
	BeginScope();
	std::for_each(block.m_stmts.begin(), block.m_stmts.end(), [this](std::unique_ptr<Statement>& statement)
		{
			std::visit([this](auto&& arg) { (*this)(arg); }, *statement);
		});
	EndScope();
}

void TypeChecker::operator()(Simple& simple)
{
	MidoriResult::TypeResult result = std::visit([this](auto&& arg) { return (*this)(arg); }, *simple.m_expr);

	if (!result.has_value())
	{
		m_errors.emplace_back(result.error());
	}
}

void TypeChecker::operator()(Define& def)
{
	if (std::holds_alternative<Closure>(*def.m_value))
	{
		Closure& closure = std::get<Closure>(*def.m_value);
		m_name_type_table.back().emplace(def.m_name.m_lexeme, std::make_shared<MidoriType>(FunctionType(closure.m_param_types, closure.m_return_type, false)));
		const MidoriType& actual_type = *m_name_type_table.back()[def.m_name.m_lexeme];
		BeginScope();
		for (size_t i = 0u; i < closure.m_param_types.size(); i += 1u)
		{
			m_name_type_table.back().emplace(closure.m_params[i].m_lexeme, closure.m_param_types[i]);
		}

		if (def.m_annotated_type.has_value())
		{
			const MidoriType& annotated_type = *def.m_annotated_type.value();
			if (annotated_type != actual_type)
			{
				std::vector<const MidoriType*> expected_types = { &annotated_type };
				m_errors.emplace_back(MidoriError::GenerateTypeCheckerError("Define statement type error", def.m_name, expected_types, actual_type));
				return;
			}
		}

		MidoriResult::TypeResult closure_type_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *def.m_value);
		if (!closure_type_result.has_value())
		{
			m_errors.emplace_back(closure_type_result.error());
			return;
		}

		EndScope();
		return;
	}
	else
	{
		MidoriResult::TypeResult init_expr_type = std::visit([this](auto&& arg) { return (*this)(arg); }, *def.m_value);
		if (!init_expr_type.has_value())
		{
			m_errors.emplace_back(init_expr_type.error());
			return;
		}

		m_name_type_table.back().emplace(def.m_name.m_lexeme, init_expr_type.value());

		if (def.m_annotated_type.has_value())
		{
			const MidoriType& annotated_type = *def.m_annotated_type.value();
			const MidoriType& actual_type = *init_expr_type.value();
			if (*def.m_annotated_type.value() != *init_expr_type.value())
			{
				std::vector<const MidoriType*> expected_types = { &annotated_type };
				m_errors.emplace_back(MidoriError::GenerateTypeCheckerError("Define statement type error", def.m_name, expected_types, actual_type));
				return;
			}
		}
	}
}

void TypeChecker::operator()(If& if_stmt)
{
	MidoriResult::TypeResult condition_type = std::visit([this](auto&& arg) { return (*this)(arg); }, *if_stmt.m_condition);
	if (condition_type.has_value())
	{
		if (!MidoriTypeUtil::IsBoolType(*condition_type.value()))
		{
			const MidoriType& actual_type = *condition_type.value();
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[3] };
			m_errors.emplace_back(MidoriError::GenerateTypeCheckerError("If statement condition must be of type bool.", if_stmt.m_if_keyword, expected_types, actual_type));
		}
	}
	else
	{
		m_errors.emplace_back(condition_type.error());
	}

	std::visit([this](auto&& arg) { (*this)(arg); }, *if_stmt.m_true_branch);

	if (if_stmt.m_else_branch.has_value())
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *(*if_stmt.m_else_branch));
	}
}

void TypeChecker::operator()(While& while_stmt)
{
	MidoriResult::TypeResult condition_type = std::visit([this](auto&& arg) { return (*this)(arg); }, *while_stmt.m_condition);
	if (condition_type.has_value())
	{
		const MidoriType& actual_type = *condition_type.value();
		if (!MidoriTypeUtil::IsBoolType(actual_type))
		{
			m_errors.emplace_back(MidoriError::GenerateTypeCheckerError("While statement condition must be of type bool.", while_stmt.m_while_keyword, { &m_atomic_types[3] }, actual_type));
			return;
		}
	}
	else
	{
		m_errors.emplace_back(condition_type.error());
		return;
	}

	std::visit([this](auto&& arg) { (*this)(arg); }, *while_stmt.m_body);
}

void TypeChecker::operator()(For& for_stmt)
{
	BeginScope();
	if (for_stmt.m_condition_intializer.has_value())
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *for_stmt.m_condition_intializer.value());
	}
	if (for_stmt.m_condition.has_value())
	{
		MidoriResult::TypeResult condition_type = std::visit([this](auto&& arg) { return (*this)(arg); }, *for_stmt.m_condition.value());
		if (condition_type.has_value())
		{
			const MidoriType& actual_type = *condition_type.value();
			if (!MidoriTypeUtil::IsBoolType(actual_type))
			{
				m_errors.emplace_back(MidoriError::GenerateTypeCheckerError("For statement condition must be of type bool.", for_stmt.m_for_keyword, { &m_atomic_types[3] }, actual_type));
				return;
			}
		}
		else
		{
			m_errors.emplace_back(condition_type.error());
			return;
		}
	}
	if (for_stmt.m_condition_incrementer.has_value())
	{
		std::visit([this](auto&& arg) { (*this)(arg); }, *for_stmt.m_condition_incrementer.value());
	}

	std::visit([this](auto&& arg) { (*this)(arg); }, *for_stmt.m_body);
	EndScope();
}

void TypeChecker::operator()(Break& break_stmt)
{
	return;
}

void TypeChecker::operator()(Continue& continue_stmt)
{
	return;
}

void TypeChecker::operator()(Return& return_stmt)
{
	MidoriResult::TypeResult return_type = std::visit([this](auto&& arg) { return (*this)(arg); }, *return_stmt.m_value);
	
	if (!return_type.has_value())
	{
		m_errors.emplace_back(return_type.error());
		return;
	}

	const MidoriType& actual_type = *return_type.value();
	const MidoriType& expected_type = *m_curr_closure_return_type;
	if (actual_type != expected_type)
	{
		m_errors.emplace_back(MidoriError::GenerateTypeCheckerError("Return statement expression type error.", return_stmt.m_keyword, { &expected_type }, actual_type));
		return;
	}
}

void TypeChecker::operator()(Namespace& namespace_stmt)
{
	return;
}

MidoriResult::TypeResult TypeChecker::operator()(Binary& binary)
{
	MidoriResult::TypeResult left_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *binary.m_left);
	MidoriResult::TypeResult right_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *binary.m_right);
	if (!left_result.has_value())
	{
		return left_result;
	}
	if (!right_result.has_value())
	{
		return right_result;
	}

	if (*left_result.value() != *right_result.value())
	{
		std::string left_type = MidoriTypeUtil::ToString(*left_result.value());
		std::string right_type = MidoriTypeUtil::ToString(*right_result.value());
		std::string error_message = std::format("Binary expression type error: left type is {}, right type is {}", left_type, right_type);
		return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError(error_message, binary.m_op, {}, *right_result.value()));
	}

	binary.m_type = left_result.value();
	const MidoriType& actual_type = *left_result.value();

	if (m_binary_partial_order_comparison_operators.find(binary.m_op.m_token_name) != m_binary_partial_order_comparison_operators.cend())
	{
		if (!MidoriTypeUtil::IsNumericType(*left_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[0], &m_atomic_types[1] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Binary expression type error", binary.m_op, expected_types, actual_type));
		}

		return std::make_shared<MidoriType>(BoolType());
	}
	else if (m_binary_arithmetic_operators.find(binary.m_op.m_token_name) != m_binary_arithmetic_operators.cend())
	{
		if (!MidoriTypeUtil::IsNumericType(*left_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[0], &m_atomic_types[1] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Binary expression type error", binary.m_op, expected_types, actual_type));
		}
	}
	else if (m_binary_bitwise_operators.find(binary.m_op.m_token_name) != m_binary_bitwise_operators.cend())
	{
		if (!MidoriTypeUtil::IsIntegerType(*left_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[1] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Binary expression type error", binary.m_op, expected_types, actual_type));
		}
	}
	else if (m_binary_equality_operators.find(binary.m_op.m_token_name) != m_binary_equality_operators.cend())
	{
		return std::make_shared<MidoriType>(BoolType());
	}
	else if (m_binary_logical_operators.find(binary.m_op.m_token_name) != m_binary_logical_operators.cend())
	{
		if (!MidoriTypeUtil::IsBoolType(*left_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[3] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Binary expression type error", binary.m_op, expected_types, actual_type));
		}

		return std::make_shared<MidoriType>(BoolType());
	}
	else if (m_binary_concatenation_operators.find(binary.m_op.m_token_name) != m_binary_concatenation_operators.cend())
	{
		if (!(MidoriTypeUtil::IsTextType(*left_result.value()) || MidoriTypeUtil::IsArrayType(*left_result.value())))
		{
			MidoriType undecided_array_type = ArrayType(std::make_shared<MidoriType>(ArrayType(std::make_shared<MidoriType>(UndecidedType()))));
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[2], &undecided_array_type };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Binary expression type error: expected array or text", binary.m_op, expected_types, actual_type));
		}
	}

	return left_result;
}

MidoriResult::TypeResult TypeChecker::operator()(Group& group)
{
	return std::visit([this](auto&& arg) { return (*this)(arg); }, *group.m_expr_in);
}

MidoriResult::TypeResult TypeChecker::operator()(Unary& unary)
{
	MidoriResult::TypeResult right_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *unary.m_right);

	if (!right_result.has_value())
	{
		return right_result;
	}

	const MidoriType& actual_type = *right_result.value();

	if (unary.m_op.m_token_name == Token::Name::MINUS)
	{
		if (!MidoriTypeUtil::IsNumericType(*right_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[0], &m_atomic_types[1] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Unary expression type error", unary.m_op, expected_types, actual_type));
		}
	}
	else if (unary.m_op.m_token_name == Token::Name::BANG)
	{
		if (!MidoriTypeUtil::IsBoolType(*right_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[3] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Unary expression type error", unary.m_op, expected_types, actual_type));
		}
	}
	else if (unary.m_op.m_token_name == Token::Name::TILDE)
	{
		if (!MidoriTypeUtil::IsIntegerType(*right_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[1] };
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Unary expression type error", unary.m_op, expected_types, actual_type));
		}
	}

	unary.m_type = right_result.value();
	return right_result;
}

MidoriResult::TypeResult TypeChecker::operator()(Call& call)
{
	MidoriResult::TypeResult callee_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *call.m_callee);
	if (!callee_result.has_value())
	{
		return callee_result;
	}

	const MidoriType& actual_type = *callee_result.value();

	if (!MidoriTypeUtil::IsFunctionType(actual_type))
	{
		// TODO: Add function type
		MidoriType function_type = FunctionType({ std::make_shared<MidoriType>(UndecidedType()) }, std::make_shared<MidoriType>(UndecidedType()), false);
		std::vector<const MidoriType*> expected_types = { &function_type };
		return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Call expression type error: not a callable", call.m_paren, expected_types, actual_type));
	}

	std::vector<MidoriResult::TypeResult> arg_results;
	std::for_each(call.m_arguments.begin(), call.m_arguments.end(), [this, &arg_results](std::unique_ptr<Expression>& arg)
		{
			arg_results.emplace_back(std::visit([this](auto&& arg) { return (*this)(arg); }, *arg));
		});

	for (size_t i = 0u; i < arg_results.size(); i += 1u)
	{
		if (!arg_results[i].has_value())
		{
			return arg_results[i];
		}
	}

	const FunctionType& function_type = MidoriTypeUtil::GetFunctionType(*callee_result.value());
	call.m_is_native = function_type.m_is_native;

	const std::vector<std::shared_ptr<MidoriType>>& param_types = function_type.m_param_types;
	for (size_t i = 0u; i < arg_results.size(); i += 1u)
	{
		std::vector<const MidoriType*> expected_types = { std::addressof(*param_types[i]) };
		const MidoriType& actual_type = *arg_results[i].value();
		if (*expected_types[0] != actual_type)
		{
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Call expression type error", call.m_paren, expected_types, actual_type));
		}
	}

	return MidoriTypeUtil::GetFunctionType(*callee_result.value()).m_return_type;
}

MidoriResult::TypeResult TypeChecker::operator()(Get& get)
{
	return std::make_shared<MidoriType>(UnitType());
}

MidoriResult::TypeResult TypeChecker::operator()(Set& set)
{
	return std::make_shared<MidoriType>(UnitType());
}

MidoriResult::TypeResult TypeChecker::operator()(Variable& variable)
{
	MidoriResult::TypeResult result;
	for (TypeChecker::TypeEnvironmentStack::reverse_iterator it = m_name_type_table.rbegin(); it != m_name_type_table.rend(); ++it)
	{
		TypeEnvironment::const_iterator var = it->find(variable.m_name.m_lexeme);
		if (var != it->end())
		{
			result = var->second;
			break;
		}
	}

	return result;
}

MidoriResult::TypeResult TypeChecker::operator()(Bind& bind)
{
	MidoriResult::TypeResult result;
	MidoriResult::TypeResult value_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *bind.m_value);
	if (!value_result.has_value())
	{
		return value_result;
	}

	for (TypeChecker::TypeEnvironmentStack::reverse_iterator it = m_name_type_table.rbegin(); it != m_name_type_table.rend(); ++it)
	{
		TypeEnvironment::const_iterator var = it->find(bind.m_name.m_lexeme);
		if (var != it->end())
		{
			std::vector<const MidoriType*> expected_types = { std::addressof(*var->second) };
			const MidoriType& actual_type = *value_result.value();
			if (*expected_types[0] != actual_type)
			{
				return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Bind expression type error", bind.m_name, expected_types, actual_type));
			}
			else
			{
				result = var->second;
				break;
			}
		}
	}

	return result;
}

MidoriResult::TypeResult TypeChecker::operator()(TextLiteral& text)
{
	return std::make_shared<MidoriType>(TextType());
}

MidoriResult::TypeResult TypeChecker::operator()(BoolLiteral& bool_expr)
{
	return std::make_shared<MidoriType>(BoolType());
}

MidoriResult::TypeResult TypeChecker::operator()(FractionLiteral& fraction)
{
	return std::make_shared<MidoriType>(FractionType());
}

MidoriResult::TypeResult TypeChecker::operator()(IntegerLiteral& integer)
{
	return std::make_shared<MidoriType>(IntegerType());
}

MidoriResult::TypeResult TypeChecker::operator()(UnitLiteral& unit)
{
	return std::make_shared<MidoriType>(UnitType());
}

MidoriResult::TypeResult TypeChecker::operator()(Closure& closure)
{
	std::shared_ptr<MidoriType> prev_return_type = std::move(m_curr_closure_return_type);
	m_curr_closure_return_type = closure.m_return_type;

	BeginScope();
	for (size_t i = 0u; i < closure.m_param_types.size(); i += 1u)
	{
		m_name_type_table.back().emplace(closure.m_params[i].m_lexeme, closure.m_param_types[i]);
	}

	std::visit([this](auto&& arg) { (*this)(arg); }, *closure.m_body);

	EndScope();

	m_curr_closure_return_type = std::move(prev_return_type);
	return std::make_shared<MidoriType>(FunctionType(std::move(closure.m_param_types), std::move(closure.m_return_type), false));
}

MidoriResult::TypeResult TypeChecker::operator()(Array& array)
{
	std::vector<MidoriResult::TypeResult> element_results;
	std::for_each(array.m_elems.cbegin(), array.m_elems.cend(), [&element_results, this](const std::unique_ptr<Expression>& element) -> void
		{
			element_results.emplace_back(std::visit([this](auto&& arg) { return (*this)(arg); }, *element));
		});

	for (size_t i = 0u; i < element_results.size(); i += 1u)
	{
		if (!element_results[i].has_value())
		{
			return element_results[i];
		}
	}

	std::vector<const MidoriType*> expected_types = { std::addressof(*element_results[0].value()) };

	for (size_t i = 0u; i < element_results.size(); i += 1u)
	{
		if (*expected_types[0] != *element_results[i].value())
		{
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Array expression type error", array.m_op, expected_types, *element_results[i].value()));
		}
	}

	return std::make_shared<MidoriType>(ArrayType(element_results[0].value()));
}

MidoriResult::TypeResult TypeChecker::operator()(ArrayGet& array_get)
{
	MidoriResult::TypeResult array_var_type_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *array_get.m_arr_var);
	if (!array_var_type_result.has_value())
	{
		return array_var_type_result;
	}

	size_t indices_size = array_get.m_indices.size();
	for (size_t i = 0u; i < indices_size; i += 1u)
	{
		MidoriResult::TypeResult index_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *array_get.m_indices[i]);
		if (!index_result.has_value())
		{
			return index_result;
		}

		if (!MidoriTypeUtil::IsIntegerType(*index_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[1] };
			const MidoriType& actual_type = *index_result.value();
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Array get expression type error", array_get.m_op, expected_types, actual_type));
		}
	}

	const MidoriType* array_var_type = std::addressof(*array_var_type_result.value());
	for (size_t i = 0u; i < indices_size; i += 1u)
	{
		if (!MidoriTypeUtil::IsArrayType(*array_var_type))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[2] };
			const MidoriType& actual_type = *array_var_type;
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Array get expression type error", array_get.m_op, expected_types, actual_type));
		}

		array_var_type = MidoriTypeUtil::GetArrayType(*array_var_type).m_element_type.get();
	}

	return std::make_shared<MidoriType>(*array_var_type);
}

MidoriResult::TypeResult TypeChecker::operator()(ArraySet& array_set)
{
	MidoriResult::TypeResult array_var_type_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *array_set.m_arr_var);
	if (!array_var_type_result.has_value())
	{
		return array_var_type_result;
	}

	MidoriResult::TypeResult value_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *array_set.m_value);
	if (!value_result.has_value())
	{
		return value_result;
	}

	size_t indices_size = array_set.m_indices.size();
	for (size_t i = 0u; i < indices_size; i += 1u)
	{
		MidoriResult::TypeResult index_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *array_set.m_indices[i]);
		if (!index_result.has_value())
		{
			return index_result;
		}

		if (!MidoriTypeUtil::IsIntegerType(*index_result.value()))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[1] };
			const MidoriType& actual_type = *index_result.value();
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Array get expression type error", array_set.m_op, expected_types, actual_type));
		}
	}

	const MidoriType* array_var_type = std::addressof(*array_var_type_result.value());
	for (size_t i = 0u; i < indices_size; i += 1u)
	{
		if (!MidoriTypeUtil::IsArrayType(*array_var_type))
		{
			std::vector<const MidoriType*> expected_types = { &m_atomic_types[2] };
			const MidoriType& actual_type = *array_var_type;
			return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Array get expression type error", array_set.m_op, expected_types, actual_type));
		}

		array_var_type = MidoriTypeUtil::GetArrayType(*array_var_type).m_element_type.get();
	}

	const MidoriType& value_type = *value_result.value();
	if (*array_var_type != value_type)
	{
		std::vector<const MidoriType*> expected_types = { std::addressof(*array_var_type) };
		return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Array set expression type error", array_set.m_op, expected_types, value_type));
	}

	return value_result.value();
}

MidoriResult::TypeResult TypeChecker::operator()(Ternary& ternary)
{
	MidoriResult::TypeResult condition_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *ternary.m_condition);
	if (!condition_result.has_value())
	{
		return condition_result;
	}
	if (!MidoriTypeUtil::IsBoolType(*condition_result.value()))
	{
		std::vector<const MidoriType*> expected_types = { &m_atomic_types[3] };
		const MidoriType& actual_type = *condition_result.value();
		return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Ternary expression type error", ternary.m_question, expected_types, actual_type));
	}

	MidoriResult::TypeResult true_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *ternary.m_true_branch);
	if (!true_result.has_value())
	{
		return true_result;
	}

	MidoriResult::TypeResult else_result = std::visit([this](auto&& arg) { return (*this)(arg); }, *ternary.m_else_branch);
	if (!else_result.has_value())
	{
		return else_result;
	}

	if (*true_result.value() != *else_result.value())
	{
		std::vector<const MidoriType*> expected_types = { std::addressof(*true_result.value()) };
		const MidoriType& actual_type = *else_result.value();
		return std::unexpected<std::string>(MidoriError::GenerateTypeCheckerError("Ternary expression type error", ternary.m_question, expected_types, actual_type));
	}

	return true_result;
}