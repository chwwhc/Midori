#pragma once

#include <functional>
#include <list>
#include <string>
#include <variant>
#include <vector>
#include <unordered_set>
#include <memory>

#include "Common/BytecodeStream/BytecodeStream.h"

#define THREE_BYTE_MAX 16777215

class MidoriTraceable;

class MidoriValue
{

public:
	using MidoriInteger = int64_t;
	using MidoriFraction = double;
	using MidoriUnit = std::monostate;
	using MidoriBool = bool;


	struct NativeFunction
	{
		std::function<void()> m_cpp_function;
		const char* m_name;
		int m_arity;
	};

private:
	std::variant<MidoriFraction, MidoriInteger, MidoriUnit, MidoriBool, NativeFunction, MidoriTraceable*> m_value;

private:
	static std::string DoubleToStringWithoutTrailingZeros(MidoriFraction value);

public:

	MidoriValue() : m_value(MidoriUnit()) {}

	MidoriValue(MidoriFraction d) : m_value(d) {}

	MidoriValue(MidoriInteger l) : m_value(l) {}

	MidoriValue(bool b) : m_value(b) {}

	MidoriValue(NativeFunction f) : m_value(std::move(f)) {}

	MidoriValue(MidoriTraceable* o) : m_value(o) {}

	inline MidoriFraction GetFraction() const { return std::get<MidoriFraction>(m_value); }

	inline bool IsFraction() const { return std::holds_alternative<MidoriFraction>(m_value); }

	inline MidoriInteger GetInteger() const { return std::get<MidoriInteger>(m_value); }

	inline bool IsInteger() const { return std::holds_alternative<MidoriInteger>(m_value); }

	inline MidoriUnit GetUnit() const { return std::get<MidoriUnit>(m_value); }

	inline bool IsUnit() const { return std::holds_alternative<MidoriUnit>(m_value); }

	inline MidoriBool GetBool() const { return std::get<MidoriBool>(m_value); }

	inline bool IsBool() const { return std::holds_alternative<MidoriBool>(m_value); }

	inline NativeFunction& GetNativeFunction() const { return const_cast<NativeFunction&>(std::get<NativeFunction>(m_value)); }

	inline bool IsNativeFunction() const { return std::holds_alternative<NativeFunction>(m_value); }

	inline MidoriTraceable* GetObjectPointer() const { return std::get<MidoriTraceable*>(m_value); }

	inline bool IsObjectPointer() const { return std::holds_alternative<MidoriTraceable*>(m_value); }

	inline static bool AreSameType(const MidoriValue& lhs, const MidoriValue& rhs) { return lhs.m_value.index() == rhs.m_value.index(); }

	inline friend bool operator==(const MidoriValue& lhs, const MidoriValue& rhs) { return lhs.m_value == rhs.m_value; }

	inline friend bool operator!=(const MidoriValue& lhs, const MidoriValue& rhs) { return !(lhs == rhs); }

	std::string ToString() const;
};

class MidoriTraceable
{
public:
	using GarbageCollectionRoots = std::unordered_set<MidoriTraceable*>;
	using CellValue = MidoriValue;

	static inline size_t s_total_bytes_allocated;
	static inline size_t s_static_bytes_allocated;
	static inline std::list<MidoriTraceable*> s_objects;

	using MidoriText = std::string;
	using MidoriArray = std::vector<MidoriValue>;

	struct Closure
	{
		std::vector<MidoriTraceable*> m_cell_values;
		int m_module_index;
		int m_arity;
	};

private:
	std::variant<MidoriText, std::vector<MidoriValue>, CellValue, Closure> m_value;
	size_t m_size;
	bool m_is_marked;

public:

	template<typename T>
	static MidoriTraceable* AllocateObject(T&& value) { return new MidoriTraceable(std::forward<T>(value)); }

	inline MidoriText& GetText() const { return const_cast<MidoriText&>(std::get<MidoriText>(m_value)); }

	inline bool IsText() const { return std::holds_alternative<MidoriText>(m_value); }

	inline MidoriArray& GetArray() const { return const_cast<MidoriArray&>(std::get<MidoriArray>(m_value)); }

	inline bool IsArray() const { return std::holds_alternative<MidoriArray>(m_value); }

	inline bool IsCellValue() const { return std::holds_alternative<CellValue>(m_value); }

	inline CellValue& GetCellValue() const { return const_cast<CellValue&>(std::get<CellValue>(m_value)); }

	inline bool IsClosure() const { return std::holds_alternative<Closure>(m_value); }

	inline Closure& GetClosure() const { return const_cast<Closure&>(std::get<Closure>(m_value)); }

	inline size_t GetSize() const { return m_size; }

	inline void Mark() { m_is_marked = true; }

	inline void Unmark() { m_is_marked = false; }

	inline bool Marked() const { return m_is_marked; }

	static inline void* operator new(size_t size)
	{
		void* object = ::operator new(size);
		MidoriTraceable* traceable = static_cast<MidoriTraceable*>(object);

		traceable->m_size = size;
		s_total_bytes_allocated += size;
		s_objects.emplace_back(traceable);

		return static_cast<void*>(traceable);
	}

	static inline void operator delete(void* object, size_t size) noexcept
	{
		MidoriTraceable* traceable = static_cast<MidoriTraceable*>(object);
		s_total_bytes_allocated -= traceable->m_size;

		::operator delete(object, size);
	}

	std::string ToString() const;

	void Trace();

	static void CleanUp();

	static void PrintMemoryTelemetry();

private:
	MidoriTraceable() = default;

	MidoriTraceable(const MidoriTraceable& other) = default;

	MidoriTraceable(MidoriTraceable&& other) noexcept = default;

	MidoriTraceable(std::string&& str) : m_value(std::move(str)) {}

	MidoriTraceable(std::vector<MidoriValue>&& array) : m_value(std::move(array)) {}

	MidoriTraceable(CellValue&& cell_value) : m_value(std::move(cell_value)) {}

	MidoriTraceable(Closure&& closure) : m_value(std::move(closure)) {}
};
