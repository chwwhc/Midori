#include "Disassembler.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Common/Value/StaticData.h"

#include <iostream>
#include <iomanip>

namespace
{
	constexpr int address_width = 8;
	constexpr int instr_width = 20;
	constexpr int comment_width = 20;
	std::string_view two_tabs = "\t\t";

	void SimpleInstruction(std::string_view name, int& offset)
	{
		offset += 1;

		std::cout << std::left << std::setw(instr_width) << name << std::endl;
	}

	void PopMultipleInstruction(std::string_view name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand << std::endl;
	}

	void ConstantInstruction(std::string_view name, const BytecodeStream& stream, const StaticData& static_data, int& offset)
	{
		int operand;
		if (name == "CONSTANT_LONG_LONG")
		{
			operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
				(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8) |
				(static_cast<int>(stream.ReadByteCode(offset + 3)) << 16);
			offset += 4;
		}
		else if (name == "CONSTANT_LONG")
		{
			operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
				(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8);
			offset += 3;
		}
		else
		{
			operand = static_cast<int>(stream.ReadByteCode(offset + 1));
			offset += 2;
		}

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // static value: " << static_data.GetConstant(operand).ToString() << std::setfill(' ') << std::endl;
	}

	void JumpInstruction(std::string_view name, int sign, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
			(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8);
		offset += 3;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // destination: " << '[' << std::right << std::setfill('0') << std::setw(address_width) << std::hex << (offset + sign * operand) << ']' << std::setfill(' ') << std::endl;
	}

	void GlobalVariableInstruction(std::string_view name, const BytecodeStream& stream, const GlobalVariableTable& global_table, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // global variable: " << global_table.GetGlobalVariable(operand) << std::setfill(' ') << std::endl;
	}

	void LocalOrCellVariableInstruction(std::string_view name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // stack offset: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void ArrayInstruction(std::string_view name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // number of indices: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void ArrayCreateInstruction(std::string_view name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
			(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8) |
			(static_cast<int>(stream.ReadByteCode(offset + 3)) << 16);
		offset += 4;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // array length: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void ClosureCreateInstruction(std::string_view name, const BytecodeStream& stream, int& offset)
	{
		// TODO: fix this	
		int captured_count = static_cast<int>(stream.ReadByteCode(offset + 1));
		int arity = static_cast<int>(stream.ReadByteCode(offset + 2));
		int index = static_cast<int>(stream.ReadByteCode(offset + 3));
		offset += 4;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << captured_count << ", " << std::dec << arity << ", " << std::dec << index;
		std::cout << std::setw(comment_width) << " // number of captured variables: " << std::dec << captured_count << std::setfill(' ') << std::endl;
	}

	void CallInstruction(std::string_view name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // number of parameters: " << std::dec << operand << std::setfill(' ') << std::endl;
	}
}

namespace Disassembler
{
	void DisassembleBytecodeStream(const BytecodeStream& stream, const StaticData& static_data, const GlobalVariableTable& global_table, const char* name)
	{
		std::cout << "================================================== " << name << " ==================================================" << std::endl;

		int offset = 0;
		while (offset < stream.GetByteCodeSize())
		{
			DisassembleInstruction(stream, static_data, global_table, offset);
		}
		std::cout << "-----------------------------------------------------------------------------------------------\n" << std::endl;
	}

	void DisassembleInstruction(const BytecodeStream& stream, const StaticData& static_data, const GlobalVariableTable& global_table, int& offset)
	{
		std::cout << '[' << std::right << std::setfill('0') << std::setw(::address_width) << std::hex << offset << "] " << std::setfill(' ');

		std::cout << std::setw(::address_width) << std::left;
		if (offset > 0 && stream.GetLine(offset) == stream.GetLine(offset - 1))
		{
			std::cout << "|" << std::setfill(' ');
		} 
		else
		{
			std::cout << std::dec << stream.GetLine(offset) << std::setfill(' ');
		}
		std::cout << std::right << ' ';

		OpCode instruction = stream.ReadByteCode(offset);
		switch (instruction) {
		case OpCode::CONSTANT:
			ConstantInstruction("CONSTANT", stream, static_data, offset);
			break;
		case OpCode::CONSTANT_LONG:
			ConstantInstruction("CONSTANT_LONG", stream, static_data, offset);
			break;
		case OpCode::CONSTANT_LONG_LONG:
			ConstantInstruction("CONSTANT_LONG_LONG", stream, static_data, offset);
			break;
		case OpCode::UNIT:
			SimpleInstruction("UNIT", offset);
			break;
		case OpCode::TRUE:
			SimpleInstruction("TRUE", offset);
			break;
		case OpCode::FALSE:
			SimpleInstruction("FALSE", offset);
			break;
		case OpCode::CREATE_ARRAY:
			ArrayCreateInstruction("CREATE_ARRAY", stream, offset);
			break;
		case OpCode::GET_ARRAY:
			ArrayInstruction("GET_ARRAY", stream, offset);
			break;
		case OpCode::SET_ARRAY:
			ArrayInstruction("SET_ARRAY", stream, offset);
			break;
		case OpCode::LEFT_SHIFT:
			SimpleInstruction("LEFT_SHIFT", offset);
			break;
		case OpCode::RIGHT_SHIFT:
			SimpleInstruction("RIGHT_SHIFT", offset);
			break;
		case OpCode::BITWISE_AND:
			SimpleInstruction("BITWISE_AND", offset);
			break;
		case OpCode::BITWISE_OR:
			SimpleInstruction("BITWISE_OR", offset);
			break;
		case OpCode::BITWISE_XOR:
			SimpleInstruction("BITWISE_XOR", offset);
			break;
		case OpCode::BITWISE_NOT:
			SimpleInstruction("BITWISE_NOT", offset);
			break;
		case OpCode::ADD_FRACTION:
			SimpleInstruction("ADD_FRACTION", offset);
			break;
		case OpCode::SUBTRACT_FRACTION:
			SimpleInstruction("SUBTRACT_FRACTION", offset);
			break;
		case OpCode::MULTIPLY_FRACTION:
			SimpleInstruction("MULTIPLY_FRACTION", offset);
			break;
		case OpCode::DIVIDE_FRACTION:
			SimpleInstruction("DIVIDE_FRACTION", offset);
			break;
		case OpCode::MODULO_FRACTION:
			SimpleInstruction("MODULO_FRACTION", offset);
			break;
		case OpCode::ADD_INTEGER:
			SimpleInstruction("ADD_INTEGER", offset);
			break;
		case OpCode::SUBTRACT_INTEGER:
			SimpleInstruction("SUBTRACT_INTEGER", offset);
			break;
		case OpCode::MULTIPLY_INTEGER:
			SimpleInstruction("MULTIPLY_INTEGER", offset);
			break;
		case OpCode::DIVIDE_INTEGER:
			SimpleInstruction("DIVIDE_INTEGER", offset);
			break;
		case OpCode::MODULO_INTEGER:
			SimpleInstruction("MODULO_INTEGER", offset);
			break;
		case OpCode::CONCAT_ARRAY:
			SimpleInstruction("CONCAT_ARRAY", offset);
			break;
		case OpCode::CONCAT_TEXT:
			SimpleInstruction("CONCAT_TEXT", offset);
			break;
		case OpCode::EQUAL_FRACTION:
			SimpleInstruction("EQUAL_FRACTION", offset);
			break;
		case OpCode::NOT_EQUAL_FRACTION:
			SimpleInstruction("NOT_EQUAL_FRACTION", offset);
			break;
		case OpCode::GREATER_FRACTION:
			SimpleInstruction("GREATER_FRACTION", offset);
			break;
		case OpCode::GREATER_EQUAL_FRACTION:
			SimpleInstruction("GREATER_EQUAL_FRACTION", offset);
			break;
		case OpCode::LESS_FRACTION:
			SimpleInstruction("LESS_FRACTION", offset);
			break;
		case OpCode::LESS_EQUAL_FRACTION:
			SimpleInstruction("LESS_EQUAL_FRACTION", offset);
			break;
		case OpCode::EQUAL_INTEGER:
			SimpleInstruction("EQUAL_INTEGER", offset);
			break;
		case OpCode::NOT_EQUAL_INTEGER:
			SimpleInstruction("NOT_EQUAL_INTEGER", offset);
			break;
		case OpCode::GREATER_INTEGER:
			SimpleInstruction("GREATER_INTEGER", offset);
			break;
		case OpCode::GREATER_EQUAL_INTEGER:
			SimpleInstruction("GREATER_EQUAL_INTEGER", offset);
			break;
		case OpCode::LESS_INTEGER:
			SimpleInstruction("LESS_INTEGER", offset);
			break;
		case OpCode::LESS_EQUAL_INTEGER:
			SimpleInstruction("LESS_EQUAL_INTEGER", offset);
			break;
		case OpCode::NOT:
			SimpleInstruction("NOT", offset);
			break;
		case OpCode::NEGATE_FRACTION:
			SimpleInstruction("NEGATE_FRACTION", offset);
			break;
		case OpCode::NEGATE_INTEGER:
			SimpleInstruction("NEGATE_INTEGER", offset);
			break;
		case OpCode::JUMP_IF_FALSE:
			JumpInstruction("JUMP_IF_FALSE", 1, stream, offset);
			break;
		case OpCode::JUMP_IF_TRUE:
			JumpInstruction("JUMP_IF_TRUE", 1, stream, offset);
			break;
		case OpCode::JUMP:
			JumpInstruction("JUMP", 1, stream, offset);
			break;
		case OpCode::JUMP_BACK:
			JumpInstruction("JUMP_BACK", -1, stream, offset);
			break;
		case OpCode::CALL_NATIVE:
			CallInstruction("CALL_NATIVE", stream, offset);
			break;
		case OpCode::CALL_DEFINED:
			CallInstruction("CALL_DEFINED", stream, offset);
			break;
		case OpCode::CREATE_CLOSURE:
			ClosureCreateInstruction("CREATE_CLOSURE", stream, offset);
			break;
		case OpCode::DEFINE_GLOBAL:
			GlobalVariableInstruction("DEFINE_GLOBAL", stream, global_table, offset);
			break;
		case OpCode::GET_GLOBAL:
			GlobalVariableInstruction("GET_GLOBAL", stream, global_table, offset);
			break;
		case OpCode::SET_GLOBAL:
			GlobalVariableInstruction("SET_GLOBAL", stream, global_table, offset);
			break;
		case OpCode::GET_LOCAL:
			LocalOrCellVariableInstruction("GET_LOCAL", stream, offset);
			break;
		case OpCode::SET_LOCAL:
			LocalOrCellVariableInstruction("SET_LOCAL", stream, offset);
			break;
		case OpCode::GET_CELL:
			LocalOrCellVariableInstruction("GET_CELL", stream, offset);
			break;
		case OpCode::SET_CELL:
			LocalOrCellVariableInstruction("SET_CELL", stream, offset);
			break;
		case OpCode::POP:
			SimpleInstruction("POP", offset);
			break;
		case OpCode::POP_MULTIPLE:
			PopMultipleInstruction("POP_MULTIPLE", stream, offset);
			break;
		case OpCode::RETURN:
			SimpleInstruction("RETURN", offset);
			break;
		case OpCode::HALT:
			SimpleInstruction("HALT", offset);
			break;
		default:
			break;
		}
	}
}