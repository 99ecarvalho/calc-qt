# Calculator for Linux

This folder ports Windows Calculator to Linux.

The math comes from the upstream engine, `src/CalcManager`. It is built as is. No upstream file is changed.
The Windows interface (UWP, XAML and C#) does not run on Linux. It is replaced by a Qt 6 Widgets interface in `app/`.

| Standard | Scientific, with history | Programmer |
|---|---|---|
| ![Standard](docs/standard.png) | ![Scientific](docs/scientific.png) | ![Programmer](docs/programmer.png) |

## What works

- Standard, Scientific and Programmer modes.
- Arbitrary precision math from the original engine (Ratpack). So `0.1 + 0.2` gives `0.3`.
- Scientific: DEG, RAD and GRAD. F-E notation. 2nd functions. Trigonometry and Function flyouts.
- Programmer: HEX, DEC, OCT and BIN at once. QWORD to BYTE. Bitwise operators. Four bit shift modes. A bit toggling keypad.
- History. Click an entry to continue from it.
- Memory: MS, MR, M+, M−, MC, per item too.
- Copy and paste (Ctrl+C, Ctrl+V).
- Keyboard shortcuts close to the Windows app.
- Light and dark theme, following the system or set by hand.
- Keep on top.
- Engine messages in every language the upstream ships. Number separators follow your locale.

## What is not ported

- Date calculation, unit and currency converters, graphing. They live in the C# view models and in Windows-only libraries.

## Build

You need CMake 3.21+, a C++17 compiler and Qt 6.5+.

On Debian or Ubuntu:

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev
```

Build from the repository root:

```bash
cmake -S src/linux -B build-linux -G Ninja
```

```bash
cmake --build build-linux
```

Run it:

```bash
./build-linux/ms-calculator
```

Run the engine tests:

```bash
ctest --test-dir build-linux --output-on-failure
```

Install for your user (adds a menu entry):

```bash
cmake --install build-linux --prefix ~/.local
```

## Keyboard

| Key | Action |
|---|---|
| 0-9, + - * / | Digits and operators |
| Enter or = | Equals |
| Esc / Delete / Backspace | C / CE / ⌫ |
| F9 | ± |
| % | Percent (Programmer: mod) |
| r, q, @ | 1/x, x², √x |
| Ctrl+M, R, P, Q, L | MS, MR, M+, M−, MC |
| Ctrl+H | History |
| Ctrl+Shift+D | Clear history |
| Alt+1, 2, 3 | Standard, Scientific, Programmer |
| Scientific: s o t, S O T | sin cos tan, and their inverses |
| Scientific: ^ or y, #, !, n, l, p, e, x, v | xʸ, x³, n!, ln, log, π, e, exp, F-E |
| Scientific: F3, F4, F5 | DEG, RAD, GRAD |
| Programmer: A-F | Hex digits |
| Programmer: F5, F6, F7, F8 | HEX, DEC, OCT, BIN |
| Programmer: < > & \| ~ ^ | Shift, AND, OR, NOT, XOR |

## Layout

- `CMakeLists.txt`: builds CalcManager as a static library, the app and the tests.
- `compat/`: stand-ins for three Windows SDK headers, and a force-included header with standard includes MSVC adds implicitly.
- `app/CalcBridge`: implements the engine display callback (`ICalcDisplay`) and turns it into Qt signals. It ports the view model logic for modes, radix and history replay.
- `app/EngineResources`: reads `CEngineStrings.resw` from every locale, embedded as Qt resources.
- `app/MainWindow`: the interface.
- `tests/engine_tests.cpp`: drives the engine headless and checks results.

The app also takes `--type <keys>` and `--screenshot <file>`. They help with tests and docs.

## License

MIT, like the upstream project. See `LICENSE` at the repository root.
