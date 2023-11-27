#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"

struct PrintAbstractSyntaxTree
{
	void PrintWithIndentation(int depth, std::string_view text) const;

	void operator()(const Block& block, int depth = 0) const;

	void operator()(const Simple& simple, int depth = 0) const;

	void operator()(const Define& def, int depth = 0) const;

	void operator()(const If& if_stmt, int depth = 0) const;

	void operator()(const While& while_stmt, int depth = 0) const;

	void operator()(const For& for_stmt, int depth = 0) const;

	void operator()(const Break&, int depth = 0) const;

	void operator()(const Continue&, int depth = 0) const;

	void operator()(const Return& return_stmt, int depth = 0) const;

	void operator()(const Struct& struct_stmt, int depth = 0) const;

	void operator()(const Binary& binary, int depth = 0) const;

	void operator()(const Group& group, int depth = 0) const;

	void operator()(const Unary& unary, int depth = 0) const;

	void operator()(const Call& call, int depth = 0) const;

	void operator()(const Get& get, int depth = 0) const;

	void operator()(const Set& set, int depth = 0) const;

	void operator()(const Variable& variable, int depth = 0) const;

	void operator()(const Bind& bind, int depth = 0) const;

	void operator()(const TextLiteral& text, int depth = 0) const;

	void operator()(const BoolLiteral& bool_val, int depth = 0) const;

	void operator()(const FractionLiteral& fraction, int depth = 0) const;

	void operator()(const IntegerLiteral& integer, int depth = 0) const;

	void operator()(const UnitLiteral&, int depth = 0) const;

	void operator()(const Closure& closure, int depth = 0) const;

	void operator()(const Construct& construct, int depth = 0) const;

	void operator()(const Array& array, int depth = 0) const;

	void operator()(const ArrayGet& array_get, int depth = 0) const;

	void operator()(const ArraySet& array_set, int depth = 0) const;

	void operator()(const Ternary& ternary, int depth = 0) const;
};
