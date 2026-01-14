<div align="center">
  <h1 style="color: #FA425EC9; border-bottom: none; margin-bottom: 0;">GLOW LANGUAGE TOOLCHAIN</h1>
  <p style="color: #777777C0; font-size: 12pt; margin-top: 0.25rem;">Hybrid, flexible programming language (early-stage).</p>
</div>

***

## <p>🛠 Technical Architecture</p>

The core of Glow is the **Abstract Syntax Tree (AST)**, which is decoupled from execution logic via the **Visitor Pattern**. This allows the toolchain to perform multiple passes (Diagnostics, Transpilation, VM Bytecode generation) without altering the underlying node structures.

### Execution Models
* **Bytecode VM**: A custom virtual machine executing on a `Value` stack, optimized for rapid testing and development.
* **C++ Transpiler**: Converts Glow AST into optimized C++ source code, leveraging a custom runtime for performance-critical deployment.

***

## <p>Current Language Capabilities</p>

- **Extended Type Palette (16-bit to 128-bit)**
- **Triple-Variant Loop Logic**
- **Unit-Based OOP & Inheritance**
- **Hybrid Execution (VM & Transpiler)**
- **Other Features (not verified yet)**

***

## <p>The Visitor Pattern Architecture</p>

The structural backbone of Glow is the **Visitor Pattern**. In a compiler context, this pattern is essential for decoupling the **data (AST)** from the **logic (backend)**.

### Why Glow uses the Visitor Pattern:
* **Separation of Concerns:** AST nodes are "passive" structures. They don't know how to execute; they only store data. The "Logic" lives entirely inside the Visitor.
* **The "Visit" Mechanism:** Instead of the nodes having logic, a `Visitor` (like the Transpiler) "visits" each node in the tree. The tree structure remains stable while the Visitor walks through it, performing specific actions at every stop.
* **Double Dispatch:** When a node `accepts` a visitor, it "calls back" to the visitor’s specific method (such as `visitLoop()`). This ensures the backend knows exactly which type of node it is dealing with without manual type-checking.
* **Extensibility:** You can add a Linter, Optimizer, or New Backend just by creating a new Visitor that "visits" the same tree in a different way, without ever touching your Parser or Nodes.
* **Hybrid Support:** This allows the **Virtual Machine** and **C++ Transpiler** to "visit" the exact same AST but interpret the nodes differently — generating bytecode, or C++ source.

<hr style="margin-top: 1.25rem;" />

## <p>Functional Features</p>

The following core systems have been fully implemented and verified through the `tests/` suite:

* **Integrated Control Flow:** support for `if/elif/else` branching and triple-variant `loop` logic (Standard `for`, conditional `while`, and collection `range`).
* **Unit-Based OOP:** support for `unit` structures, including inheritance (`from`), visibility modifiers (`pub`/`prot`/`priv`), and custom `init` lifecycles.
* **Systems Type Palette:** Native handling of 16-bit through **128-bit integers**, booleans, and strings.
* **Advanced Stream I/O:** stream-based input and output using `ostream <<` and `istream >>` with support for escape sequences and stream flushing.
* **String Interpolation:** support for dynamic string evaluation using **safe** `${}` and **unsafe** `$()` syntax.

***

## <p>Simplified Project Structure</p>

```bash
root/
├── include/           # Headers like Token.h, etc.
├── src/
│   ├── common/        # Buffer
│   ├── compiler/      # Lexer, Parser, and Transpiler
│   ├── core/          # Diagnostics and Error handling
│   ├── vm/            # Virtual Machine and Value system
│   └── include/       # Glow CXX Runtime Functions
└── tests/             # This is where testing is done
```

| Directory | Purpose |
| :- | :- |
| **`include/`** | Public API, Token definitions, and AST Node interfaces. |
| **`src/`** | Core implementation (Lexer, Parser, VM, Transpiler). |
| **`src/include/`** | Internal Standard Library Modules (`.glh`) and Runtime Logic. |
| **`tests/`** | Baseline `.glw` source files. |

<hr style="margin-top: 2rem;" />

<h2 style="margin: 0 0 2rem; padding: 1rem 0;">
  Build & Usage
</h2>

```bash
# Configure and Compile Project
cmake -B build -G Ninja
cmake --build build

# Inspection & Execution ('./' on UNIX, '.\' on Windows)
./glow ast tests/test.glw      # Visualize the tree structure (AST)
./glow run tests/test.glw      # Execute via the Bytecode VM (Interpreter)

# Compilation & Transpilation
./glow compile tests/test.glw        # Generate a native binary executable
./glow compile tests/test.glw --cpp  # Transpile to optimized C++ source
```

***

### <b style="color: #FFFF64E8;">Note:</b>
Currently **Glow** relies on **CXX**. To prevent inconsistencies and bad design, future iterations may switch to **C** or **Assembly** implementation for tighter control over program flow && performance.
