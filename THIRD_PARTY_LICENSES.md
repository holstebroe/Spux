# Third-party licenses

spux is MIT (see `LICENSE`). The parts below keep their own license, in their own folder. A plugin
binary that includes one of them must ship the notice listed.

| Part | Folder | License | A shipped plugin must include |
| --- | --- | --- | --- |
| pt2-clone minimum-phase BLEP table and routine (Olav Sørensen; BLEP by aciddose) | `include/spux/dsp/pt2/` | BSD 3-Clause | The copyright notice, the conditions and the disclaimer of `include/spux/dsp/pt2/LICENSE`, in its documentation or other materials, when it uses `spux/dsp/pt2/MinBlep.hpp` |

Submodules under `third_party/` (clap, clap-wrapper, doctest) carry their own license files; they are
build and test dependencies, and a plugin follows their terms as it does today.
