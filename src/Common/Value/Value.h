#pragma once

#include <functional>
#include <list>
#include <string>
#include <variant>
#include <vector>
#include <unordered_set>
#include <memory>

class MidoriTraceable;

class MidoriValue
{

public:
	using MidoriInteger = int64_t;
	using MidoriFraction = double;
	using MidoriUnit = std::monostate;
	using MidoriBool = bool;

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
	enum class MidoriValueTypeTag
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

	std::string ToString() const;
};

template<typename... Args>
concept MidoriValueConstructible = std::constructible_from<MidoriValue, Args...>;

template<typename T>
concept MidoriTraceableConstructible = std::constructible_from<MidoriTraceable, T>;

template <typename T>
concept MidoriNumeric = std::same_as<T, MidoriValue::MidoriFraction> || std::same_as<T, MidoriValue::MidoriInteger>;

class MidoriTraceable
{
public:
	using GarbageCollectionRoots = std::unordered_set<MidoriTraceable*>;

	static inline size_t s_total_bytes_allocated;
	static inline size_t s_static_bytes_allocated;
	static inline std::list<MidoriTraceable*> s_traceables;

	using MidoriText = std::string;
	using MidoriArray = std::vector<MidoriValue>;

	struct CellValue
	{
		MidoriValue m_heap_value;
		MidoriValue* m_stack_value_ref;
		bool m_is_on_heap = false;

		MidoriValue& GetValue();
	};

	struct MidoriClosure
	{
		using Environment = std::vector<MidoriValue>;

		Environment m_cell_values;
		int m_proc_index;
	};

	struct MidoriStruct
	{
		std::vector<MidoriValue> m_values;
	};

	struct MidoriUnion
	{
		std::vector<MidoriValue> m_values;
		int m_index;
	};

private:
	std::variant<MidoriText, MidoriArray, MidoriStruct, MidoriUnion, CellValue, MidoriClosure> m_value;
	size_t m_size;
	bool m_is_marked = false;

public:

	~MidoriTraceable() = default;

	MidoriText& GetText();

	bool IsText() const;

	MidoriArray& GetArray();

	bool IsArray() const;

	bool IsCellValue() const;

	CellValue& GetCellValue();

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

	std::string ToString() const;

	void Trace();

	static void CleanUp();

	static void PrintMemoryTelemetry();

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

	MidoriTraceable(MidoriValue* stack_value_ref) noexcept;

	MidoriTraceable(MidoriClosure&& closure) noexcept;

	MidoriTraceable(MidoriStruct&& midori_struct) noexcept;

	MidoriTraceable(MidoriUnion&& midori_union) noexcept;
};