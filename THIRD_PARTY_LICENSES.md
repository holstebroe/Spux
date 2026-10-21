# Third-party licenses

spux is MIT (see `LICENSE`). The parts below keep their own license, in their own folder. A plugin
binary that includes one of them must ship the notice listed.

| Part | Folder | License | A shipped plugin must include |
| --- | --- | --- | --- |
| pt2-clone minimum-phase BLEP table and routine (Olav Sørensen; BLEP by aciddose) | `include/spux/dsp/pt2/` | BSD 3-Clause | The copyright notice, the conditions and the disclaimer of `include/spux/dsp/pt2/LICENSE`, in its documentation or other materials, when it uses `spux/dsp/pt2/MinBlep.hpp` |
| pt2-clone 8x8 tracker font (Olav Sørensen), as a byte table in `src/ui/retro/pt2/FontPt2.cpp` | `src/ui/retro/pt2/` | BSD 3-Clause | The copyright notice, the conditions and the disclaimer of `src/ui/retro/pt2/LICENSE`, in its documentation or other materials, when it calls `fontPt2()` or `RetroConfig::tracker()` (the font is its own object, linked only when referenced) |
| Inter font (Rasmus Andersson), two static instances subset to Latin, as byte arrays in `src/ui/modern/InterFont.cpp` | `third_party/inter/` | SIL Open Font License 1.1 | The copyright line and the license text of `third_party/inter/LICENSE.txt`, when the binary links the Modern flavor (the font data is its own object, linked only when the atlas is baked) |
| stb_truetype v1.26 (Sean Barrett), bakes the font atlas | `third_party/stb/` | MIT or public domain (choose) | Nothing beyond the MIT notice at the end of `stb_truetype.h` if the MIT option is taken; compiled into `FontAtlas.cpp` only |

Submodules under `third_party/` (clap, clap-wrapper, doctest) carry their own license files; they are
build and test dependencies, and a plugin follows their terms as it does today.
