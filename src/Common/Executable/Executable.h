#pragma once

#include <cinttypes>

#include "Common/Value/Value.h"

enum class OpCode : uint8_t
{
	// Constants and Literals
	LOAD_CONSTANT,
	LOAD_CONSTANT_LONG,
	LOAD_CONSTANT_LONG_LONG,
	INTEGER_CONSTANT,
	FRACTION_CONSTANT,
	OP_UNIT,
	OP_TRUE,
	OP_FALSE,

	// Array Operations
	CREATE_ARRAY,
	GET_ARRAY,
	SET_ARRAY,
	DUP_ARRAY,
	ADD_BACK_ARRAY,
	ADD_FRONT_ARRAY,

	// Atomic type casting
	CAST_TO_FRACTION,
	CAST_TO_INTEGER,
	CAST_TO_TEXT,
	CAST_TO_BOOL,
	CAST_TO_UNIT,

	// Bit Operations
	LEFT_SHIFT,
	RIGHT_SHIFT,
	BITWISE_AND,
	BITWISE_OR,
	BITWISE_XOR,
	BITWISE_NOT,

	// Arithmetic Operations
	ADD_FRACTION,
	SUBTRACT_FRACTION,
	MULTIPLY_FRACTION,
	DIVIDE_FRACTION,
	MODULO_FRACTION,
	ADD_INTEGER,
	SUBTRACT_INTEGER,
	MULTIPLY_INTEGER,
	DIVIDE_INTEGER,
	MODULO_INTEGER,

	// Concatenations 
	CONCAT_ARRAY,
	CONCAT_TEXT,

	// Comparison Operations
	EQUAL_FRACTION,
	NOT_EQUAL_FRACTION,
	GREATER_FRACTION,
	GREATER_EQUAL_FRACTION,
	LESS_FRACTION,
	LESS_EQUAL_FRACTION,
	EQUAL_INTEGER,
	NOT_EQUAL_INTEGER,
	GREATER_INTEGER,
	GREATER_EQUAL_INTEGER,
	LESS_INTEGER,
	LESS_EQUAL_INTEGER,
	EQUAL_TEXT,

	// Logical Operations
	NOT,

	// UnaryPrefix Operations
	NEGATE_FRACTION,
	NEGATE_INTEGER,

	// Control Flow
	JUMP_IF_FALSE,
	JUMP_IF_TRUE,
	JUMP,
	JUMP_BACK,
	IF_INTEGER_LESS,
	IF_INTEGER_LESS_EQUAL,
	IF_INTEGER_GREATER,
	IF_INTEGER_GREATER_EQUAL,
	IF_INTEGER_EQUAL,	
	IF_INTEGER_NOT_EQUAL,
	IF_FRACTION_LESS,
	IF_FRACTION_LESS_EQUAL,
	IF_FRACTION_GREATER,
	IF_FRACTION_GREATER_EQUAL,
	IF_FRACTION_EQUAL,
	IF_FRACTION_NOT_EQUAL,

	// Switch
	LOAD_TAG,
	SET_TAG,

	// Callable
	CALL_FOREIGN,
	CALL_DEFINED,
	CONSTRUCT_STRUCT,
	CONSTRUCT_UNION,

	// Variable Operations
	ALLOCATE_CLOSURE,
	CONSTRUCT_CLOSURE,
	DEFINE_GLOBAL,
	GET_GLOBAL,
	SET_GLOBAL,
	GET_LOCAL,
	SET_LOCAL,
	GET_CELL,
	SET_CELL,

	// Struct Operations
	GET_MEMBER,
	SET_MEMBER,

	// Stack Operations
	POP,
	DUP,
	POP_SCOPE,
	POP_MULTIPLE,

	// Return
	RETURN,
	HALT,
};

class BytecodeStream
{
public:
	using iterator = std::vector<OpCode>::iterator;
	using const_iterator = std::vector<OpCode>::const_iterator;
	using reverse_iterator = std::vector<OpCode>::reverse_iterator;
	using const_reverse_iterator = std::vector<OpCode>::const_reverse_iterator;

	iterator begin();
	iterator end();
	const_iterator cbegin() const;
	const_iterator cend() const;
	reverse_iterator rbegin();
	reverse_iterator rend();
	const_reverse_iterator crbegin() const;
	const_reverse_iterator crend() const;

private:
	std::vector<OpCode> m_bytecode;
	std::vector<std::pair<int, int>> m_line_info; // Pair of line number and count of consecutive instructions

public:

	OpCode ReadByteCode(int index) const;

	void SetByteCode(int index, OpCode byte);

	void AddByteCode(OpCode byte, int line);

	void PopByteCode(int line);

	int GetByteCodeSize() const;

	bool IsByteCodeEmpty() const;

	int GetLine(int index) const;

	void Append(BytecodeStream&& other);

	const OpCode* operator[](int index) const;
};

class MidoriExecutable
{
public:
	using StaticData = std::vector<MidoriValue>;
	using GlobalNames = std::vector<MidoriText>;
	using Procedures = std::vector<BytecodeStream>;

#ifdef DEBUG
	std::vector<MidoriText> m_procedure_names;
#endif

private:
	MidoriTraceable::GarbageCollectionRoots m_constant_roots;
	StaticData m_constants;
	GlobalNames m_globals;
	Procedures m_procedures;

public:
	const MidoriValue& GetConstant(int index) const;

	int AddConstant(MidoriValue&& value);

	int AddGlobalVariable(MidoriText&& name);

	const MidoriText& GetGlobalVariable(int index) const;

	void AddConstantRoot(MidoriTraceable* root);

	void AttachProcedures(Procedures&& bytecode);

#ifdef DEBUG
	void AttachProcedureNames(std::vector<MidoriText>&& procedure_names);
#endif

	int GetLine(int instr_index, int proc_index) const;

	const BytecodeStream& GetBytecodeStream(int proc_index) const;

	const MidoriTraceable::GarbageCollectionRoots& GetConstantRoots();

	OpCode ReadByteCode(int instr_index, int proc_index) const;

	int GetByteCodeSize(int proc_index) const;

	int GetProcedureCount() const;

	int GetGlobalVariableCount() const;
};