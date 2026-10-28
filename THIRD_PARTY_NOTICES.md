# Third-party notices

TinyRadial includes **JSON for Modern C++ 3.11.3**, by Niels Lohmann and contributors, licensed under the MIT license. The full notice is preserved in `third_party/nlohmann/LICENSE.MIT` in source and reproduced below for binary distributions.

Source: https://github.com/nlohmann/json/tree/v3.11.3

Vendored `json.hpp` SHA-256: `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`.

```text
MIT License

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Win32, Direct2D and DirectWrite are provided by Windows; no rendering engine is bundled. A MinGW-built test package additionally includes statically linked GCC/MinGW runtime code under their respective runtime exceptions; see `docs/DEPENDENCIES.md` for details. The documented Windows release toolchain is MSVC.
