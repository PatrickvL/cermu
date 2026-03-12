/*
 * pin_labels.h - Pin label enum to string conversion functions
 * 
 * This header provides efficient enum-to-string conversion for pin labels,
 * supporting both plain text and Unicode display formats.
 */

#pragma once
#include "core/pin_types.h"
#include <string>

// Fast enum-to-string lookup for basic pin names
const char* pin_label_to_string(PinLabel label);

// Display string with Unicode symbols for enhanced GUI rendering
std::string pin_label_to_display_string(PinLabel label);

// Derive pin type from pin label for GUI color coding and categorization
PinType pin_label_to_pin_type(PinLabel label);

