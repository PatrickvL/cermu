#pragma once
// =============================================================================
// typed_port.hpp — Compile-time port type aliases
// =============================================================================
//
// TypedPort<PT> is a thin wrapper around Port, parameterized by PortType.
// It exists solely to give each connector kind a distinct C++ type so that
// ports can participate in TypedManifest type lists alongside chips:
//
//   inline constexpr auto kManifest = make_manifest(
//       Slot<MOS6502>      {.base_addr = 0x0000, .label = "CPU"},
//       Slot<PortCassette>  {.name = "Cassette"},
//       Slot<PortCompositeVideo> {.name = "Video Out", .default_device = "crt_green"},
//   );
//
// TypedPort inherits Port's protected default constructor, so it can be
// default-constructed in a std::tuple and later initialized via init().
//
// Each port type has a convenience alias (PortControlDB9, PortIecSerial, …).
//
// =============================================================================

#include "core/port.hpp"

// ── TypedPort<PT> ────────────────────────────────────────────────────────────

template<PortType PT>
struct TypedPort : Port {
    static constexpr PortType port_type = PT;
    TypedPort() = default;
};

// ── Type trait ────────────────────────────────────────────────────────────────

template<typename T> struct is_typed_port : std::false_type {};
template<PortType PT> struct is_typed_port<TypedPort<PT>> : std::true_type {};
template<typename T> inline constexpr bool is_typed_port_v = is_typed_port<T>::value;

// ── Convenience aliases ──────────────────────────────────────────────────────

// Commodore family
using PortControlDB9     = TypedPort<PortType::CONTROL_PORT_DB9>;
using PortIecSerial      = TypedPort<PortType::IEC_SERIAL>;
using PortCassette       = TypedPort<PortType::CASSETTE_PORT>;
using PortUserPort       = TypedPort<PortType::USER_PORT>;
using PortExpansion      = TypedPort<PortType::EXPANSION_PORT>;

// Nintendo
using PortControllerNes  = TypedPort<PortType::CONTROLLER_NES>;
using PortControllerSnes = TypedPort<PortType::CONTROLLER_SNES>;

// Atari
using PortControllerAtari = TypedPort<PortType::CONTROLLER_ATARI>;

// Video outputs
using PortCompositeVideo = TypedPort<PortType::VIDEO_COMPOSITE>;
using PortSVideo         = TypedPort<PortType::VIDEO_SVIDEO>;
using PortRgb            = TypedPort<PortType::VIDEO_RGB>;
using PortRgbi           = TypedPort<PortType::VIDEO_RGBI>;
using PortComponentVideo = TypedPort<PortType::VIDEO_COMPONENT>;
using PortRf             = TypedPort<PortType::VIDEO_RF>;
using PortHdmiVideo      = TypedPort<PortType::VIDEO_HDMI>;

// Audio outputs
using PortAudioMono      = TypedPort<PortType::AUDIO_MONO>;
using PortAudioStereo    = TypedPort<PortType::AUDIO_STEREO>;
using PortAudioSpdif     = TypedPort<PortType::AUDIO_SPDIF>;
using PortAudioHdmi      = TypedPort<PortType::AUDIO_HDMI>;

// Generic
using PortCustom         = TypedPort<PortType::CUSTOM>;
