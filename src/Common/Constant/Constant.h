// Midori Compiler Constants

#pragma once

#include <cinttypes>

inline constexpr int MAX_CAPTURED_COUNT{ UINT8_MAX };

inline constexpr int MAX_SIZE_OP_CONSTANT{ UINT8_MAX };

inline constexpr int MAX_SIZE_OP_CONSTANT_LONG{ UINT16_MAX };

inline constexpr int MAX_SIZE_OP_CONSTANT_LONG_LONG{ ((UINT16_MAX << 8) | 0xffff) };

inline constexpr int MAX_LOCAL_VARIABLES{ UINT8_MAX };

inline constexpr int MAX_JUMP_SIZE{ UINT16_MAX };

inline constexpr int MAX_FUNCTION_ARITY{ UINT8_MAX };

inline constexpr int MAX_FUNCTION_COUNT{ UINT8_MAX };

inline constexpr int MAX_UNION_TAG{ UINT8_MAX };

inline constexpr int MAX_ARRAY_SIZE{ ((UINT16_MAX << 8) | 0xffff) };

inline constexpr int MAX_NESTED_ARRAY_INDEX{ UINT8_MAX };