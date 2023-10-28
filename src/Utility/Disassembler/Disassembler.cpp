#include "Disassembler.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Common/Value/StaticData.h"

#include <iostream>
#include <iomanip>

namespace
{
	void SimpleInstruction(const char* name, int& offset)
	{
		std::cout << name << std::endl;
		offset += 1;
	}

	void PopMultipleInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int count = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;
		std::cout << name << " " << count << std::endl;
	}

	void ConstantInstruction(const char* name, const BytecodeStream& stream, const StaticData& static_data, int& offset)
	{
		int index;
		if (std::strcmp(name, "CONSTANT_LONG_LONG") == 0)
		{
			index = static_cast<int>(stream.ReadByteCode(offset + 1)) |
				(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8) |
				(static_cast<int>(stream.ReadByteCode(offset + 3)) << 16);
			offset += 4;
		}
		else if (std::strcmp(name, "CONSTANT_LONG") == 0)
		{
			index = static_cast<int>(stream.ReadByteCode(offset + 1)) |
				(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8);
			offset += 3;
		}
		else
		{
			index = static_cast<int>(stream.ReadByteCode(offset + 1));
			offset += 2;
		}

		std::cout << name << " " << index << " {{ " << static_data.GetConstant(index).ToString() << " }}" << std::endl;
	}

	void JumpInstruction(const char* name, int sign, const BytecodeStream& stream, int& offset)
	{
		int index = static_cast<int>(stream.ReadByteCode(offset + 1)) |
			(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8);
		offset += 3;

		std::cout << name << " (# destination " << std::setfill('0') << std::setw(4) << std::hex << offset + sign * index << " #)" << std::endl;
	}

	void GlobalVariableInstruction(const char* name, const BytecodeStream& stream, const GlobalVariableTable& global_table, int& offset)
	{
		int index = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << name << " " << index << " {{ global variable: " << global_table.GetGlobalVariable(index) << " }}" << std::endl;
	}

	void LocalOrCellVariableInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int index = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << name << " (* offset: " << index << " *)" << std::endl;
	}

	void ArrayInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int index = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;

		std::cout << name << " (- num of indices: " << index << " -)" << std::endl;
	}

	void ArrayCreateInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int length = static_cast<int>(stream.ReadByteCode(offset + 1)) |
			(static_cast<int>(stream.ReadByteCode(offset + 2)) << 8) |
			(static_cast<int>(stream.ReadByteCode(offset + 3)) << 16);
		offset += 4;
		std::cout << name << " (- num of elements: " << length << " -)" << std::endl;
	}

	void ClosureCreateInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int count = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;
		std::cout << name << " (- num of captured variables: " << count << " -)" << std::endl;
	}

	void CallInstruction(const char* name, const BytecodeStream& stream, int& offset)
	{
		int index = static_cast<int>(stream.ReadByteCode(offset + 1));
		offset += 2;
		std::cout << name << " (^ num of parameters: " << index << " ^)" << std::endl;
	}
}

namespace Disassembler
{
	void DisassembleBytecodeStream(const BytecodeStream& stream, const StaticData& static_data, const GlobalVariableTable& global_table, const char* name)
	{
		std::cout << "--------------- " << name << " ---------------" << std::endl;

		int offset = 0;
		while (offset < stream.GetByteCodeSize())
		{
			DisassembleInstruction(stream, static_data, global_table, offset);
		}
	}

	void DisassembleInstruction(const BytecodeStream& stream, const StaticData& static_data, const GlobalVariableTable& global_table, int& offset)
	{
		std::cout << std::setfill('0') << std::setw(4) << std::hex << offset << ' ';

		if (offset > 0 && stream.GetLine(offset) == stream.GetLine(offset - 1))
		{
			std::cout << "   | ";
		}
		else
		{
			std::cout << stream.GetLine(offset) << " ";
		}

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