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
	std::variant<MidoriFraction, MidoriInteger, MidoriUnit, MidoriBool, MidoriTraceable*> m_value;

public:

	MidoriValue() noexcept : m_value(MidoriUnit()) {}

	MidoriValue(MidoriFraction d) noexcept : m_value(d) {}

	MidoriValue(MidoriInteger l) noexcept : m_value(l) {}

	MidoriValue(MidoriBool b) noexcept : m_value(b) {}

	MidoriValue(MidoriTraceable* o) noexcept : m_value(o) {}

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

	inline friend bool operator==(const MidoriValue& lhs, const MidoriValue& rhs)
	{
		return lhs.m_value == rhs.m_value;
	}

	inline friend bool operator!=(const MidoriValue& lhs, const MidoriValue& rhs)
	{
		return !(lhs == rhs);
	}

	std::string ToString() const;
};

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

	template<typename T>
	static MidoriTraceable* AllocateTraceable(T&& value)
	{
		return new MidoriTraceable(std::forward<T>(value));
	}

	MidoriText& GetText() const;

	bool IsText() const;

	MidoriArray& GetArray() const;

	bool IsArray() const;

	inline bool IsCellValue() const;

	CellValue& GetCellValue() const;

	bool IsClosure() const;

	MidoriClosure& GetClosure() const;

	bool IsStruct() const;

	MidoriStruct& GetStruct() const;

	bool IsUnion() const;

	MidoriUnion& GetUnion() const;

	size_t GetSize() const;

	void Mark();

	void Unmark();

	bool Marked() const;

	static  void* operator new(size_t size) noexcept;

	static  void operator delete(void* object, size_t size) noexcept;

	std::string ToString() const;

	void Trace();

	static void CleanUp();

	static void PrintMemoryTelemetry();

private:
	MidoriTraceable() = delete;

	MidoriTraceable(const MidoriTraceable& other) = delete;

	MidoriTraceable(MidoriTraceable&& other) noexcept = delete;

	MidoriTraceable& operator=(const MidoriTraceable& other) = delete;

	MidoriTraceable& operator=(MidoriTraceable&& other) noexcept = delete;

	MidoriTraceable(MidoriText&& str) : m_value(std::move(str)) {}

	MidoriTraceable(MidoriArray&& array) : m_value(std::move(array)) {}

	MidoriTraceable(MidoriValue* stack_value_ref) : m_value(CellValue{ MidoriValue(), stack_value_ref, false }) {}

	MidoriTraceable(MidoriClosure&& closure) : m_value(std::move(closure)) {}

	MidoriTraceable(MidoriStruct&& midori_struct) : m_value(std::move(midori_struct)) {}

	MidoriTraceable(MidoriUnion&& midori_union) : m_value(std::move(midori_union)) {}
};
