// Shared checks for popup lists taller than the view, run once per flavor (and with the plain test style).
#pragma once

#include <doctest.h>

#include <functional>
#include <string>
#include <vector>

#include "spux/ui/Gui.hpp"

namespace uitest {

using spux::ui::GuiCore;
using spux::ui::MouseButton;
using spux::ui::PopupItem;
using spux::ui::Rect;

// One frame; `extra` runs inside it, between begin and end (to open a popup).
using PopupFrame = std::function<void(const std::function<void(GuiCore&)>& extra)>;

inline std::vector<PopupItem> longList(int n) {
    std::vector<PopupItem> items;
    for (int i = 0; i < n; ++i)
        items.push_back({"Item " + std::to_string(i)});
    return items;
}

// The first row of list `level` that is in its window (-1: none).
inline int firstVisible(const GuiCore& g, std::size_t level, int n) {
    for (int i = 0; i < n; ++i)
        if (!g.popupRowRect(level, i).empty())
            return i;
    return -1;
}

inline void checkLongList(GuiCore& gui, const PopupFrame& frame, int viewW, int viewH) {
    constexpr int kRows = 200, kSelected = 150;
    int picked = -1;
    const Rect anchor{20, 8, 90, 14};
    auto open = [&] {
        picked = -1;
        frame([&](GuiCore& g) { g.popup("long", anchor, longList(kRows), kSelected, [&](int i) { picked = i; }); });
        frame({});
    };
    auto pointAt = [&](float x, float y) {
        gui.onMouseMove(x, y);
        frame({});
    };

    open();
    REQUIRE(gui.popupOpen());
    // a window of the list: no taller than the view, with the current choice in it and the start scrolled out
    const Rect box = gui.popupRect(0);
    CHECK(box.y >= 0);
    CHECK(box.bottom() <= viewH);
    CHECK(box.right() <= viewW);
    CHECK_FALSE(gui.popupRowRect(0, kSelected).empty());
    CHECK(gui.popupRowRect(0, 0).empty());
    CHECK(gui.popupRowRect(0, kRows - 1).empty());
    int first = firstVisible(gui, 0, kRows);
    CHECK(first > 0);

    // the wheel scrolls the list under the pointer: a notch away from the user toward the start, three rows
    const float cx = static_cast<float>(box.x + box.w / 2), cy = static_cast<float>(box.y + box.h / 2);
    pointAt(cx, cy);
    gui.onWheel(cx, cy, 2.0f, false);
    frame({});
    CHECK(firstVisible(gui, 0, kRows) == first - 6);
    gui.onWheel(cx, cy, -1.0f, false);
    frame({});
    CHECK(firstVisible(gui, 0, kRows) == first - 3);
    CHECK(gui.popupRect(0).h == box.h); // the popup keeps its size while it scrolls

    // Home and End, then the last row can be picked and maps to its place in the whole list
    gui.onKey({spux::platform::KeyEvent::Key::Home});
    frame({});
    CHECK(firstVisible(gui, 0, kRows) == 0);
    CHECK(gui.popupRowRect(0, 0).y >= gui.popupRect(0).y);
    gui.onKey({spux::platform::KeyEvent::Key::Down});
    frame({});
    CHECK(firstVisible(gui, 0, kRows) == 1);
    gui.onKey({spux::platform::KeyEvent::Key::End});
    frame({});
    const Rect last = gui.popupRowRect(0, kRows - 1);
    REQUIRE_FALSE(last.empty());
    CHECK(last.bottom() <= gui.popupRect(0).bottom());
    const float lx = static_cast<float>(last.x + last.w / 2), ly = static_cast<float>(last.y + last.h / 2);
    pointAt(lx, ly);
    gui.onMouseDown(lx, ly, MouseButton::Left, false);
    frame({});
    gui.onMouseUp(lx, ly, MouseButton::Left);
    frame({});
    CHECK_FALSE(gui.popupOpen());
    CHECK(picked == kRows - 1);

    // a row in the middle maps too, and the end rows scroll while the pointer rests on them
    open();
    gui.onKey({spux::platform::KeyEvent::Key::Home});
    frame({});
    CHECK(firstVisible(gui, 0, kRows) == 0);
    const Rect b2 = gui.popupRect(0);
    const float ax = static_cast<float>(b2.x + b2.w / 2), ay = static_cast<float>(b2.bottom() - 7);
    pointAt(ax, ay);
    for (int i = 0; i < 40; ++i)
        frame({});
    const int moved = firstVisible(gui, 0, kRows);
    CHECK(moved > 0);
    CHECK(gui.popupOpen()); // resting on a scroll row picks nothing
    const int row = moved + 2;
    const Rect rr = gui.popupRowRect(0, row);
    REQUIRE_FALSE(rr.empty());
    const float rx = static_cast<float>(rr.x + rr.w / 2), ry = static_cast<float>(rr.y + rr.h / 2);
    pointAt(rx, ry);
    gui.onMouseDown(rx, ry, MouseButton::Left, false);
    frame({});
    gui.onMouseUp(rx, ry, MouseButton::Left);
    frame({});
    CHECK(picked == row);
    CHECK_FALSE(gui.popupOpen());

    // a list that fits is not touched: every row is placed
    frame([&](GuiCore& g) { g.popup("short", anchor, longList(4), 2, [&](int i) { picked = i; }); });
    frame({});
    REQUIRE(gui.popupOpen());
    CHECK(firstVisible(gui, 0, 4) == 0);
    CHECK_FALSE(gui.popupRowRect(0, 3).empty());
    gui.closePopup();
    frame({});
}

// A submenu in a scrolled list opens beside the row that opens it, and reports the route through the whole list.
inline void checkLongListSubmenu(GuiCore& gui, const PopupFrame& frame) {
    constexpr int kRows = 200, kParent = 120;
    std::vector<int> route;
    auto items = longList(kRows);
    items[kParent].children = {{"Sub 0"}, {"Sub 1"}, {"Sub 2"}};
    const Rect anchor{20, 8, 90, 14};
    frame([&](GuiCore& g) {
        g.popupPath("tree", anchor, items, {kParent}, [&](const std::vector<int>& r) { route = r; });
    });
    frame({});
    REQUIRE(gui.popupOpen());
    const Rect cell = gui.popupRowRect(0, kParent); // the choice starts in view
    REQUIRE_FALSE(cell.empty());
    const float x = static_cast<float>(cell.x + cell.w / 2), y = static_cast<float>(cell.y + cell.h / 2);
    gui.onMouseMove(x, y);
    frame({});
    frame({});
    const Rect sub = gui.popupRect(1);
    REQUIRE_FALSE(sub.empty());
    CHECK(sub.y <= cell.y + cell.h);
    CHECK(sub.bottom() >= cell.y);
    CHECK((sub.x >= gui.popupRect(0).right() - 6 || sub.right() <= gui.popupRect(0).x + 6));
    const Rect s1 = gui.popupRowRect(1, 1);
    REQUIRE_FALSE(s1.empty());
    const float sx = static_cast<float>(s1.x + s1.w / 2), sy = static_cast<float>(s1.y + s1.h / 2);
    gui.onMouseMove(sx, sy);
    frame({});
    gui.onMouseDown(sx, sy, MouseButton::Left, false);
    frame({});
    gui.onMouseUp(sx, sy, MouseButton::Left);
    frame({});
    CHECK_FALSE(gui.popupOpen());
    CHECK(route == std::vector<int>{kParent, 1});
}

} // namespace uitest
