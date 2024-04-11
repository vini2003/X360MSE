![X60MSE Logo](https://github.com/vini2003/X360MSE/blob/master/assets/logo.svg)

**Xbox 360 Minecraft Save Extractor** (X360MSE) is a command-line utility for extracting old Minecraft saves from Xbox 360 hard drive data.

## Features

- Extract saves from compressed archives (`.zip`, `.rar`, `.7z` and many more)!
- Extract saves from folders (eg. `X:\` if you mount the `Content/` partition there)!
- Convert saves from **Xbox 360** `.bin` files to **Java Edition** saves!

## Usage

Pre-compiled binaries can be found in the **Releases** tab.

X360MSE may be used by invoking `X360MSE.exe` in the terminal with the argument `-i <input_dir/input_file/input_archive>` and `-o <output_dir>`.

### Examples

- `.\X360MSE.exe -i "X:\Content" -o ".\Converted-Saves"` will copy all saves from `X:\Content` into `.\Converted-Saves` and run the conversion algorithm on them.
- `.\X360MSE.exe -i "X:\Content.7z" -o ".\Converted-Saves"` will extract all saves from `X:\Content.7z` into `.\Converted-Saves` and run the conversion algorithm on them.



## Acknowledgements

This work was only possible thanks to [bit7z](https://github.com/rikyoz/bit7z/) by [rikyoz](https://github.com/rikyoz) and [je2be-core](https://github.com/kbinani/je2be-core/) by [kbinani](https://github.com/kbinani)!

A huge thanks also goes to the [7-zip](https://www.7-zip.org/) project, whose `7z.dll` is distributed in `dll/7z.dll`. 

**7-zip is licensed under the GNU LGPL license and can be found at https://www.7-zip.org/.** 

# Compiling

The project should compile out of the box using CMake. However, there are issues with the provided project's `CMakeList.txt`s and with MSVC itself, which require minor tweaks to get this project to compile.

1. Use a Release build, otherwise you need to change the copy script in `je2be-core/CMakeLists.txt` to copy `mimalloc-debug` rather than `mimalloc`.
2. `//      return JE2BE_ERROR;`