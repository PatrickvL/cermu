/*
 * pin_labels.cpp — Pin label enum-to-string and enum-to-type conversions
 *
 * All functions are generated from the PIN_LABELS() X-macro defined in
 * pin_types.hpp, keeping the authoritative label list in one place.
 */

#include "core/chip_layout.hpp"

// ============================================================================
// pin_label_to_string — X-macro generated
// ============================================================================
//
// Returns the display-ready string for a PinLabel.  Active-low labels
// return the base name without "/" prefix — the display layer adds the
// overbar / slash based on get_invert_logic().

const char* pin_label_to_string(PinLabel label) {
    switch (label) {
#define PLS_INV_(id, str, cmt)   case PinLabel::id: return str;
#define PLS_CAT_(t)
#define PLS_PIN_(id, str, cmt)   case PinLabel::id: return str;
    PIN_LABELS(PLS_INV_, PLS_CAT_, PLS_PIN_)
#undef PLS_INV_
#undef PLS_CAT_
#undef PLS_PIN_
    default: return "?";
    }
}

// ============================================================================
// pin_label_to_description — X-macro generated
// ============================================================================
//
// Returns the human-readable description (the cmt field) for a PinLabel.
// Used for tooltip / hover text in chip layout rendering.

const char* pin_label_to_description(PinLabel label) {
    switch (label) {
#define PLD_INV_(id, str, cmt)   case PinLabel::id: return cmt;
#define PLD_CAT_(t)
#define PLD_PIN_(id, str, cmt)   case PinLabel::id: return cmt;
    PIN_LABELS(PLD_INV_, PLD_CAT_, PLD_PIN_)
#undef PLD_INV_
#undef PLD_CAT_
#undef PLD_PIN_
    default: return "";
    }
}

// ============================================================================
// pin_label_to_display_string
// ============================================================================
//
// For most labels, the display string stored in the X-macro already
// contains Unicode symbols (Φ, combining overline, etc.).  This function
// exists for backward compatibilty and simply forwards.

std::string pin_label_to_display_string(PinLabel label) {
    return std::string(pin_label_to_string(label));
}

// ============================================================================
// pin_label_to_pin_type — range-based derivation via sentinels
// ============================================================================
//
// Active-low labels: derive PinType from their canonical (active-high)
// counterpart via pin_canonical().
//
// Active-high labels: the PIN_LABELS X-macro places _<TYPE>_BEGIN sentinels
// at category boundaries.  We build a static array of {sentinel, PinType}
// and do a reverse linear scan (small N ~ 30, branch-free candidate).

namespace {

struct PinTypeRange {
    PinLabel begin;
    PinType  type;
};

static constexpr PinTypeRange pin_type_ranges_[] = {
#define PTR_INV_(id, str, cmt)
#define PTR_CAT_(t)              { PinLabel::_##t##_BEGIN, PinType::t },
#define PTR_PIN_(id, str, cmt)
    PIN_LABELS(PTR_INV_, PTR_CAT_, PTR_PIN_)
#undef PTR_INV_
#undef PTR_CAT_
#undef PTR_PIN_
};

static constexpr int NUM_PIN_TYPE_RANGES =
    sizeof(pin_type_ranges_) / sizeof(pin_type_ranges_[0]);

} // anonymous namespace

PinType pin_label_to_pin_type(PinLabel label) {
    // Active-low: resolve via canonical form
    if (label < PinLabel::ACTIVE_LOW_END)
        return pin_label_to_pin_type(pin_canonical(label));

    // Reverse scan through range table
    for (int i = NUM_PIN_TYPE_RANGES - 1; i >= 0; --i) {
        if (label >= pin_type_ranges_[i].begin)
            return pin_type_ranges_[i].type;
    }
    return PinType::NO_CONNECT;
}

// ChipPin member function implementation
PinType ChipPin::get_pin_type() const {
    return pin_label_to_pin_type(label);
}
