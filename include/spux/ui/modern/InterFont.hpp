// The embedded UI font: Inter, SIL Open Font License 1.1 (third_party/inter/LICENSE.txt), two static
// instances subset to Latin and the symbols the widgets use. The data is generated (tools/make_fonts.py)
// and lives in its own translation unit, so it is linked only into a binary that bakes the atlas.
#pragma once

namespace spux::inline v1::ui::modern {

extern const unsigned char kInterRegular[];
extern const unsigned long kInterRegularSize;
extern const unsigned char kInterSemiBold[];
extern const unsigned long kInterSemiBoldSize;

} // namespace spux::inline v1::ui::modern
