#include "spux/ui/Gui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace spux::inline v1::ui {

using params::ParamInfo;
using params::ParamKind;

namespace {

// FNV-1a over a painter state: a widget whose state hashes the same is not redrawn.
class StateHash {
public:
    void bytes(const void* p, std::size_t n) {
        const auto* c = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h_ ^= c[i];
            h_ *= 1099511628211ull;
        }
    }
    StateHash& operator<<(double v) {
        bytes(&v, sizeof v);
        return *this;
    }
    StateHash& operator<<(std::uint64_t v) {
        bytes(&v, sizeof v);
        return *this;
    }
    StateHash& operator<<(int v) { return *this << static_cast<std::uint64_t>(static_cast<std::uint32_t>(v)); }
    StateHash& operator<<(std::uint32_t v) { return *this << static_cast<std::uint64_t>(v); }
    StateHash& operator<<(bool v) { return *this << static_cast<std::uint64_t>(v ? 1 : 0); }
    StateHash& operator<<(const char* s) {
        if (s)
            bytes(s, std::strlen(s));
        const unsigned char end = 0xFF;
        bytes(&end, 1);
        return *this;
    }
    StateHash& operator<<(const Rect& r) { return *this << r.x << r.y << r.w << r.h; }
    StateHash& operator<<(const WidgetState& s) {
        return *this << s.hot << s.active << s.focused << s.disabled
                     << static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.theme));
    }
    [[nodiscard]] std::uint64_t value() const { return h_; }

private:
    std::uint64_t h_ = 1469598103934665603ull;
};

std::uint64_t keyOf(const KnobState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.paramId << s.value << s.label << s.valueText << s.editing
      << (s.editing ? s.editText : "") << (s.editing && s.caret) << s.bipolar << s.large << s.dimmed << s.showValue
      << static_cast<std::uint32_t>(s.accent) << s.mod.any << s.mod.base << s.mod.shared << s.mod.voiceCount
      << s.mod.armed << s.mod.armedRoute << s.mod.armedDepth << s.mod.overHandle << s.mod.handleActive;
    for (int v = 0; v < s.mod.voiceCount; ++v)
        h << s.mod.voices[v];
    return h.value();
}
std::uint64_t keyOf(const SliderState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.paramId << s.value << s.label << s.valueText << s.vertical
      << static_cast<std::uint32_t>(s.accent);
    return h.value();
}
std::uint64_t keyOf(const ToggleState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.paramId << s.on << s.label << static_cast<std::uint32_t>(s.accent);
    return h.value();
}
std::uint64_t keyOf(const SelectorState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.paramId << s.count << s.index << s.hotIndex << s.vertical
      << static_cast<std::uint32_t>(s.accent);
    if (s.labels)
        for (int i = 0; i < s.count; ++i)
            h << s.labels[i];
    return h.value();
}
std::uint64_t keyOf(const ButtonState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.label << s.on << static_cast<std::uint32_t>(s.accent) << s.variant;
    return h.value();
}
std::uint64_t keyOf(const DropdownState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.label << s.valueText << s.open;
    return h.value();
}
std::uint64_t keyOf(const StepperState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.label << s.valueText << s.hotPart;
    return h.value();
}
std::uint64_t keyOf(const TextFieldState& s) {
    StateHash h;
    h << static_cast<const WidgetState&>(s) << s.text << s.caret;
    return h.value();
}
std::uint64_t keyOf(const PanelState& s) {
    StateHash h;
    h << s.title << static_cast<std::uint32_t>(s.accent)
      << static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.theme));
    return h.value();
}

bool caretOn(double time) { return (static_cast<long long>(time * 2.0) & 1) != 0; }

// Drops the last UTF-8 character.
void popUtf8(std::string& s) {
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80)
        s.pop_back();
    if (!s.empty())
        s.pop_back();
}

} // namespace

std::uint64_t GuiCore::hashId(const char* s, std::uint64_t h) {
    for (; s && *s; ++s) {
        h ^= static_cast<unsigned char>(*s);
        h *= 1099511628211ull;
    }
    return h;
}

std::uint64_t GuiCore::hashId(std::uint64_t v, std::uint64_t h) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (8 * i)) & 0xFFu;
        h *= 1099511628211ull;
    }
    return h;
}

GuiCore::GuiCore(EditorHost& host, const Theme& theme) : host_(host), theme_(&theme) {
    gestureOpen_.assign(host_.paramTable().size(), false);
    gestureNest_.assign(host_.paramTable().size(), 0);
    keys_.reserve(32);
    focusables_.reserve(64);
    frameAreas_.reserve(128);
    frameIds_.reserve(128);
}

void GuiCore::setTheme(const Theme& t) {
    theme_ = &t;
    forceAll_ = true;
}

void GuiCore::pushId(const char* s) { ids_.push_back(hashId(s, scope())); }
void GuiCore::pushId(std::uint64_t v) { ids_.push_back(hashId(v, scope())); }
void GuiCore::popId() {
    if (!ids_.empty())
        ids_.pop_back();
}

// --- frame ---------------------------------------------------------------------------------------------------

void GuiCore::begin(IRenderer& r, double time, float dt, int width, int height) {
    r_ = &r;
    time_ = time;
    dt_ = dt;
    if (width != w_ || height != h_)
        forceAll_ = true;
    w_ = width;
    h_ = height;
    cache_ = caching_ ? r.layers() : nullptr;
    if (cache_)
        cache_->setTarget(Layer::Widgets);
    ++frame_;
    cursor_ = 0;
    focusables_.clear();
    frameAreas_.clear();
    frameIds_.clear();
    ids_.clear();
    hoveredParam_ = kNoParamId;
    painted_ = skipped_ = collisions_ = overlaps_ = 0;

    blocked_ = popupOpen_;
    if (popupOpen_ && downEvent_) {
        // a click while a list is open picks an item or closes the list; it never reaches the widgets below
        const bool inside = popup_.rect.contains(static_cast<int>(downX_), static_cast<int>(downY_));
        int picked = -1;
        if (inside)
            for (std::size_t i = 0; i < popup_.itemRects.size() && i < popup_.items.size(); ++i)
                if (popup_.itemRects[i].contains(static_cast<int>(downX_), static_cast<int>(downY_)) &&
                    !popup_.items[i].separator && popup_.items[i].enabled)
                    picked = static_cast<int>(i);
        if (inside && picked < 0) {
            // a press on a separator or a disabled item keeps the list open
        } else {
            auto choose = std::move(popup_.choose);
            popupOpen_ = false;
            if (picked >= 0 && choose)
                choose(picked);
        }
        downEvent_ = false;
        rightDownEvent_ = false;
    }
    if (popupOpen_) {
        for (const KeyEvent& k : keys_)
            if (k.key == KeyEvent::Key::Escape)
                popupOpen_ = false;
        keys_.clear();
    }
}

void GuiCore::beginBackground(IRenderer& r, int width, int height) {
    r_ = &r;
    w_ = width;
    h_ = height;
    background_ = true;
}

void GuiCore::endBackground() {
    background_ = false;
    forceAll_ = true; // the widgets were drawn over the old background
}

bool GuiCore::wantsFrame(double now) const {
    if (active_ != 0 || editing_ != 0)
        return true; // a drag in progress, or a caret blinking
    if (hasTooltips_ && hoverId_ != 0 && !tooltipShown_ && now - hoverSince_ >= theme_->feel.tooltipDelay)
        return true;
    return false;
}

void GuiCore::markSeen(std::uint64_t id) {
    if (!checkOverlaps_) // the collision count is a debug diagnostic, off with the overlap check
        return;
    if (std::find(frameIds_.begin(), frameIds_.end(), id) != frameIds_.end())
        ++collisions_;
    frameIds_.push_back(id);
}

bool GuiCore::beginPaint(std::uint64_t id, const Rect& area, std::uint64_t key) {
    markSeen(id);
    if (background_)
        return true;
    if (!cache_) {
        ++painted_;
        return true;
    }
    const Rect a = area.intersect({0, 0, w_, h_});
    if (checkOverlaps_)
        for (const Rect& o : frameAreas_)
            if (o.overlaps(a))
                ++overlaps_;
    frameAreas_.push_back(a);
    Record* found = nullptr;
    if (cursor_ < records_.size() && records_[cursor_].id == id)
        found = &records_[cursor_++];
    else
        for (std::size_t i = 0; i < records_.size() && !found; ++i)
            if (records_[i].id == id) {
                found = &records_[i];
                cursor_ = i + 1;
            }
    if (!found) {
        records_.push_back({id, {}, 0, 0});
        found = &records_.back();
    }
    Record& rec = *found;
    const bool fresh = rec.frame + 1 == frame_;
    if (!forceAll_ && fresh && rec.area == a && rec.key == key) {
        rec.frame = frame_;
        ++skipped_;
        return false;
    }
    if (fresh && rec.area != a)
        cache_->restore(rec.area);
    cache_->restore(a);
    rec = {id, a, key, frame_};
    ++painted_;
    return true;
}

template <class P, class S>
bool GuiCore::paintWidget(std::uint64_t id, const Rect& b, const P& painter, const S& state, std::uint64_t key) {
    if (!beginPaint(id, b.expand(painter.margin()), key))
        return false;
    painter.paint(*r_, b, state);
    return true;
}

void GuiCore::flushEdits() {
    while (!pendingEnds_.empty() && host_.pushEdit({params::ParamEdit::Kind::End, pendingEnds_.front(), 0.0}, 0))
        pendingEnds_.erase(pendingEnds_.begin());
    if (editsQueued_ || !pendingEnds_.empty()) {
        host_.requestFlush();
        editsQueued_ = false;
    }
}

void GuiCore::endImpl(const PopupPainter* popupPainter, const TooltipPainter* tooltipPainter) {
    // widgets not drawn this frame are erased
    if (cache_) {
        for (const Record& rec : records_)
            if (rec.frame != frame_ && rec.frame + 1 == frame_)
                cache_->restore(rec.area);
    }
    records_.erase(
        std::remove_if(records_.begin(), records_.end(), [this](const Record& r) { return r.frame != frame_; }),
        records_.end());
    forceAll_ = false;

    // overlays: laid out now, drawn last on the display layer (with caches) so they never touch a widget
    std::vector<Rect> overlays;
    Rect popupRect, tipRect;
    TooltipState tip;
    std::string tipText;
    if (popupOpen_ && popupPainter) {
        PopupState ps;
        ps.items = &popup_.items;
        ps.anchor = popup_.anchor;
        ps.selected = popup_.selected;
        ps.columns = popup_.columns;
        ps.theme = theme_;
        popup_.rect = popupPainter->layout(*r_, ps, w_, h_, popup_.itemRects);
        popup_.hot = -1;
        for (std::size_t i = 0; i < popup_.itemRects.size() && i < popup_.items.size(); ++i)
            if (popup_.itemRects[i].contains(static_cast<int>(mx_), static_cast<int>(my_)) &&
                !popup_.items[i].separator && popup_.items[i].enabled)
                popup_.hot = static_cast<int>(i);
        popupRect = popup_.rect;
        overlays.push_back(popupRect.expand(popupPainter->margin()));
    } else if (popupOpen_) {
        popupOpen_ = false; // no painter: no popups in this style
    }
    hasTooltips_ = tooltipPainter != nullptr;
    tooltipShown_ = false;
    if (!popupOpen_ && tooltipPainter && tooltipParam_ != kNoParamId && active_ == 0 &&
        time_ - hoverSince_ >= theme_->feel.tooltipDelay) {
        const std::size_t idx = indexOf(tooltipParam_);
        if (idx != params::kNoParam) {
            const ParamInfo& p = host_.paramTable()[idx];
            tipText = formatAt(idx, plainAt(idx));
            if (const char* hint = host_.paramHint(idx))
                tipText += std::string("   ") + hint;
            tip.title = p.name;
            tip.text = tipText.c_str();
            tip.pointer = {mx_, my_};
            tip.theme = theme_;
            tipRect = tooltipPainter->layout(*r_, tip, w_, h_);
            overlays.push_back(tipRect.expand(tooltipPainter->margin()));
            tooltipShown_ = true;
        }
    }
    if (cache_) {
        for (const Rect& o : overlayRects_)
            cache_->markDirty(o); // where the overlays were: back to the widgets
        for (const Rect& o : overlays)
            cache_->markDirty(o);
        if (!overlays.empty()) {
            cache_->composite();
            cache_->setTarget(Layer::Display);
        }
    }
    if (!popupRect.empty()) {
        PopupState ps;
        ps.items = &popup_.items;
        ps.anchor = popup_.anchor;
        ps.selected = popup_.selected;
        ps.hot = popup_.hot;
        ps.columns = popup_.columns;
        ps.itemRects = &popup_.itemRects;
        ps.theme = theme_;
        popupPainter->paint(*r_, popupRect, ps);
    }
    if (!tipRect.empty())
        tooltipPainter->paint(*r_, tipRect, tip);
    if (cache_)
        cache_->setTarget(Layer::Widgets);
    overlayRects_ = std::move(overlays);
    if (tooltipParam_ == kNoParamId)
        hoverId_ = 0;
    tooltipParam_ = kNoParamId;

    // Tab moves the focus through the widgets of this frame, in drawing order
    for (const KeyEvent& k : keys_) {
        if (k.key != KeyEvent::Key::Tab || focusables_.empty())
            continue;
        const std::size_t n = focusables_.size();
        std::size_t cur = n;
        for (std::size_t i = 0; i < n; ++i)
            if (focusables_[i] == focus_)
                cur = i;
        const std::size_t next = cur == n ? (k.shift ? n - 1 : 0) : (k.shift ? (cur + n - 1) % n : (cur + 1) % n);
        focus_ = focusables_[next];
    }
    keys_.clear();

    // a drag ends with the button, wherever the pointer is and whether or not the widget is still drawn
    if (active_ && !leftDown_) {
        if (activeParam_ != kNoParamId)
            endEdit(activeParam_);
        active_ = 0;
        activeParam_ = kNoParamId;
    }
    downEvent_ = upEvent_ = rightDownEvent_ = rightUpEvent_ = false;
    wheel_ = 0.0f;
    flushEdits();
}

// --- input ---------------------------------------------------------------------------------------------------

void GuiCore::onMouseMove(float x, float y) {
    mx_ = x;
    my_ = y;
}
void GuiCore::onLeave() { mx_ = my_ = -1.0f; }

void GuiCore::onMouseDown(float x, float y, MouseButton b, bool shift) {
    mx_ = x;
    my_ = y;
    shift_ = shift;
    downX_ = x;
    downY_ = y;
    if (b == MouseButton::Left) {
        leftDown_ = true;
        downEvent_ = true;
    } else {
        rightDown_ = true;
        rightDownEvent_ = true;
    }
}

void GuiCore::onMouseUp(float x, float y, MouseButton b) {
    mx_ = x;
    my_ = y;
    if (b == MouseButton::Left) {
        leftDown_ = false;
        upEvent_ = true;
    } else {
        rightDown_ = false;
        rightUpEvent_ = true;
    }
}

void GuiCore::onWheel(float x, float y, float steps, bool shift) {
    mx_ = x;
    my_ = y;
    shift_ = shift;
    wheel_ += steps;
}

void GuiCore::onKey(const KeyEvent& k) {
    if (keys_.size() < 32)
        keys_.push_back(k);
}

bool GuiCore::hovered(const Rect& r) const { return !blocked_ && r.contains(Point{mx_, my_}); }

bool GuiCore::pressedIn(const Rect& r, MouseButton b) const {
    if (blocked_ || background_)
        return false;
    const bool ev = b == MouseButton::Left ? downEvent_ : rightDownEvent_;
    return ev && r.contains(Point{downX_, downY_});
}

bool GuiCore::pressedInCircle(Point c, float radius) const {
    if (blocked_ || background_ || !downEvent_)
        return false;
    const float dx = downX_ - c.x, dy = downY_ - c.y;
    return dx * dx + dy * dy <= radius * radius;
}

bool GuiCore::takeDoubleClick(std::uint64_t id) {
    const bool dbl = lastClickId_ == id && time_ - lastClickTime_ <= theme_->feel.doubleClick;
    lastClickId_ = dbl ? 0 : id; // a third click starts a new pair
    lastClickTime_ = time_;
    return dbl;
}

bool GuiCore::keyPressed(KeyEvent::Key k) const {
    return std::any_of(keys_.begin(), keys_.end(), [k](const KeyEvent& e) { return e.key == k; });
}

void GuiCore::tooltipFor(const Rect& r, std::uint32_t paramId) {
    if (!hovered(r))
        return;
    const std::uint64_t id = static_cast<std::uint64_t>(paramId) + 1; // 0 is "nothing hovered"
    if (hoverId_ != id) {
        hoverId_ = id;
        hoverSince_ = time_;
    }
    tooltipParam_ = paramId;
}

// --- parameters ------------------------------------------------------------------------------------------------

std::size_t GuiCore::indexOf(std::uint32_t paramId) const { return host_.paramTable().indexOf(paramId); }

const ParamInfo* GuiCore::info(std::uint32_t paramId) const { return host_.paramTable().find(paramId); }

double GuiCore::plainAt(std::size_t i) const { return params::hostToPlain(host_.paramTable()[i], host_.paramHost(i)); }
double GuiCore::normAt(std::size_t i) const { return params::toNormalized(host_.paramTable()[i], plainAt(i)); }

double GuiCore::plain(std::uint32_t paramId) const {
    const std::size_t i = indexOf(paramId);
    return i == params::kNoParam ? 0.0 : plainAt(i);
}
double GuiCore::norm(std::uint32_t paramId) const {
    const std::size_t i = indexOf(paramId);
    return i == params::kNoParam ? 0.0 : normAt(i);
}

void GuiCore::beginAt(std::size_t i) {
    if (gestureOpen_.size() < host_.paramTable().size())
        gestureOpen_.resize(host_.paramTable().size(), false);
    if (gestureNest_.size() < gestureOpen_.size())
        gestureNest_.resize(gestureOpen_.size(), 0);
    if (gestureOpen_[i]) { // nested in an open gesture (a wheel step during a drag): the host sees only the outer one
        ++gestureNest_[i];
        return;
    }
    // keep one slot for the matching end; a begin that does not fit is dropped together with its end,
    // so the host never sees an unbalanced gesture
    gestureOpen_[i] = host_.pushEdit({params::ParamEdit::Kind::Begin, static_cast<std::uint32_t>(i), 0.0}, 1);
    editsQueued_ = true;
}

void GuiCore::valueAt(std::size_t i, double value) {
    const ParamInfo& p = host_.paramTable()[i];
    value = std::clamp(value, p.min, p.max);
    if (p.kind != ParamKind::Float)
        value = std::round(value);
    const double h = params::plainToHost(p, value);
    host_.setParamHost(i, h);
    host_.pushEdit({params::ParamEdit::Kind::Value, static_cast<std::uint32_t>(i), h}, host_.gestureReserve());
    editsQueued_ = true;
}

void GuiCore::endAt(std::size_t i) {
    if (i >= gestureOpen_.size() || !gestureOpen_[i])
        return;                // its begin was dropped
    if (gestureNest_[i] > 0) { // the end of a nested begin
        --gestureNest_[i];
        return;
    }
    // values leave the gesture reserve free, so the end fits; if it still does not, it is retried at
    // every flush until the host has drained the queue
    if (!host_.pushEdit({params::ParamEdit::Kind::End, static_cast<std::uint32_t>(i), 0.0}, 0))
        pendingEnds_.push_back(static_cast<std::uint32_t>(i));
    gestureOpen_[i] = false;
    editsQueued_ = true;
}

void GuiCore::setPlainAt(std::size_t i, double value) {
    beginAt(i);
    valueAt(i, value);
    endAt(i);
}

void GuiCore::beginEdit(std::uint32_t paramId) {
    const std::size_t i = indexOf(paramId);
    if (i != params::kNoParam)
        beginAt(i);
}
void GuiCore::editValue(std::uint32_t paramId, double value) {
    const std::size_t i = indexOf(paramId);
    if (i != params::kNoParam)
        valueAt(i, value);
}
void GuiCore::endEdit(std::uint32_t paramId) {
    const std::size_t i = indexOf(paramId);
    if (i != params::kNoParam)
        endAt(i);
}
void GuiCore::setPlain(std::uint32_t paramId, double value) {
    const std::size_t i = indexOf(paramId);
    if (i != params::kNoParam)
        setPlainAt(i, value);
}
void GuiCore::setNorm(std::uint32_t paramId, double n) {
    const std::size_t i = indexOf(paramId);
    if (i != params::kNoParam)
        setPlainAt(i, params::fromNormalized(host_.paramTable()[i], std::clamp(n, 0.0, 1.0)));
}

const char* GuiCore::formatAt(std::size_t i, double plain) {
    text_[0] = '\0';
    host_.formatParam(i, plain, text_, sizeof(text_));
    return text_;
}

std::string GuiCore::valueText(std::uint32_t paramId) {
    const std::size_t i = indexOf(paramId);
    return i == params::kNoParam ? std::string() : std::string(formatAt(i, plainAt(i)));
}

double GuiCore::wheelNormStep(double optionStep) const {
    const double step = optionStep < 0.0 ? theme_->feel.wheelStep : optionStep;
    return shift_ ? step * theme_->feel.fine : step;
}

// --- plain widgets -----------------------------------------------------------------------------------------------

bool GuiCore::view(std::uint64_t id, const Rect& area, std::uint64_t key) {
    if (area.empty())
        return false;
    return beginPaint(hashId(id, scope()), area, key);
}

Interaction GuiCore::interact(std::uint64_t idIn, const Rect& hit, float radius) {
    const std::uint64_t id = hashId(idIn, scope() ^ 0x9E3779B97F4A7C15ull);
    Interaction in;
    const Point c = hit.center();
    auto inside = [&](float x, float y) {
        if (radius > 0.0f)
            return (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= radius * radius;
        return hit.contains(Point{x, y});
    };
    in.hot = !blocked_ && !background_ && inside(mx_, my_);
    in.pointer = {mx_, my_};
    in.shift = shift_;
    if (in.hot)
        in.wheel = wheel_;
    if (!blocked_ && !background_ && downEvent_ && inside(downX_, downY_)) {
        in.pressed = true;
        in.doubleClick = takeDoubleClick(id);
        active_ = id;
        activeParam_ = kNoParamId;
        dragStartX_ = downX_;
        dragStartY_ = downY_;
    }
    if (!blocked_ && !background_ && rightDownEvent_ && inside(downX_, downY_))
        in.rightPressed = true;
    if (active_ == id) {
        in.start = {dragStartX_, dragStartY_};
        in.dx = mx_ - dragStartX_;
        in.dy = my_ - dragStartY_;
        if (in.dx != 0.0f || in.dy != 0.0f)
            lastClickId_ = 0; // a drag is not the first half of a double click
        if (leftDown_) {
            in.active = true;
        } else {
            in.released = true;
            if (activeParam_ != kNoParamId)
                endEdit(activeParam_);
            active_ = 0;
            activeParam_ = kNoParamId;
        }
    }
    return in;
}

Interaction GuiCore::handle(const char* idText, Point c, float radius, std::uint32_t gestureParam) {
    const Rect hit{static_cast<int>(std::floor(c.x - radius)), static_cast<int>(std::floor(c.y - radius)),
                   static_cast<int>(std::ceil(radius * 2)) + 1, static_cast<int>(std::ceil(radius * 2)) + 1};
    Interaction in = interact(hashId(idText, hashId("handle")), hit, radius);
    if (in.pressed && gestureParam != kNoParamId) {
        activeParam_ = gestureParam;
        beginEdit(gestureParam);
    }
    return in;
}

void GuiCore::text(const char* s, const Rect& area, Color c, float px, Align a, FontWeight w) {
    StateHash h;
    h << s << static_cast<std::uint32_t>(c) << static_cast<double>(px) << static_cast<int>(a) << static_cast<int>(w);
    const std::uint64_t id = hashId(static_cast<std::uint64_t>(static_cast<std::uint32_t>(area.x)) << 32 |
                                        static_cast<std::uint32_t>(area.y),
                                    hashId(static_cast<std::uint64_t>(kText), scope()));
    if (!beginPaint(id, area, h.value()))
        return;
    const float x = a == Align::Left     ? static_cast<float>(area.x)
                    : a == Align::Center ? static_cast<float>(area.x) + static_cast<float>(area.w) * 0.5f
                                         : static_cast<float>(area.right());
    r_->pushClip(area);
    r_->drawTextPx(s, x, static_cast<float>(area.y), c, px, a, w);
    r_->popClip();
}

void GuiCore::label(const char* s, float x, float y, Color c, float px, Align a, FontWeight w) {
    r_->drawTextPx(s, x, y, c, px, a, w);
}

void GuiCore::popup(const char* idText, const Rect& anchor, std::vector<PopupItem> items, int selected,
                    std::function<void(int)> choose, int columns) {
    popup_ = {};
    popup_.id = hashId(idText, scope());
    popup_.anchor = anchor;
    popup_.items = std::move(items);
    popup_.selected = selected;
    popup_.choose = std::move(choose);
    popup_.columns = std::max(1, columns);
    popupOpen_ = true;
}

void GuiCore::popup(const char* idText, const Rect& anchor, const std::vector<std::string>& items, int selected,
                    std::function<void(int)> choose, int columns) {
    std::vector<PopupItem> list;
    list.reserve(items.size());
    for (const std::string& s : items)
        list.push_back({s});
    popup(idText, anchor, std::move(list), selected, std::move(choose), columns);
}

void GuiCore::panelImpl(const Rect& b, const char* title, Color accent, const PanelPainter& p) {
    PanelState s;
    s.title = title ? title : "";
    s.accent = accent ? accent : this->accent();
    s.theme = theme_;
    const std::uint64_t id =
        hashId(static_cast<std::uint64_t>(static_cast<std::uint32_t>(b.x)) << 32 | static_cast<std::uint32_t>(b.y),
               hashId(static_cast<std::uint64_t>(kPanel), scope()));
    paintWidget(id, b, p, s, keyOf(s));
}

bool GuiCore::buttonImpl(const char* idText, const Rect& b, const char* label, const ButtonOptions& o,
                         const ButtonPainter& p) {
    const std::uint64_t id = hashId(idText, hashId(static_cast<std::uint64_t>(kButton), scope()));
    registerFocus(id);
    const bool press = pressedIn(b);
    const bool click = press || (focused(id) && keyPressed(KeyEvent::Key::Enter));
    if (press) {
        focus_ = id;
        active_ = id;
        activeParam_ = kNoParamId;
    }
    ButtonState s;
    s.hot = hovered(b);
    s.active = active_ == id && leftDown_;
    s.focused = focused(id);
    s.theme = theme_;
    s.label = label ? label : "";
    s.on = o.on;
    s.accent = o.accent ? o.accent : accent();
    s.variant = o.variant;
    paintWidget(id, b, p, s, keyOf(s));
    return click;
}

GuiCore::TextResult GuiCore::textFieldImpl(const char* idText, const Rect& b, std::string& text, std::size_t maxBytes,
                                           const TextFieldPainter& p) {
    const std::uint64_t id = hashId(idText, hashId(static_cast<std::uint64_t>(kTextField), scope()));
    focus_ = id;
    editing_ = id; // keeps frames coming for the caret
    TextResult result = TextResult::Editing;
    for (const KeyEvent& k : keys_) {
        if (k.key == KeyEvent::Key::Char) {
            if (text.size() + std::strlen(k.text) <= maxBytes)
                text += k.text;
        } else if (k.key == KeyEvent::Key::Backspace) {
            popUtf8(text);
        } else if (k.key == KeyEvent::Key::Enter) {
            result = TextResult::Commit;
        } else if (k.key == KeyEvent::Key::Escape) {
            result = TextResult::Cancel;
        }
    }
    keys_.clear();
    if (result != TextResult::Editing)
        editing_ = 0;
    TextFieldState s;
    s.focused = true;
    s.theme = theme_;
    s.text = text.c_str();
    s.caret = caretOn(time_);
    paintWidget(id, b, p, s, keyOf(s));
    return result;
}

// --- parameter widgets -------------------------------------------------------------------------------------------

void GuiCore::knobImpl(std::uint32_t paramId, const Rect& b, const KnobOptions& o, const KnobPainter& p) {
    const std::size_t idx = indexOf(paramId);
    if (idx == params::kNoParam || b.empty())
        return;
    const ParamInfo& info = host_.paramTable()[idx];
    const std::uint64_t id = widgetId(kKnob, paramId);
    const Rect dial = p.hitArea(b);
    const Point center = b.center();
    const double n = normAt(idx);

    ModulationHost* mh = (info.flags & params::PF_MODULATABLE) ? host_.modulation() : nullptr;
    ModulationView mv;
    if (mh) {
        const ModulationHost::Display d = mh->display(paramId);
        mv.any = d.any;
        mv.base = d.base;
        mv.shared = d.shared;
        mv.voiceCount = std::clamp(d.voiceCount, 0, 4);
        for (int v = 0; v < mv.voiceCount; ++v)
            mv.voices[v] = d.voices[v];
        mv.armed = mh->armed();
        mv.armedRoute = d.armedRoute;
        mv.armedDepth = d.armedDepth;
    }
    // the armed source's depth handle sits where the painter puts it, at the end of its route
    Point handleAt;
    const bool hasHandle =
        mv.armed && p.depthHandle(b, std::clamp(mv.base + (mv.armedRoute ? mv.armedDepth : 0.0), 0.0, 1.0), handleAt);
    const bool overHandle = hasHandle && hovered(b) && std::hypot(mx_ - handleAt.x, my_ - handleAt.y) < 9.0f;

    auto inHit = [&](float x, float y) {
        if (o.hitRadius > 0.0f)
            return (x - center.x) * (x - center.x) + (y - center.y) * (y - center.y) <= o.hitRadius * o.hitRadius;
        return dial.contains(Point{x, y}) || b.contains(Point{x, y});
    };
    const bool hot = !blocked_ && inHit(mx_, my_);
    const bool pressed = !blocked_ && downEvent_ && inHit(downX_, downY_);
    registerFocus(id);
    if (hot) {
        hoveredParam_ = paramId;
        tooltipFor(b, paramId);
    }

    // --- interaction ---
    const bool editingThis = editing_ == id;
    if (pressed) {
        focus_ = id;
        if (editing_ && !editingThis)
            editing_ = 0;
        const bool dbl = takeDoubleClick(id);
        if (overHandle) {
            // the depth handle: the route is made on the first grab, and its depth parameter is the gesture
            const std::uint32_t dp = mh->depthParam(paramId);
            if (dp != ModulationHost::kNone && indexOf(dp) != params::kNoParam) {
                active_ = id | 1;
                activeParam_ = dp;
                dragStartY_ = downY_;
                dragStart_ = plain(dp);
                dragShift_ = shift_;
                beginEdit(dp);
            }
        } else if (dbl && !editingThis) {
            setPlainAt(idx, info.def);
            active_ = 0;
        } else if (!editingThis) {
            active_ = id;
            activeParam_ = paramId;
            dragStartY_ = downY_;
            dragStart_ = n;
            dragShift_ = shift_;
            beginAt(idx);
        }
    }
    if (active_ == id && leftDown_) {
        if (shift_ != dragShift_) { // fine and coarse switch without a jump
            dragStart_ = normAt(idx);
            dragStartY_ = my_;
            dragShift_ = shift_;
        }
        if (my_ != dragStartY_)
            lastClickId_ = 0;
        const float pixels = o.dragPixels > 0.0f ? o.dragPixels : theme_->feel.dragPixels;
        const double sens = (shift_ ? theme_->feel.fine : 1.0f) / pixels;
        const double target = std::clamp(dragStart_ + static_cast<double>(dragStartY_ - my_) * sens, 0.0, 1.0);
        valueAt(idx, params::fromNormalized(info, target));
    } else if (active_ == (id | 1) && leftDown_ && activeParam_ != kNoParamId && mh) {
        const ParamInfo* dp = this->info(activeParam_);
        if (dp) {
            const double half = 0.5 * (dp->max - dp->min);
            const double depth = dragStart_ + static_cast<double>(dragStartY_ - my_) / mh->depthDragPixels() * half;
            editValue(activeParam_, std::clamp(depth, dp->min, dp->max));
        }
    }
    const double wheelStep = wheelNormStep(o.wheelStep);
    if (hot && wheel_ != 0.0f && wheelStep != 0.0) {
        if (info.kind != ParamKind::Float)
            setPlainAt(idx, plainAt(idx) + (wheel_ > 0 ? 1.0 : -1.0));
        else
            setPlainAt(idx,
                       params::fromNormalized(info, std::clamp(n + static_cast<double>(wheel_) * wheelStep, 0.0, 1.0)));
    }
    if (hot && rightDownEvent_ && o.valueEntry && !mv.armed) {
        editing_ = id;
        editText_ = formatAt(idx, plainAt(idx));
        focus_ = id;
    }
    bool enteredNow = false; // the Enter that opens value entry is not also its commit
    if (focused(id) && editing_ != id) {
        for (const KeyEvent& k : keys_) {
            double dir = 0;
            if (k.key == KeyEvent::Key::Up || k.key == KeyEvent::Key::Right)
                dir = 1;
            if (k.key == KeyEvent::Key::Down || k.key == KeyEvent::Key::Left)
                dir = -1;
            if (k.key == KeyEvent::Key::Enter && o.valueEntry) {
                editing_ = id;
                editText_ = formatAt(idx, plainAt(idx));
                enteredNow = true;
            }
            if (dir != 0) {
                if (info.kind != ParamKind::Float)
                    setPlainAt(idx, plainAt(idx) + dir);
                else
                    setPlainAt(idx, params::fromNormalized(
                                        info, std::clamp(normAt(idx) +
                                                             dir * (k.shift ? theme_->feel.keyStep * theme_->feel.fine
                                                                            : theme_->feel.keyStep),
                                                         0.0, 1.0)));
            }
        }
    }
    if (editing_ == id) {
        if (downEvent_ && !b.contains(Point{downX_, downY_}))
            editing_ = 0;
        for (const KeyEvent& k : keys_) {
            if (enteredNow)
                break;
            if (k.key == KeyEvent::Key::Char)
                editText_ += k.text;
            else if (k.key == KeyEvent::Key::Backspace)
                popUtf8(editText_);
            else if (k.key == KeyEvent::Key::Escape)
                editing_ = 0;
            else if (k.key == KeyEvent::Key::Enter) {
                double v = 0.0;
                if (host_.parseParam(idx, editText_.c_str(), v))
                    setPlainAt(idx, v);
                editing_ = 0;
            }
        }
        keys_.clear();
    }

    // --- state for the painter ---
    KnobState s;
    s.theme = theme_;
    s.param = &info;
    s.paramId = paramId;
    s.plain = plainAt(idx);
    s.value = params::toNormalized(info, s.plain);
    s.hot = hot && !overHandle;
    s.active = active_ == id;
    s.focused = focused(id);
    s.label = o.label ? o.label : info.name;
    s.valueText = o.valueText ? o.valueText : formatAt(idx, s.plain);
    s.editing = editing_ == id;
    s.editText = editText_.c_str();
    s.caret = s.editing && caretOn(time_);
    s.bipolar = o.bipolar;
    s.large = o.large;
    s.dimmed = o.dimmed;
    s.showValue = o.showValue;
    s.accent = o.accent ? o.accent : accent();
    mv.overHandle = overHandle;
    mv.handleActive = active_ == (id | 1);
    s.mod = mv;
    paintWidget(id, b, p, s, keyOf(s));
}

void GuiCore::sliderImpl(std::uint32_t paramId, const Rect& b, const SliderOptions& o, const SliderPainter& p) {
    const std::size_t idx = indexOf(paramId);
    if (idx == params::kNoParam || b.empty())
        return;
    const ParamInfo& info = host_.paramTable()[idx];
    const std::uint64_t id = widgetId(kSlider, paramId);
    const Rect hit = b.inset(-o.hitPad);
    const double n = normAt(idx);
    const bool hot = hovered(hit);
    registerFocus(id);
    if (hot) {
        hoveredParam_ = paramId;
        tooltipFor(b, paramId);
    }
    SliderState s;
    s.theme = theme_;
    s.param = &info;
    s.paramId = paramId;
    s.vertical = o.vertical;
    const double dir = o.inverted ? -1.0 : 1.0;

    if (pressedIn(hit)) {
        focus_ = id;
        if (takeDoubleClick(id)) {
            setPlainAt(idx, info.def);
        } else {
            active_ = id;
            activeParam_ = paramId;
            dragStart_ = n;
            dragStartX_ = downX_;
            dragStartY_ = downY_;
            dragShift_ = shift_;
            beginAt(idx);
        }
    }
    if (active_ == id && leftDown_) {
        if (o.jump) {
            s.value = n;
            const Rect tr = p.track(b, s);
            const double t = o.vertical ? (static_cast<double>(tr.bottom()) - my_) / std::max(1, tr.h)
                                        : (static_cast<double>(mx_) - tr.x) / std::max(1, tr.w);
            valueAt(idx, params::fromNormalized(info, std::clamp(o.inverted ? 1.0 - t : t, 0.0, 1.0)));
        } else {
            if (shift_ != dragShift_) {
                dragStart_ = normAt(idx);
                dragStartX_ = mx_;
                dragStartY_ = my_;
                dragShift_ = shift_;
            }
            const double moved =
                o.vertical ? static_cast<double>(dragStartY_ - my_) : static_cast<double>(mx_ - dragStartX_);
            if (moved != 0.0)
                lastClickId_ = 0;
            const float pixels = o.dragPixels > 0.0f ? o.dragPixels : theme_->feel.dragPixels;
            const double sens = (shift_ ? theme_->feel.fine : 1.0f) / pixels;
            valueAt(idx, params::fromNormalized(info, std::clamp(dragStart_ + dir * moved * sens, 0.0, 1.0)));
        }
    }
    const double wheelStep = wheelNormStep(o.wheelStep);
    if (hot && wheel_ != 0.0f && wheelStep != 0.0 && !(active_ == id && leftDown_)) {
        const double sign = (wheel_ > 0 ? 1.0 : -1.0) * dir;
        if (info.kind != ParamKind::Float)
            setPlainAt(idx, plainAt(idx) + sign);
        else
            setPlainAt(idx, params::fromNormalized(
                                info, std::clamp(n + dir * static_cast<double>(wheel_) * wheelStep, 0.0, 1.0)));
    }
    if (focused(id)) {
        for (const KeyEvent& k : keys_) {
            double d = 0;
            if (k.key == KeyEvent::Key::Up || k.key == KeyEvent::Key::Right)
                d = 1;
            if (k.key == KeyEvent::Key::Down || k.key == KeyEvent::Key::Left)
                d = -1;
            if (d != 0)
                setPlainAt(idx,
                           params::fromNormalized(
                               info, std::clamp(normAt(idx) + d * (k.shift ? theme_->feel.keyStep * theme_->feel.fine
                                                                           : theme_->feel.keyStep),
                                                0.0, 1.0)));
        }
    }
    s.plain = plainAt(idx);
    s.value = params::toNormalized(info, s.plain);
    s.hot = hot;
    s.active = active_ == id;
    s.focused = focused(id);
    s.label = o.label ? o.label : info.name;
    s.valueText = o.valueText ? o.valueText : formatAt(idx, s.plain);
    s.accent = o.accent ? o.accent : accent();
    paintWidget(id, b, p, s, keyOf(s));
}

bool GuiCore::toggleImpl(std::uint32_t paramId, const Rect& b, const ToggleOptions& o, const TogglePainter& p) {
    const std::size_t idx = indexOf(paramId);
    if (idx == params::kNoParam || b.empty())
        return false;
    const ParamInfo& info = host_.paramTable()[idx];
    const std::uint64_t id = widgetId(kToggle, paramId);
    const bool hot = hovered(b);
    registerFocus(id);
    if (hot) {
        hoveredParam_ = paramId;
        tooltipFor(b, paramId);
    }
    const bool on = plainAt(idx) > 0.5 * (info.min + info.max);
    bool changed = false;
    const bool press = pressedIn(b);
    if (press || (focused(id) && keyPressed(KeyEvent::Key::Enter))) {
        if (press)
            focus_ = id;
        bool next = !on;
        if (press && o.centerBand > 0) {
            const int rel = static_cast<int>(downY_) - (b.y + b.h / 2);
            if (rel < -o.centerBand)
                next = false;
            else if (rel > o.centerBand)
                next = true;
        }
        setPlainAt(idx, next ? info.max : info.min);
        changed = next != on;
    }
    if (hot && o.wheel && wheel_ != 0.0f)
        setPlainAt(idx, wheel_ > 0 ? info.max : info.min);
    ToggleState s;
    s.theme = theme_;
    s.param = &info;
    s.paramId = paramId;
    s.on = plainAt(idx) > 0.5 * (info.min + info.max);
    s.hot = hot;
    s.active = press;
    s.focused = focused(id);
    s.label = o.label ? o.label : info.name;
    s.accent = o.accent ? o.accent : accent();
    paintWidget(id, b, p, s, keyOf(s));
    return changed;
}

void GuiCore::selectorImpl(std::uint32_t paramId, const Rect& b, const SelectorOptions& o, const SelectorPainter& p) {
    const std::size_t idx = indexOf(paramId);
    if (idx == params::kNoParam || b.empty())
        return;
    const ParamInfo& info = host_.paramTable()[idx];
    const int count = info.kind == ParamKind::Enum ? static_cast<int>(params::enumCount(info))
                                                   : static_cast<int>(std::lround(info.max - info.min)) + 1;
    if (count <= 0)
        return;
    const std::uint64_t id = widgetId(kSelector, paramId);
    registerFocus(id);
    const bool hot = hovered(b);
    if (hot) {
        hoveredParam_ = paramId;
        tooltipFor(b, paramId);
    }
    SelectorState s;
    s.theme = theme_;
    s.param = &info;
    s.paramId = paramId;
    s.count = count;
    s.vertical = o.vertical;
    s.labels = o.labels;
    s.index = static_cast<int>(std::lround(plainAt(idx) - info.min));
    for (int i = 0; i < count; ++i) {
        const Rect c = p.cell(b, s, i);
        if (hovered(c))
            s.hotIndex = i;
        if (pressedIn(c)) {
            focus_ = id;
            setPlainAt(idx, info.min + i);
        }
    }
    if (hot && o.wheel != 0 && wheel_ != 0.0f)
        setPlainAt(idx, std::clamp(plainAt(idx) + (wheel_ > 0 ? o.wheel : -o.wheel), info.min, info.max));
    if (focused(id)) {
        for (const KeyEvent& k : keys_) {
            if (k.key == KeyEvent::Key::Right || k.key == KeyEvent::Key::Down)
                setPlainAt(idx, std::min(info.max, plainAt(idx) + 1.0));
            if (k.key == KeyEvent::Key::Left || k.key == KeyEvent::Key::Up)
                setPlainAt(idx, std::max(info.min, plainAt(idx) - 1.0));
        }
    }
    s.index = static_cast<int>(std::lround(plainAt(idx) - info.min));
    s.hot = hot;
    s.focused = focused(id);
    s.accent = o.accent ? o.accent : accent();
    paintWidget(id, b, p, s, keyOf(s));
}

void GuiCore::dropdownImpl(std::uint32_t paramId, const Rect& b, const char* label, const DropdownPainter& p) {
    const std::size_t idx = indexOf(paramId);
    if (idx == params::kNoParam || b.empty())
        return;
    const ParamInfo& info = host_.paramTable()[idx];
    const std::uint64_t id = widgetId(kDropdown, paramId);
    const bool hot = hovered(b);
    registerFocus(id);
    if (hot) {
        hoveredParam_ = paramId;
        tooltipFor(b, paramId);
    }
    const bool press = pressedIn(b);
    if (press)
        focus_ = id;
    if (press || (focused(id) && keyPressed(KeyEvent::Key::Enter))) {
        std::vector<PopupItem> items;
        const std::size_t n = info.kind == ParamKind::Enum
                                  ? params::enumCount(info)
                                  : static_cast<std::size_t>(std::lround(info.max - info.min)) + 1;
        items.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            items.push_back({formatAt(idx, info.min + static_cast<double>(i))});
        const int cols = n > 24 ? 3 : n > 12 ? 2 : 1;
        const double lo = info.min;
        popup(
            "dropdown", b, std::move(items), static_cast<int>(std::lround(plainAt(idx) - lo)),
            [this, idx, lo](int i) { setPlainAt(idx, lo + i); }, cols);
        popup_.id = id;
    }
    DropdownState s;
    s.theme = theme_;
    s.param = &info;
    s.hot = hot;
    s.focused = focused(id);
    s.label = label ? label : "";
    s.valueText = formatAt(idx, plainAt(idx));
    s.open = popupOpen_ && popup_.id == id;
    paintWidget(id, b, p, s, keyOf(s));
}

void GuiCore::stepperImpl(std::uint32_t paramId, const Rect& b, const char* label, const StepperPainter& p) {
    const std::size_t idx = indexOf(paramId);
    if (idx == params::kNoParam || b.empty())
        return;
    const ParamInfo& info = host_.paramTable()[idx];
    const std::uint64_t id = widgetId(kStepper, paramId);
    registerFocus(id);
    const bool hot = hovered(b);
    if (hot) {
        hoveredParam_ = paramId;
        tooltipFor(b, paramId);
    }
    const Rect minus = p.minusPart(b), plus = p.plusPart(b);
    if (pressedIn(minus)) {
        focus_ = id;
        setPlainAt(idx, plainAt(idx) - 1.0);
    }
    if (pressedIn(plus)) {
        focus_ = id;
        setPlainAt(idx, plainAt(idx) + 1.0);
    }
    if (hot && wheel_ != 0.0f)
        setPlainAt(idx, plainAt(idx) + (wheel_ > 0 ? 1.0 : -1.0));
    if (focused(id)) {
        for (const KeyEvent& k : keys_) {
            if (k.key == KeyEvent::Key::Right || k.key == KeyEvent::Key::Up)
                setPlainAt(idx, plainAt(idx) + 1.0);
            if (k.key == KeyEvent::Key::Left || k.key == KeyEvent::Key::Down)
                setPlainAt(idx, plainAt(idx) - 1.0);
        }
    }
    StepperState s;
    s.theme = theme_;
    s.param = &info;
    s.hot = hot;
    s.focused = focused(id);
    s.hotPart = hovered(minus) ? -1 : hovered(plus) ? 1 : 0;
    s.label = label ? label : "";
    s.valueText = formatAt(idx, plainAt(idx));
    paintWidget(id, b, p, s, keyOf(s));
}

} // namespace spux::inline v1::ui
