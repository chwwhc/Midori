#pragma once

#include <functional>
#include <list>
#include <string>
#include <variant>
#include <vector>
#include <iostream>
#include <unordered_set>

#include "Common/BytecodeStream/BytecodeStream.h"

#define THREE_BYTE_MAX 16777215

class Object;

class Traceable {

public:
	using GarbageCollectionRoots = std::unordered_set<Traceable*>;

	bool m_is_marked;
	size_t m_size;
	static inline size_t s_total_bytes_allocated;
	static inline size_t s_static_bytes_allocated;
	static inline std::list<Traceable*> s_objects;

public:

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

	static inline void CleanUp() 
	{
		s_static_bytes_allocated = 0u;
		for (Traceable* object : s_objects) 
		{
			delete object;
		}
		s_objects.clear();
	}

	static void PrintMemoryTelemetry() 
	{
		std::cout << "\n------------------------------\n";
		std::cout << "Memory telemetry:\n";
		std::cout << "Heap objects allocated: " << std::dec << s_objects.size() << '\n';
		std::cout << "Total Bytes allocated: " << std::dec << s_total_bytes_allocated << '\n';
		std::cout << "Static Bytes allocated: " << std::dec << s_static_bytes_allocated << '\n';
		std::cout << "Dynamic Bytes allocated: " << std::dec << s_total_bytes_allocated - s_static_bytes_allocated;
		std::cout << "\n------------------------------\n";
	}
};


class Value
{

public:

private:
	std::variant<double, std::monostate, bool, Object*> m_value;

private:
	static std::string DoubleToStringWithoutTrailingZeros(double value)
	{
		std::string str = std::to_string(value);

		size_t decimal_point = str.find('.');
		if (decimal_point != std::string::npos)
		{
			str.erase(str.find_last_not_of('0') + 1, std::string::npos);
			if (str.back() == '.')
			{
				str.pop_back();
			}
		}

		return str;
	}

public:

	Value() : m_value(std::monostate()) {}

	Value(double d) : m_value(d) {}

	Value(bool b) : m_value(b) {}

	Value(Object* o) : m_value(o) {}

	inline double GetNumber() const { return std::get<double>(m_value); }

	inline bool IsNumber() const { return std::holds_alternative<double>(m_value); }

	inline std::monostate GetNull() const { return std::get<std::monostate>(m_value); }

	inline bool IsNull() const { return std::holds_alternative<std::monostate>(m_value); }

	inline bool GetBool() const { return std::get<bool>(m_value); }

	inline bool IsBool() const { return std::holds_alternative<bool>(m_value); }

	inline Object* GetObjectPointer() const { return std::get<Object*>(m_value); }

	inline bool IsObjectPointer() const { return std::holds_alternative<Object*>(m_value); }

	inline static bool AreSameType(const Value& lhs, const Value& rhs) { return lhs.m_value.index() == rhs.m_value.index(); }

	inline std::string ToString() const
	{
		return std::visit([](auto&& arg) -> std::string
			{
				using Type = std::decay_t<decltype(arg)>;

				if constexpr (std::is_same_v<Type, double>)
				{
					return DoubleToStringWithoutTrailingZeros(arg);
				}
				else if constexpr (std::is_same_v<Type, std::monostate>)
				{
					return "nil";
				}
				else if constexpr (std::is_same_v<Type, bool>)
				{
					return arg ? "true" : "false";
				}
				else if constexpr (std::is_same_v<Type, Object*>)
				{
					return arg->ToString();
				}
			}, m_value);
	}

	friend bool operator==(const Value& lhs, const Value& rhs)
	{
		return lhs.m_value == rhs.m_value;
	}

	friend bool operator!=(const Value& lhs, const Value& rhs)
	{
		return !(lhs == rhs);
	}
};

class Object : public Traceable
{
public:
	using Closure = std::vector<Value>;

	struct NativeFunction
	{
		std::function<void()> m_cpp_function;
		const char* m_name;
		int m_arity;
	};

	struct DefinedFunction
	{
		BytecodeStream m_bytecode;
		Closure m_closure;
		int m_arity;
	};

private:
	std::variant<std::string, std::vector<Value>, NativeFunction, DefinedFunction> m_value;

public:

	template<typename T>
	static Object* AllocateObject(T&& value) { return new Object(std::forward<T>(value)); }

	inline std::string& GetString() const { return const_cast<std::string&>(std::get<std::string>(m_value)); }

	inline bool IsString() const { return std::holds_alternative<std::string>(m_value); }

	inline std::vector<Value>& GetArray() const { return const_cast<std::vector<Value>&>(std::get<std::vector<Value>>(m_value)); }

	inline bool IsArray() const { return std::holds_alternative<std::vector<Value>>(m_value); }

	inline NativeFunction& GetNativeFunction() const { return const_cast<NativeFunction&>(std::get<NativeFunction>(m_value)); }

	inline bool IsNativeFunction() const { return std::holds_alternative<NativeFunction>(m_value); }

	inline DefinedFunction& GetDefinedFunction() const { return const_cast<DefinedFunction&>(std::get<DefinedFunction>(m_value)); }

	inline bool IsDefinedFunction() const { return std::holds_alternative<DefinedFunction>(m_value); }

	inline std::string ToString() const
	{
		return std::visit([](auto&& arg) -> std::string
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, std::string>)
				{
					return arg;
				}
				else if constexpr (std::is_same_v<T, std::vector<Value>>)
				{
					std::string result = "[";
					for (const Value& value : arg)
					{
						result.append(value.ToString());
						result.append(",");
					}
					result.pop_back();
					result.append("]");
					return result;
				}
				else if constexpr (std::is_same_v<T, NativeFunction>)
				{
					return "<native function " + std::string(arg.m_name) + ">";
				}
				else if constexpr (std::is_same_v<T, DefinedFunction>)
				{
					return "<defined function at: "  + std::to_string(reinterpret_cast<uintptr_t>(&arg.m_bytecode)) + ">";
				}
				else
				{
					return "Unknown Object";	
				}
			}, m_value);
	}

private:
	Object() = default;

	Object(const Object& other) = default;

	Object(Object&& other) noexcept = default;

	Object(std::string&& str) : m_value(std::move(str)) {}

	Object(std::vector<Value>&& array) : m_value(std::move(array)) {}

	Object(NativeFunction&& native_function) : m_value(std::move(native_function)) {}

	Object(DefinedFunction&& defined_function) : m_value(std::move(defined_function)) {}
};
