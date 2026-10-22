// A compact preset bar for the editor header: [<] [ name * ] [>] [SAVE]. Built from the standard widgets (arrow
// buttons, a display button, a list popup), driven by the host's PresetHost (EditorHost.hpp).
//
//   auto r = spux::ui::presetBar(gui, "presets", {220, 3, 330, 16});
//   if (r.save) { /* the plugin asks for a name and calls presets()->save(...) */ }
//
// The arrows load the previous and next preset of the factory list followed by the user list (wrapping); the name
// opens the whole list (with byCategory, a list of the categories whose submenus hold their presets, the row of the
// current preset marked; user presets join a factory category of the same name and the factory/user separator is not
// drawn). A preset that was changed since loading shows a star after its name. Without a PresetHost the bar draws
// nothing. Saving needs a name, which is the plugin's page (a text field); the bar only reports that SAVE was pressed.
#pragma once

#include <string>
#include <vector>

#include "spux/ui/Gui.hpp"

namespace spux::inline v1::ui {

struct PresetBarOptions {
    bool save = true; // show the SAVE button
    const char* saveLabel = "SAVE";
    int arrowWidth = 18;
    int saveWidth = 44;
    int gap = 8; // room for the shadows of the buttons
    // The list is one row per category (PresetHost::Entry::category, in order of first appearance) with the presets
    // in its submenu; a preset without a category is a row of its own. Off: one flat list.
    bool byCategory = false;
};

struct PresetBarResult {
    bool save = false;   // SAVE was pressed
    bool loaded = false; // a preset was loaded this frame (an arrow or a pick)
};

template <class Style>
PresetBarResult presetBar(Gui<Style>& g, const char* id, const Rect& r, const PresetBarOptions& o = {}) {
    PresetBarResult out;
    PresetHost* ph = g.host().presets();
    if (!ph || r.empty())
        return out;
    g.pushId(id);
    const int saveW = o.save ? o.saveWidth + o.gap : 0;
    const Rect prev{r.x, r.y, o.arrowWidth, r.h};
    const Rect next{r.right() - saveW - o.arrowWidth, r.y, o.arrowWidth, r.h};
    const Rect save{r.right() - o.saveWidth, r.y, o.saveWidth, r.h};
    const Rect name{prev.right() + o.gap, r.y, next.x - prev.right() - 2 * o.gap, r.h};

    std::string label = ph->currentName();
    if (ph->modified())
        label += " *";

    // the list: factory presets, then the user's
    auto entries = [ph] {
        std::vector<PresetHost::Entry> all = ph->factory();
        const std::vector<PresetHost::Entry> user = ph->user();
        all.insert(all.end(), user.begin(), user.end());
        return all;
    };
    auto step = [&](int dir) {
        const std::vector<PresetHost::Entry> all = entries();
        if (all.empty())
            return;
        int cur = -1;
        const std::string now = ph->currentName();
        for (std::size_t i = 0; i < all.size(); ++i)
            if (all[i].name == now)
                cur = static_cast<int>(i);
        const int n = static_cast<int>(all.size());
        const int to = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
        out.loaded = ph->load(all[static_cast<std::size_t>(to)]);
    };

    if (g.button("prev", prev, "", {.variant = kButtonArrowLeft}))
        step(-1);
    if (g.button("next", next, "", {.variant = kButtonArrowRight}))
        step(+1);
    if (g.button("name", name, label.c_str(), {.variant = kButtonDisplay})) {
        const std::vector<PresetHost::Entry> all = entries();
        if (o.byCategory) {
            std::vector<PopupItem> items;
            std::vector<std::vector<std::size_t>> groups; // top-level row -> entries (a lone preset: one)
            std::vector<std::string> names;               // top-level row -> category, empty for a lone preset
            std::vector<int> selected; // the route to the current preset: its row, then its entry in the category
            const std::string now = ph->currentName();
            for (std::size_t i = 0; i < all.size(); ++i) {
                std::size_t row = items.size();
                if (!all[i].category.empty())
                    for (std::size_t k = 0; k < names.size(); ++k)
                        if (names[k] == all[i].category)
                            row = k;
                if (row == items.size()) {
                    items.push_back({all[i].category.empty() ? all[i].name : all[i].category});
                    groups.emplace_back();
                    names.push_back(all[i].category);
                }
                groups[row].push_back(i);
                if (!all[i].category.empty())
                    items[row].children.push_back({all[i].name});
                if (all[i].name == now) {
                    selected = {static_cast<int>(row)};
                    if (!all[i].category.empty())
                        selected.push_back(static_cast<int>(items[row].children.size()) - 1);
                }
            }
            const int cols = items.size() > 24   ? 3
                             : items.size() > 12 ? 2
                                                 : 1; // top-level rows; submenus are one column
            g.popupPath(
                "presets", name, std::move(items), selected,
                [ph, all, groups](const std::vector<int>& path) {
                    if (path.empty() || static_cast<std::size_t>(path[0]) >= groups.size())
                        return;
                    const std::vector<std::size_t>& group = groups[static_cast<std::size_t>(path[0])];
                    const std::size_t k = path.size() > 1 ? static_cast<std::size_t>(path[1]) : 0;
                    if (k < group.size())
                        ph->load(all[group[k]]);
                },
                cols);
        } else {
            std::vector<PopupItem> items;
            const std::size_t nFactory = ph->factory().size();
            int selected = -1;
            std::vector<int> map; // popup row -> entry
            for (std::size_t i = 0; i < all.size(); ++i) {
                if (i == nFactory && i > 0 && i < all.size())
                    items.push_back({"", true, true});
                if (all[i].name == ph->currentName())
                    selected = static_cast<int>(items.size());
                map.push_back(static_cast<int>(items.size()));
                items.push_back({all[i].name});
            }
            g.popup(
                "presets", name, std::move(items), selected,
                [ph, all, map](int row) {
                    for (std::size_t i = 0; i < map.size(); ++i)
                        if (map[i] == row)
                            ph->load(all[i]);
                },
                all.size() > 24   ? 3
                : all.size() > 12 ? 2
                                  : 1);
        }
    }
    if (o.save && g.button("save", save, o.saveLabel))
        out.save = true;
    g.popId();
    return out;
}

} // namespace spux::inline v1::ui
