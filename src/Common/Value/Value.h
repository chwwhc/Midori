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

class Traceable;

class Value
{

public:
	struct NativeFunction
	{
		std::function<void()> m_cpp_function;
		const char* m_name;
		int m_arity;
	};

private:
	std::variant<double, std::monostate, bool, NativeFunction, Traceable*> m_value;

private:
	static std::string DoubleToStringWithoutTrailingZeros(double value);

public:

	Value() : m_value(std::monostate()) {}

	Value(double d) : m_value(d) {}

	Value(bool b) : m_value(b) {}

	Value(NativeFunction f) : m_value(std::move(f)) {}

	Value(Traceable* o) : m_value(o) {}

	inline double GetNumber() const { return std::get<double>(m_value); }

	inline bool IsNumber() const { return std::holds_alternative<double>(m_value); }

	inline std::monostate GetUnit() const { return std::get<std::monostate>(m_value); }

	inline bool IsUnit() const { return std::holds_alternative<std::monostate>(m_value); }

	inline bool GetBool() const { return std::get<bool>(m_value); }

	inline bool IsBool() const { return std::holds_alternative<bool>(m_value); }

	inline NativeFunction& GetNativeFunction() const { return const_cast<NativeFunction&>(std::get<NativeFunction>(m_value)); }

	inline bool IsNativeFunction() const { return std::holds_alternative<NativeFunction>(m_value); }

	inline Traceable* GetObjectPointer() const { return std::get<Traceable*>(m_value); }

	inline bool IsObjectPointer() const { return std::holds_alternative<Traceable*>(m_value); }

	inline static bool AreSameType(const Value& lhs, const Value& rhs) { return lhs.m_value.index() == rhs.m_value.index(); }

	inline friend bool operator==(const Value& lhs, const Value& rhs) { return lhs.m_value == rhs.m_value; }

	inline friend bool operator!=(const Value& lhs, const Value& rhs) { return !(lhs == rhs); }

	std::string ToString() const;
};

class Traceable
{
public:
	using GarbageCollectionRoots = std::unordered_set<Traceable*>;

	static inline size_t s_total_bytes_allocated;
	static inline size_t s_static_bytes_allocated;
	static inline std::list<Traceable*> s_objects;

	struct CellValue
	{
		Value m_value;
	};

	struct Closure
	{
		std::vector<Traceable*> m_cell_values;
		int m_bytecode_index;
		int m_arity;
	};

private:
	std::variant<std::string, std::vector<Value>, CellValue, Closure> m_value;
	size_t m_size;
	bool m_is_marked;

public:

	template<typename T>
	static Traceable* AllocateObject(T&& value) { return new Traceable(std::forward<T>(value)); }

	inline std::string& GetString() const { return const_cast<std::string&>(std::get<std::string>(m_value)); }

	inline bool IsString() const { return std::holds_alternative<std::string>(m_value); }

	inline std::vector<Value>& GetArray() const { return const_cast<std::vector<Value>&>(std::get<std::vector<Value>>(m_value)); }

	inline bool IsArray() const { return std::holds_alternative<std::vector<Value>>(m_value); }

	inline bool IsCellValue() const { return std::holds_alternative<CellValue>(m_value); }

	inline CellValue& GetCellValue() const { return const_cast<CellValue&>(std::get<CellValue>(m_value)); }

	inline bool IsClosure() const { return std::holds_alternative<Closure>(m_value); }

	inline Closure& GetClosure() const { return const_cast<Closure&>(std::get<Closure>(m_value)); }

	inline size_t GetSize() const { return m_size; }

	inline void Mark() { m_is_marked = true; }

	inline void Unmark() { m_is_marked = false; }

	inline bool IsMarked() const { return m_is_marked; }

	static inline void* operator new(size_t size)
	{
		void* object = ::operator new(size);
		Traceable* traceable = static_cast<Traceable*>(object);

		traceable->m_size = size;
		s_total_bytes_allocated += size;
		s_objects.emplace_back(traceable);

		return static_cast<void*>(traceable);
	}

	static inline void operator delete(void* object, size_t size) noexcept
	{
		Traceable* traceable = static_cast<Traceable*>(object);
		s_total_bytes_allocated -= traceable->m_size;

		::operator delete(object, size);
	}

	std::string ToString() const;

	void Trace();

	static void CleanUp();

	static void PrintMemoryTelemetry();

private:
	Traceable() = default;

	Traceable(const Traceable& other) = default;

	Traceable(Traceable&& other) noexcept = default;

	Traceable(std::string&& str) : m_value(std::move(str)) {}

	Traceable(std::vector<Value>&& array) : m_value(std::move(array)) {}

	Traceable(CellValue&& cell_value) : m_value(std::move(cell_value)) {}

	Traceable(Closure&& closure) : m_value(std::move(closure)) {}
};
