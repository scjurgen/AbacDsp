# IDE setup

## clangd / static analysis

The repo includes a `.clangd` file that points clangd to `cmake-build-debug/compile_commands.json`,
so no symlink is needed. You only need to create that build directory once:

**Tests-only build** (no JUCE required):
```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
```

**Full build** (includes JUCE examples - required for metronome, reverb, etc.):
```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_FULL_PROJECT=ON ..
```

After configuring, restart your language server (or reopen the project). With the full build
the IDE will resolve all JUCE headers and the `AbacDsp` includes inside `examples/`.
