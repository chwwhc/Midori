# ミドリ (MIDORI)

## Introduction

Midori is a general-purpose programming language. It features static typing and garbage collection. The Midori compiler is written in C++ and compiles to bytecode for the Midori Virtual Machine (MVM). 
The MVM is written in C++ and is responsible for executing the bytecode. The MVM is also responsible for garbage collection.
<br><br>
This project is still in early development. The language is not yet *that* usable.

## Examples
### Product Type
```
struct Vec3
{
	x : Frac,
	y : Frac,
	z : Frac
};

struct Complex
{
	real : Frac,
	img : Frac,
};
```
### Church Numeral
```
fixed zero = fn(fixed f : (Int, Int) -> Int, fixed x : Int) : Int
{
    return x;
};

fixed one = fn(fixed f : (Int, Int) -> Int, fixed x : Int) : Int
{
    return f(x, 1); 
};

fixed two = fn(fixed f : (Int, Int) -> Int, fixed x : Int) : Int
{
    return f(f(x, 1), 1); 
};

fixed add = fn(fixed n : ((Int, Int) -> Int, Int) -> Int, 
                fixed m : ((Int, Int) -> Int, Int) -> Int) 
              : ((Int, Int) -> Int, Int) -> Int
{
    return fn(fixed f : (Int, Int) -> Int, fixed x : Int) : Int
    {
        return n(f, m(f, x));
    };
};

fixed church_to_number = fn(fixed c : ((Int, Int) -> Int, Int) -> Int) : Int
{
    return c(fn(fixed x : Int, fixed y : Int) : Int { return x + y; }, 0);
};


fixed main = fn() : Unit
{
	PrintLine(church_to_number(add(one, two)) as Text); // prints out "3"
	return ();
};
```
### Man or boy test
```
fixed x = fn(fixed x : Int) : () -> Int
{
    return fn() : Int { return x; };
};

fixed A = fn(var k : Int, fixed x1 : () -> Int, fixed x2 : () -> Int, 
        fixed x3 : () -> Int, fixed x4 : () -> Int, fixed x5 : () -> Int) : Int
{
    fixed B = fn() : Int
    {
        k = k - 1;
        return A(k, B, x1, x2, x3, x4); 
    };
    return k > 0 ? B() : x4() + x5();
};

fixed main = fn() : Int
{
    PrintLine(A(10, x(1), x(-1), x(-1), x(1), x(0)) as Text);   

    return 0;
};
```
