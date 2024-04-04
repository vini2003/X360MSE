

# How to compile

1. Replace `__cplusplus` with `_MSVC_LANG` if using MSVC for compiling (which you should, on Windows).
2. Use a Release build, otherwise you need to change the copy script in `je2be-core/CMakeLists.txt` to copy `mimalloc-debug` rather than `mimalloc`.
3. Remove `examples` from `pbar`, including in `CMakeLists.txt`.