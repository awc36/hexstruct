# hexstruct
A program to interpret binary files using structs.

# How to compile
To compile it, run: 

gcc code.c -o exe

If you enable "usemath" in the code: 

gcc code.c -lm -o exe

# How to use it
## Command line Arguments

| Option     | Description                   |Default |
| ---------- | ----------------------------- | ------ |
| -e[h/l]    | sets the endian to high/low   | low    |
| -i[y/n]    | ignore bytes after end?       | no     |
| -v[y/n]    | set default for \"valueonly\" | no     |
| -s         | the path to the struct file   |        |
| -m[number] | the maximum size of an array must be below 1'000'000'000 | 0(no limit)

You need to give a struct path and a binary path (without prefix).

# How does the struct file work

For examples take a look in the structs folder.

You can use structs to build new types.

Variable definitions have the following syntax:

```
type name;
type name[expession];
type name[start][end];        //start:incl, end:excl
type name = expression;
type name[expression] = expression;
type name[start][end] = expression;
```

Struct definitions have the following syntax:
```
struct name 
{
    type1 name1;
    type2 name2 = 5;
    ...
};
```

The standart types are: i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, x8, x16, x32, x64, x128, f32, f64, c8, c16, c32, s8, s16, s32

The array types can be assigned a number for all variables or a string for s... types.

Expressions can have the following operands: +(unary&binary) ,-(unary&binary), *, / ,^(power, evaluation right to left, only when "usemath" is defined in the sourcecode), <, <=, ==, >, >=, &&, ||, ^^(xor), $(unary)

The supported expression types are int, float, string. You need to convert between them explicitly with the functions: int(...), float(...), char(...)

But you can't convert a char back to int.

The only other function is exp(...). It takes int and float.

The Stacktrace is a list of the current array indecies.

The stacktrace operator "$n" returns the n-th index in the stacktrace.

You can also use initialized local and global variables in your expressions.

To do that, just type the global path into the expression or use "this." to use a local variable.

To get the value of an array element, just type: name[expression]

To get the value of an s... array, just type: name

You can access the current file address in an expression with "here". (the address before the current variable)

To hide a variable the name must start with an underscore. But you can still use it in an expression.

Variables can have prefixes before the type name. Variable definitions then have the following syntax:
```
(prefix1=value, prefix2=value ,...) type name;
```

The following prefixes are available:
| Prefix   | Description                                                                                  |
| ------   | -----------                                                                                  |
| inline     | The values of a variable of type struct replace the variable. (But the paths aren't inline.) |
| location   | sets the absolute location of the variable. Doesn't affect the byte pointer               |
| simplify   | allow simplification of array names (see "How does the program work")                |
| endian     | Set the endian for the following variables. (including the current variable)             |
| value[...] | Set a string to be displayed when the value is \"...\".                   |
| valueonly | Output only the string defined by \"value\", when a string is defined.              |

Numbers can have the following prefixes (case insensitive):
| Prefix | base        |
| ------ | ----        |
| b      | binary      |
| o      | octal       |
| d      | duodecimal  |
| h      | hexadecimal |

# Notes
- "true" and "false" (case sensitive) just mean 1 and 0.

- int*str=(str repeated) in an expression

- Variables are initialized from top to bottom.

- You can set the parameters endian and ignore-end in the code with:#pa; (don't forget the ';')
Where p is e/i and a is h/l/y/n/number

- There isn't any error detection for overflow in the expressions. So be sure that the absolute value of any operation is in the bounds of i128 or f64 (the internal number formats).
Therefore u128, x128, c128 and s128 are treated as signed in expressions.

- You can enable caching in the source code so the programm reads chunks of data instead of bytes.

- You can adjust the tablen in the source code.

- You can comment with // and /**/ (like in c)

# Return values
| value | meaning              |
| ----- | -------              |
| 0     | success              |
| 1     | something went wrong |

# How does the program work
- The program reads the arguments, reads the struct file to "structstr" and opens a file stream to the binary file ("input").
- The comments are removed from "structstr". This is the only change made to "structstr" as it is used for printing the line in case of an error.
- "structstr" gets split at the semicolons into individual units. The list of splits is now in the variable "structlines".
- Each unit gets parsed and the result is stored in "prevars", "structs" or used to overwrite a command line argument.
- The prefixes are parsed in "setPrefixes".
- "addVar" is called to add every variable in "prevars" to "root" and read the data to the variables.
- Now the tree is printed.

"addVar" does the following:
- It parses the type with "parsetype" and the value (if given) with "eval" and "setValue".
- It creates a new entry in "this".
- If the type is an array type, it may create multiple entries. (or none)
- If the type is a struct type, it calls "addVar" on the variable to add.
- If the type is a struct array type it pushes the index to the "stacktrace".
- If the type is an array type but start=0 and end=1, it is treated as a non-array type. (unless simplify=false)
