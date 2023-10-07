#pragma once

#include <functional>
#include <string>
#include <variant>
#include <vector>
#include <memory>

#define THREE_BYTE_MAX 16777215

class ExecutableModule;
class Object;

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

	constexpr Value() : m_value(std::monostate()) {}

	constexpr Value(double d) : m_value(d) {}

	constexpr Value(bool b) : m_value(b) {}

	constexpr Value(Object* o) : m_value(o) {}

	constexpr inline double GetNumber() const { return std::get<double>(m_value); }

	constexpr inline bool IsNumber() const { return std::holds_alternative<double>(m_value); }

	constexpr inline std::monostate GetNull() const { return std::get<std::monostate>(m_value); }

	constexpr inline bool IsNull() const { return std::holds_alternative<std::monostate>(m_value); }

	constexpr inline bool GetBool() const { return std::get<bool>(m_value); }

	constexpr inline bool IsBool() const { return std::holds_alternative<bool>(m_value); }

	constexpr inline Object* GetObjectPointer() const { return std::get<Object*>(m_value); }

	constexpr inline bool IsObjectPointer() const { return std::holds_alternative<Object*>(m_value); }

	constexpr inline static bool AreSameType(const Value& lhs, const Value& rhs) { return lhs.m_value.index() == rhs.m_value.index(); }

	constexpr inline std::string ToString() const
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

class Object
{
public:
	using Closure = std::vector<std::shared_ptr<Value>>;

	struct NativeFunction
	{
		std::function<void()> m_cpp_function;
		const char* m_name;
		int m_arity;
	};

	struct DefinedFunction
	{
		std::unique_ptr<ExecutableModule> m_module;
		Closure m_closure;
		int m_arity;
	};

private:
	std::variant<std::string, std::vector<Value>, NativeFunction, DefinedFunction> m_value;

public:

	constexpr Object(std::string&& str) : m_value(std::move(str)) {}

	constexpr Object(std::vector<Value>&& array) : m_value(std::move(array)) {}

	constexpr Object(NativeFunction&& native_function) : m_value(std::move(native_function)) {}

	constexpr Object(DefinedFunction&& defined_function) : m_value(std::move(defined_function)) {}

	constexpr inline std::string& GetString() const { return const_cast<std::string&>(std::get<std::string>(m_value)); }

	constexpr inline bool IsString() const { return std::holds_alternative<std::string>(m_value); }

	constexpr inline std::vector<Value>& GetArray() const { return const_cast<std::vector<Value>&>(std::get<std::vector<Value>>(m_value)); }

	constexpr inline bool IsArray() const { return std::holds_alternative<std::vector<Value>>(m_value); }

	constexpr inline NativeFunction& GetNativeFunction() const { return const_cast<NativeFunction&>(std::get<NativeFunction>(m_value)); }

	constexpr inline bool IsNativeFunction() const { return std::holds_alternative<NativeFunction>(m_value); }

	constexpr inline DefinedFunction& GetDefinedFunction() const { return const_cast<DefinedFunction&>(std::get<DefinedFunction>(m_value)); }

	constexpr inline bool IsDefinedFunction() const { return std::holds_alternative<DefinedFunction>(m_value); }

	constexpr inline std::string ToString() const
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
					return "<defined function at: "  + std::to_string(reinterpret_cast<uintptr_t>(arg.m_module.get())) + ">";
				}
				else
				{
					return "Unknown Object";	// Should never happen
				}
			}, m_value);
	}
};
