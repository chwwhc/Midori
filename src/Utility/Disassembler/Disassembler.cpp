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
	const char* two_tabs = "\t\t";

	void SimpleInstruction(const char* name, int& offset)
	{
		offset += 1;

		std::cout << std::left << std::setw(instr_width) << name << std::endl;
	}

	void PopMultipleInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand << std::endl;
	}

	void ConstantInstruction(const char* name, const BytecodeStream& stream, const StaticData& static_data, int& offset)
	{
		int operand;
		if (std::strcmp(name, "CONSTANT_LONG_LONG") == 0)
		{
			operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
				(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8) |
				(static_cast<int>(stream.ReadByteCode(offset + 3)) << 16);
			offset += 4;
		}
		else if (std::strcmp(name, "CONSTANT_LONG") == 0)
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

	void JumpInstruction(const char* name, int sign, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
			(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8);
		offset += 3;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // destination: " << '[' << std::right << std::setfill('0') << std::setw(address_width) << std::hex << (offset + sign * operand) << ']' << std::setfill(' ') << std::endl;
	}

	void GlobalVariableInstruction(const char* name, const BytecodeStream& stream, const GlobalVariableTable& global_table, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // global variable: " << global_table.GetGlobalVariable(operand) << std::setfill(' ') << std::endl;
	}

	void LocalOrCellVariableInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // stack offset: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void ArrayInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // number of indices: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void ArrayCreateInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1)) |
			(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8) |
			(static_cast<int>(stream.ReadByteCode(offset + 3)) << 16);
		offset += 4;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // array length: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void ClosureCreateInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int operand = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << std::left << std::setw(instr_width) << name;
		std::cout << ' ' << std::dec << operand;
		std::cout << two_tabs << std::setw(comment_width) << " // number of captured variables: " << std::dec << operand << std::setfill(' ') << std::endl;
	}

	void CallInstruction(const char* name, const BytecodeStream& stream, int& offset)
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
		case OpCode::RESERVE_ARRAY:
			SimpleInstruction("RESERVE_ARRAY", offset);
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
		case OpCode::ADD:
			SimpleInstruction("ADD", offset);
			break;
		case OpCode::SUBTRACT:
			SimpleInstruction("SUBTRACT", offset);
			break;
		case OpCode::MULTIPLY:
			SimpleInstruction("MULTIPLY", offset);
			break;
		case OpCode::DIVIDE:
			SimpleInstruction("DIVIDE", offset);
			break;
		case OpCode::MODULO:
			SimpleInstruction("MODULO", offset);
			break;
		case OpCode::CONCAT:
			SimpleInstruction("CONCAT", offset);
			break;
		case OpCode::EQUAL:
			SimpleInstruction("EQUAL", offset);
			break;
		case OpCode::NOT_EQUAL:
			SimpleInstruction("NOT_EQUAL", offset);
			break;
		case OpCode::GREATER:
			SimpleInstruction("GREATER", offset);
			break;
		case OpCode::GREATER_EQUAL:
			SimpleInstruction("GREATER_EQUAL", offset);
			break;
		case OpCode::LESS:
			SimpleInstruction("LESS", offset);
			break;
		case OpCode::LESS_EQUAL:
			SimpleInstruction("LESS_EQUAL", offset);
			break;
		case OpCode::NOT:
			SimpleInstruction("NOT", offset);
			break;
		case OpCode::NEGATE:
			SimpleInstruction("NEGATE", offset);
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
		case OpCode::CALL:
			CallInstruction("CALL", stream, offset);
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