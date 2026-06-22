# GDS Development Guidelines

GDS is a waveform generator, where the lower-level machine uses STM32 and outputs waveforms through DAC. The upper-level machine has designed both Matlab and Python methods for use and debugging. For details, please refer to following.
```
./1.Firmware/user/app/WGS/README.md,
./2.TestForHost/wgs_matlab/README.md,
./2.TestForHost/wgs_python/README.md
```

## Platform And Toolchain

- C++23 / C11
- `arm-none-eabi`
- FreeRTOS
- STM32 HAL

## Build

```bash
cmake -B build -G Ninja
```

## Compile

```bash
ninja -C build
```

CMake glob automatically includes all `.cpp, .cc, and .hpp` files under the `user/` directory

## Coding Standards

### Naming Rules

| Category | Style | Examples |
|---|---|---|
| Class | PascalCase | `WGS`, `OutputController` |
| Enum type | PascalCase + `_e` | `RegId_e`, `ErrorCode_e` |
| Struct type | PascalCase + `_s` | `Empty_s`, `ProtoData_s` |
| Union type, C style | PascalCase + `_u` | `Pids_u`, `Motors_u` |
| Union type, C++ style | PascalCase + `_v` | `Command_v` |
| Enum member | UPPER_SNAKE_CASE | `DM_REG_UV_VALUE`, `NO_ERROR` |
| Namespace | UPPER_SNAKE_CASE | `GDS_N`, `INS_N`, `LED` |
| Function / variable | camelCase | `enable()`, `bspInit()`, `createApp()` |
| Private class member | camelCase + trailing `_` | `tasks_`, `insDat_` |
| Function parameter | leading `_` + camelCase | `_flag`, `_ctx` |

### Abbreviations

Except for UPPER_SNAKE_CASE names, abbreviations with exactly three letters should stay fully uppercase. Other abbreviations should be treated as normal words: capitalize the first letter and keep the rest lowercase.

### Keyword Conflicts

If an identifier conflicts with a C++ keyword, append an underscore:

```cpp
float class_ = 0; // `class` is a C++ keyword
```

## Commit Convention

Use the Commitizen format:

```text
<type>[scope]: <description>
```

Example:

```text
doc(USB): add README.md
```

Do not add `Co-Authored-By` trailers or AI tool signatures.