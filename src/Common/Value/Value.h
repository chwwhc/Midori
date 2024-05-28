#pragma once

#include <functional>
#include <list>
#include <variant>
#include <unordered_set>

class MidoriTraceable;
class MidoriText;

using MidoriInteger = int64_t;
using MidoriFraction = double;
using MidoriUnit = std::monostate;
using MidoriBool = bool;

class MidoriValue
{
private:
	union MidoriValueUnion
	{
		MidoriFraction m_fraction;
		MidoriInteger m_integer;
		MidoriUnit m_unit;
		MidoriBool m_bool;
		MidoriTraceable* m_pointer;

		MidoriValueUnion(MidoriFraction d) noexcept;

		MidoriValueUnion(MidoriInteger l) noexcept;

		MidoriValueUnion(MidoriUnit u) noexcept;

		MidoriValueUnion(MidoriBool b) noexcept;

		MidoriValueUnion(MidoriTraceable* o) noexcept;
	};
	enum MidoriValueTypeTag : uint8_t
	{
		Fraction,
		Integer,
		Unit,
		Bool,
		Pointer
	};

	MidoriValueUnion m_value{ MidoriUnit{} };
	MidoriValueTypeTag m_type_tag{ MidoriValueTypeTag::Unit };

public:

	MidoriValue() noexcept = default;

	MidoriValue(MidoriFraction d) noexcept;

	MidoriValue(MidoriInteger l) noexcept;

	MidoriValue(MidoriBool b) noexcept;

	MidoriValue(MidoriTraceable* o) noexcept;

	MidoriValue(const MidoriValue& other) noexcept = default;

	MidoriValue(MidoriValue&& other) noexcept = default;

	MidoriValue& operator=(const MidoriValue& other) noexcept = default;

	MidoriValue& operator=(MidoriValue&& other) noexcept = default;

	MidoriFraction GetFraction() const;

	bool IsFraction() const;

	MidoriInteger GetInteger() const;

	bool IsInteger() const;

	MidoriUnit GetUnit() const;

	bool IsUnit() const;

	MidoriBool GetBool() const;

	bool IsBool() const;

	MidoriTraceable* GetPointer() const;

	bool IsPointer() const;

	MidoriText ToText() const;
};

template<typename... Args>
concept MidoriValueConstructible = std::constructible_from<MidoriValue, Args...>;

template<typename T>
concept MidoriTraceableConstructible = std::constructible_from<MidoriTraceable, T>;

template <typename T>
concept MidoriNumeric = std::same_as<T, MidoriFraction> || std::same_as<T, MidoriInteger>;

class MidoriText
{
private:
	char* m_data{ nullptr };
	int m_size{ 0 };
	int m_capacity{ 0 };

public:
	MidoriText() = default;

	MidoriText(const char* str);

	MidoriText(const MidoriText& other);

	MidoriText(MidoriText&& other) noexcept;

	MidoriText& operator=(const MidoriText& other);

	MidoriText& operator=(MidoriText&& other) noexcept;

	~MidoriText();

	int GetLength() const noexcept;

	const char* GetCString() const noexcept;

	MidoriText& Pop();

	MidoriText& Append(const char* str);

	MidoriText& Append(char c);

	MidoriText& Append(const MidoriText& other);

	char operator[](int index) const;

	bool operator==(const MidoriText& other) const;

	bool operator!=(const MidoriText& other) const;

	MidoriInteger ToInteger() const;

	MidoriFraction ToFraction() const;

	static MidoriText FromInteger(MidoriInteger value);

	static MidoriText FromFraction(MidoriFraction value);

	static MidoriText Concatenate(const MidoriText& a, const MidoriText& b);

private:
	MidoriText(int size);

	void Expand(int new_size);
};

class MidoriArray
{
private:
	inline static constexpr int s_initial_capacity = 8;
	MidoriValue* m_data{ nullptr };
	int m_size{ 0 };
	int m_end{ 0 };

public:
	MidoriArray() = default;

	MidoriArray(int size);

	MidoriArray(const MidoriArray& other);

	MidoriArray(MidoriArray&& other) noexcept;

	MidoriArray& operator=(const MidoriArray& other);

	MidoriArray& operator=(MidoriArray&& other) noexcept;

	~MidoriArray();

	MidoriValue& operator[](int index);

	void Add(const MidoriValue& value);

	int GetLength() const;

	static MidoriArray Concatenate(const MidoriArray& a, const MidoriArray& b);

private:
	void Expand();
};

struct MidoriCellValue
{
	MidoriValue m_heap_value;
	MidoriValue* m_stack_value_ref;
	bool m_is_on_heap = false;

	MidoriValue& GetValue();
};

struct MidoriClosure
{
	using Environment = MidoriArray;

	Environment m_cell_values;
	int m_proc_index;
};

struct MidoriStruct
{
	MidoriArray m_values{};
};

struct MidoriUnion
{
	MidoriArray m_values{};
	int m_index{ 0 };
};

class MidoriTraceable
{
public:
	// Garbage collection utilities
	using GarbageCollectionRoots = std::unordered_set<MidoriTraceable*>;
	static inline size_t s_total_bytes_allocated;
	static inline size_t s_static_bytes_allocated;
	static inline std::list<MidoriTraceable*> s_traceables;

private:
	std::variant<MidoriText, MidoriArray, MidoriStruct, MidoriUnion, MidoriCellValue, MidoriClosure> m_value;
	size_t m_size;
	bool m_is_marked = false;

public:

	~MidoriTraceable() = default;

	MidoriText& GetText();

	bool IsText() const;

	MidoriArray& GetArray();

	bool IsArray() const;

	bool IsCellValue() const;

	MidoriCellValue& GetCellValue();

	bool IsClosure() const;

	MidoriClosure& GetClosure();

	bool IsStruct() const;

	MidoriStruct& GetStruct();

	bool IsUnion() const;

	MidoriUnion& GetUnion();

	size_t GetSize() const;

	void Mark();

	void Unmark();

	bool Marked() const;

	static void* operator new(size_t size) noexcept;

	static void operator delete(void* object, size_t size) noexcept;

	MidoriText ToText();

	void Trace();

	template<typename T>
	static MidoriTraceable* AllocateTraceable(T&& arg)
	{
		return new MidoriTraceable(std::forward<T>(arg));
	}

private:
	MidoriTraceable() = delete;

	MidoriTraceable(const MidoriTraceable& other) = delete;

	MidoriTraceable(MidoriTraceable&& other) noexcept = delete;

	MidoriTraceable& operator=(const MidoriTraceable& other) = delete;

	MidoriTraceable& operator=(MidoriTraceable&& other) noexcept = delete;

	MidoriTraceable(MidoriText&& str) noexcept;

	MidoriTraceable(MidoriArray&& array) noexcept;

	MidoriTraceable(MidoriCellValue&& cell_value) noexcept;

	MidoriTraceable(MidoriClosure&& closure) noexcept;

	MidoriTraceable(MidoriStruct&& midori_struct) noexcept;

	MidoriTraceable(MidoriUnion&& midori_union) noexcept;
};