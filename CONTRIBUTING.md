# Contributing Guidelines

## C++
### Include Order
* STD includes
* External library includes
* Includes in a parent directory with `<>` (where otherwise you would need `../`)
* Local includes with `""`

### Naming rules
* Macro: `SCREAMING_SNAKE_CASE`
* Namespaces: `camelCase`
* Enum: `PascalCase`
* Enumerator: `PascalCase`
* Union: `PascalCase`
* Classes: `PascalCase` except for abbreviations such as OS and NCE
* Local Variable: `camelCase`
* Member Variables: `camelCase`
* Global Variables: `PascalCase`
* Functions: `PascalCase`
* Parameter: `camelCase`
* Files and Directories: `snake_case` except for when they correspond to a HOS structure (EG: Services, Kernel Objects)

### Comments:
Use doxygen style comments for:
* Classes/Structs - Use `/**` block comments with a brief
* Class/Struct Variables - Use `//!<` single-line comments on their utility 
* Class/Struct Functions - Use `/**` block comments on their function with a brief, all arguments and the return value (The brief can be skipped if the function's arguments and return value alone explain what the function does)
* Enumerations - Use a `/**` block comment with a brief for the enum itself and a `//!<` single-line comment for all the individual items

Notes: 
* The DeviceState object can be skipped from function argument documentation as well as class members in the constructor
* Any class members don't need to be redundantly documented in the constructor

### Control flow statements (if, for and while):
#### If a child control-flow statement has brackets, the parent statement must as well
* Correct
  ```cpp
  if (a) {
    while (b) {
      printf("A");
      printf("B");
      b = false;
    }
  }
  ```
* Incorrect
  ```cpp
  if (a)
    while (b) {
      printf("A");
      printf("B");
      b = false;
    }
  ```

#### If any cases of an if statement have curly brackets all must have curly brackets:
* Correct
  ```cpp
  if (a) {
    printf("a");
    a = false;
  } else {
    printf("none");
  }
  ```
* Incorrect
  ```cpp
  if (a) {
    printf("a");
    a = false;
  } else
    printf("none");
  ```

### Spacing
We generally follow the rule of **"Functional Spacing"**, that being spacing between chunks of code that do something functionally different while functionally similar blocks of code can be closer together.

Here are a few examples of this to help with intution:
* If variable declarations are small, they can have no spacing
  ```cpp
  auto a = 1;
  auto b = GetSomething();
  DoSomething(a, b);

  return;
  ```
* If variable declarations are large, spacing should follow them. This applies even more if error checking code needs to follow it.
  ```cpp
  auto a = GetSomethingA();
  auto b = GetSomethingB();
  auto c = GetSomethingC();

  auto result = DoSomething(a, b, c);
  if (!result)
    throw exception("DoSomething has failed: {}", result);
  ```
* If a function doesn't require multiple variable declarations, the function call should be right after the variable declaration
  ```cpp
  auto a = GetClassA();
  a.DoSomething();

  auto b = GetClassB();
  b.DoSomething();
  ```
* If a single variable is used by a control flow statement, there can be no spaces after it's declaration
  ```cpp
  auto a = GetClass();
  if (a.something) {
    for(const auto& item : a.items)
      DoSomething(item);
  }
  ```
* **Be consistent** (The above examples assume that `a`/`b`/`c` are correlated)
  * Inconsistent Disaster
    ```cpp
    auto a = GetClassA();
    a.DoSomething();

    auto b = GetClassB();
    auto c = GetClassC();

    b.DoSomething();
    c.DoSomething();
    ```
  * The Right Way:tm:
    ```cpp
    auto a = GetClassA();
    auto b = GetClassB();
    auto c = GetClassC();

    a.DoSomething();
    b.DoSomething();
    c.DoSomething();
    ```
**Readability is key at the end, skirt these rules if you think it increases readability**

### Lambda Usage
We generally support the usage of functional programming and lambda, usage of it for assigning conditional variables is recommended especially if it would otherwise be a nested ternary statement:
* With Lambda (Inlined function)
  ```cpp
  auto a = random();
  auto b = [a] {
    if (a > 1000)
      return 0;
    else if (a > 500)
      return 1;
    else if (a > 250)
      return 2;
    else
      return 3;
  }();
  ```
* With Ternary Operator
  ```cpp
  auto a = random();
  auto b = (a > 1000) ? 0 : ((a > 500) ? 1 : (a > 250 ? 2 : 3)); 
  ```

### References
For passing any parameter which isn't a primitive prefer to use references/const references to pass them into functions or other places as a copy can be avoided that way.  
In addition, always use a const reference rather than a normal reference unless the argument needs to be modified in-place as the compiler knows the intent far better in that case.
```cpp
void DoSomething(const Class& class, u32 primitive);
void ClassConstructor(const Class& class) : class(class) {} // Make a copy directly from a `const reference` for class member initialization
```

### Range-based Iterators
Use C++ range-based iterators for any C++ container iteration unless it can be performed better with functional programming (After C++20 when they are merged with the container). In addition, stick to using references/const references using them

### Usage of auto
Use `auto` unless a variable needs to be a specific type that isn't automatically deducible.
```cpp
u16 a = 20; // Only if `a` is required to specifically be 16-bit, otherwise integers should be auto
Handle b = 0x10; // Handle is a specific type that won't be automatically assigned
```

### Constants
If a variable is constant at compile time use `constexpr`, if it's only used in a local function then place it in the function but if it's used throughout a class then in the corresponding header add the variable to the `skyline::constant` namespace. If a constant is used throughout the codebase, add it to `common.h`.

In addition, try to `constexpr` as much as possible including constructors and functions so that they may be initialized at compile-time and have lesser runtime overhead during usage and certain values can be pre-calculated in advance.
