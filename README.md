# ¥ß¥É¥ê (MIDORI)

## Introduction

Midori is a general-purpose programming language. It features static typing and garbage collection. The Midori compiler is written in C++ and compiles to bytecode for the Midori Virtual Machine (MVM). 
The MVM is written in C++ and is responsible for executing the bytecode. The MVM is also responsible for garbage collection.
<br><br>
This project is still in early development. The language is not yet *that* usable.

## Example programs
### A bit of everything
```
foreign Print : (Text) -> Unit;

fixed PrintLine := closure(fixed val : Text) : Unit
{
	Print(val);
	Print("\n");
	return #;
};

struct Complex
{
	real : Fraction;
	img : Fraction;
}

fixed main := closure() : Unit
{
	fixed fib := closure(fixed x : Integer) : Integer
	{
		return x < 2 ? x : fib(x - 1) + fib(x - 2);
	};

	fixed factorial := closure(fixed x : Fraction) : Fraction
	{
		return x <= 1.0 ? 1.0 : factorial(x - 1.0) * x;
	};

	fixed dot_product := closure(fixed n : Integer, fixed a : Array<Fraction>, fixed b : Array<Fraction>) : Fraction
	{
		var output := 0.0;

		for (var i := 0; i < n; i = i + 1)
		{
			output = output + a[i] * b[i];
		}

		return output;
	};

	Print("Calculate the 35th Fibonacci number: ");
	PrintLine(fib(35) as Text);

	fixed complex := new Complex(1.1, 2.2);
	Print("I just created a complex number: ");
	PrintLine(complex as Text);

	Print("Calculate dot product: ");
	PrintLine(dot_product(3, [1.0, 2.0, 3.0], [1.0, 2.0, 3.0]) as Text);

	return #;
};
```
### Man or boy test
```
fixed x := closure(fixed x : Integer) : () -> Integer
{
    return closure() : Integer { return x; };
};

fixed A := closure(var k : Integer, fixed x1 : () -> Integer, fixed x2 : () -> Integer, 
        fixed x3 : () -> Integer, fixed x4 : () -> Integer, fixed x5 : () -> Integer) : Integer
{
    fixed B := closure() : Integer
    {
        k = k - 1;
        return A(k, B, x1, x2, x3, x4); 
    };
    return k > 0 ? B() : x4() + x5();
};

fixed main := closure() : Integer
{
    PrintLine(A(10, x(1), x(-1), x(-1), x(1), x(0)));   

    return 0;
};
```
