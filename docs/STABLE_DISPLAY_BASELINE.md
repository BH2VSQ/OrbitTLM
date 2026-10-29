# Stable display baseline restoration

This source tree restores the top-level page construction and the original custom
`AutoWrapTabWidget` from the user-verified `OrbitTLM-protocol-segfault-fix.zip`
baseline. Later native nested-tab conversion, opaque-page flags, and forced repaint
loops are intentionally not included. The current i18n dictionaries and validators
are retained and compiled into the UI target as Qt resources. The raw KISS editor
remains a direct page of the original nested `QTabWidget`; the code only refreshes
its content while that page is active and never calls `show()`/`hide()` on it.

Build and test with the project's supported Qt 6 environment:

```sh
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```
