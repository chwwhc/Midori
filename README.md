# ¾v (MIDORI)

## Introduction

Midori is a general-purpose programming language. It features static typing and garbage collection. The Midori compiler is written in C++ and compiles to bytecode for the Midori Virtual Machine (MVM). 
The MVM is written in C++ and is responsible for executing the bytecode. The MVM is also responsible for garbage collection.
<br><br>
This project is still in early development. The language is not yet usable.

## Example programs
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
