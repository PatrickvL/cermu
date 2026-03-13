# Cermu Architecture — Complete Implementation Reference

**Status:** Ready for implementation
**Replaces:** All previous plan revisions

---

## Registries (Complete — No Changes)

All four registries follow the same static self-registration pattern.

**`ChipRegistry`** — string → `ChipSlot::FactoryFn`. `REGISTER_CHIP` / `REGISTER_CHIP_TYPE` macros. `make_slot_from_registry()` bridge for file-driven slots.

**`DeviceRegistry`** — string → `DeviceFactory`. `REGISTER_DEVICE` macro. `get_compatible_devices(PortType)` for port matching. Complex peripherals (1541) register here and produce a `Board` instance rather than a bare `PeripheralDevice`.

**`SystemRegistry`** — two-phase file identification, alias-boost, confidence scoring. `create_system_for_file()` / `create_system_by_name()`. Produces a `System`. No changes.

**`PortRegistry`** — string / `PortType` → `PortDefinition`. `REGISTER_PORT` macro. Standard port definitions in `PortSignals::` self-register from `port.cpp`. System-specific ports register from their own `.cpp`. Not yet implemented.

---

## Type Hierarchy

```
ComponentBase
├── ChipBase                — bus-attached; registered in BoardBase::components_
└── Port           — signal ports; registered in BoardBase::components_

BoardBase : ComponentBase   — non-templated base; owns component list
└── Board<Spec>             — owns chips, ports, unified buffer, BusMap
    └── (e.g. VIC20Board)   — concrete board implementation

System                      — one or more BoardBase instances + inter-board Connections
Session                     — one or more Systems + inter-system Connections

PeripheralDevice            — external attachment to a Port
└── InputPeripheralDevice   — adds host input binding, SDL routing, keymap presets
```

`EmulatorHost` (formerly `GenericEmulatorGUI`) — host substrate, unchanged.
`SessionGUI` (formerly `SystemGUI`) — owns `Session`, implements `EmulatorHost` hooks.

---

## ComponentBase

```cpp
class ComponentBase {
public:
    virtual ~ComponentBase() = default;
    virtual void        reset()          {}
    virtual void        power_on()       {}
    virtual const char* component_name() const { return nullptr; }
};
```

`ChipBase` and `Port` already derive from this. Complete.

---

## PortType and Signal Tables

```cpp
enum class PortType {
    // Commodore
    CONTROL_PORT_DB9,
    IEC_SERIAL,
    CASSETTE_PORT,
    USER_PORT,
    EXPANSION_PORT,

    // Nintendo
    CONTROLLER_NES,
    CONTROLLER_SNES,

    // Atari
    CONTROLLER_ATARI,

    // Video outputs
    VIDEO_COMPOSITE,
    VIDEO_SVIDEO,
    VIDEO_RGB,
    VIDEO_RGBI,
    VIDEO_COMPONENT,
    VIDEO_HDMI,

    // Audio outputs
    AUDIO_MONO,
    AUDIO_STEREO,
    AUDIO_SPDIF,
    AUDIO_HDMI,

    CUSTOM,
    COUNT
};
```

---

## Output Signal Descriptors

Signal descriptors describe what a port carries. They live on `Port` instances.

```cpp
enum class VideoSignalType {
    Composite,
    SVideo,
    RGB,
    RGBI,
    YPbPr,
    Digital,
};

enum class AudioSignalType {
    Mono,
    Stereo,
    Quadraphonic,
};

struct VideoOutput {
    VideoSignalType signal_type         = VideoSignalType::Composite;
    int             width               = 0;
    int             height              = 0;
    float           refresh_rate_hz     = 0.0f;
    float           dot_clock_mhz       = 0.0f;
    float           pixel_aspect_ratio  = 1.0f;
    uint32_t*       pixels              = nullptr;  // ARGB8888; host-allocated
};

struct AudioOutput {
    AudioSignalType signal_type         = AudioSignalType::Mono;
    int             sample_rate_hz      = 0;
    float*          samples             = nullptr;  // interleaved float [-1,1]; host-allocated
    uint32_t        capacity            = 0;
    uint32_t        written             = 0;
};
```

---

## Port

Extends the existing open-collector signal model with optional output descriptors. All existing attach/detach/signal API unchanged.

```cpp
class Port : public ComponentBase {
public:
    explicit Port(const PortDefinition& def, int port_index = 0);

    // Existing API — unchanged
    const PortDefinition& get_definition()    const;
    PortType              get_type()           const;
    const char*                get_name()           const;
    int                        get_port_index()     const;
    uint32_t                   read_signals()       const;
    void   write_system_signals(uint32_t mask, uint32_t value);
    bool   attach_device(PeripheralDevice* device);
    void   detach_device(PeripheralDevice* device = nullptr);
    void   set_signal_change_callback(SignalChangeCallback cb);
    void   notify_device_output_changed(uint32_t device_signals);

    // Output signal descriptors — absent on input and bus ports
    void         set_video_output(VideoOutput vo) { video_output_ = vo; }
    void         set_audio_output(AudioOutput ao) { audio_output_ = ao; }
    VideoOutput* video_output() { return video_output_ ? &*video_output_ : nullptr; }
    AudioOutput* audio_output() { return audio_output_ ? &*audio_output_ : nullptr; }

    // ComponentBase
    const char* component_name() const override { return get_definition().name; }
    void        reset()          override;

private:
    std::optional<VideoOutput> video_output_;
    std::optional<AudioOutput> audio_output_;
    // existing private members unchanged
};
```

---

## BusMap

Extracted from `Board` (formerly `BusMemory`). Owns address-decode logic only — page table wiring, MMIO handler registration, sub-table management. Holds no chip lifetime, no buffer ownership.

`BusMap` takes a non-owning pointer to the unified buffer (owned by `Board`) and non-owning chip pointers (owned by `Board`) during `apply()`.

```cpp
template<BusSpecConcept Spec>
class BusMap {
public:
    using Bus = MemoryBus<Spec>;

    template<size_t N>
    explicit BusMap(const ChipManifest<N>& manifest);

    // Wire page tables, MMIO handlers, and sub-tables from bound chips.
    // Buffer pointer comes from Board — BusMap does not own it.
    void apply(Bus& bus, uint8_t* unified_buffer, size_t viewer_id = 0);

    // Manual page mapping helpers (for bank switching etc.)
    void map_chip_read (Bus& bus, size_t viewer_id, size_t first_page, ChipId base_id) const noexcept;
    void map_chip_write(Bus& bus, size_t viewer_id, size_t first_page, ChipId base_id) const noexcept;
    void map_chip      (Bus& bus, size_t viewer_id, size_t first_page, ChipId base_id) const noexcept;

    // Slot layout queries — page counts, base ids, buffer offsets
    size_t base_id      (size_t slot_index) const noexcept;
    size_t slot_count   ()                  const noexcept;

    // Slot record access (for binding chips to slots)
    SlotRecord&       slot(size_t index)       noexcept;
    const SlotRecord& slot(size_t index) const noexcept;
    std::span<const SlotRecord> slots()  const noexcept;

    // Bind a chip to a slot — sets mmio_idx, sub_table_idx on the record.
    // Called by Board after chip creation.
    void bind_chip(size_t slot_index, ChipBase* chip) noexcept;

private:
    std::vector<SlotRecord> slots_;
    size_t mmio_count_ = 0;
    // MMIO trampoline statics
    static bus_state_t mmio_read_trampoline_ (void* ctx, bus_state_t bus) noexcept;
    static bus_state_t mmio_write_trampoline_(void* ctx, bus_state_t bus) noexcept;
};
```

---

## BoardBase and Board\<Spec\>

`BoardBase` is the non-templated base. It owns the component list as a non-owning index (raw pointers) into chip and port storage that `Board<Spec>` owns.

```cpp
class BoardBase : public ComponentBase {
public:
    virtual void reset()    override = 0;
    virtual void power_on() {}
    virtual void tick()     = 0;

    std::span<ComponentBase* const> components() const { return components_; }

    ComponentBase* find_component(std::string_view name) const {
        for (auto* c : components_)
            if (c->component_name() == name) return c;
        return nullptr;
    }

    template<typename T>
    T* find_component() const {
        for (auto* c : components_)
            if (auto* t = dynamic_cast<T*>(c)) return t;
        return nullptr;
    }

    template<typename T>
    std::vector<T*> find_components() const {
        std::vector<T*> result;
        for (auto* c : components_)
            if (auto* t = dynamic_cast<T*>(c)) result.push_back(t);
        return result;
    }

protected:
    void register_component(ComponentBase* c) {
        components_.push_back(c);
    }

private:
    std::vector<ComponentBase*> components_;  // non-owning; lifetime in Board<Spec>
};
```

`Board<Spec>` is the renamed and refactored `BusMemory<Spec>`. It retains everything from the current `BusMemory` except the address-decode logic, which moves to `BusMap`. Chip ownership, unified buffer ownership, factory creation, dynamic chip pool, and ROM loading all stay here.

```cpp
template<BusSpecConcept Spec>
class Board : public BoardBase {
public:
    using Bus     = MemoryBus<Spec>;
    using Map     = BusMap<Spec>;
    using ChipId  = typename Bus::ChipId;

    static constexpr size_t kPageSize = Bus::kPageSize;
    static constexpr size_t kPageBits = Spec::PageBits;

    template<size_t N>
    explicit Board(const ChipManifest<N>& manifest)
        : bus_map_(manifest) {
        const size_t total = manifest.total_pages(kPageBits);
        buffer_.assign(total * kPageSize, uint8_t(0xFF));
    }

    Bus& mem_bus() { return mem_bus_; }
    Map& bus_map() { return bus_map_; }

    // Chip ownership
    std::span<const std::unique_ptr<ChipBase>> owned_chips() const noexcept {
        return owned_chips_;
    }

    // Chip creation — from manifest slot factories
    using ConditionFn = bool (*)(uint16_t condition, const void* context);
    void create_chips(const bus_state_t* system_bus,
                      ConditionFn condition_fn  = nullptr,
                      const void* condition_ctx = nullptr);

    // Chip binding
    void bind_chip(size_t slot_index, ChipBase* chip);

    // Typed chip access
    template<typename T> T* chip_as(size_t slot_index) noexcept;
    template<typename T> T* first_chip(std::initializer_list<size_t> indices) noexcept;

    // Apply — wires page tables and MMIO via BusMap
    void apply(size_t viewer_id = 0) {
        bus_map_.apply(mem_bus_, buffer_.data(), viewer_id);
    }

    // Buffer access
    uint8_t*       chip_buffer(ChipId base_id)       noexcept;
    const uint8_t* chip_buffer(ChipId base_id) const noexcept;
    bool load(ChipId base_id, std::span<const uint8_t> data) noexcept;
    void fill(ChipId base_id, uint8_t value = 0xFF)  noexcept;

    // Lifecycle
    void reset_chips() noexcept {
        for (auto& chip : owned_chips_)
            if (chip) chip->reset();
    }

    // Dynamic chip pool
    static constexpr ChipId kInvalidChipId = ChipId(~ChipId(0));
    ChipId add_chip   (std::string_view name, size_t num_pages, bool read_only = false) noexcept;
    void   remove_chip(ChipId base_id) noexcept;

    // Variadic initialize — bind + apply in one call
    template<typename... Chips>
    void initialize(Bus& bus, Chips*... chips);

protected:
    void add_port(std::unique_ptr<Port> port) {
        ports_.push_back(std::move(port));
    }

    // Call after create_chips() + apply() — registers all chips and
    // ports as non-owning pointers in BoardBase::components_
    void register_board_components() {
        for (auto& chip : owned_chips_)
            register_component(chip.get());
        for (auto& port : ports_)
            register_component(port.get());
    }

    std::vector<uint8_t>                    buffer_;       // unified address-space buffer
    std::vector<std::unique_ptr<ChipBase>>  owned_chips_;  // chip ownership
    std::vector<std::unique_ptr<Port>> ports_;
    Bus mem_bus_;
    Map bus_map_;
};
```

---

## Concrete Board Pattern (VIC-20 as reference)

```cpp
class VIC20Board : public Board<VIC20BusTraits::Spec> {
public:
    VIC20Board() : Board(kVIC20Chips) {}

    bool initialize(const rom_config_t* roms, const VIC20Config& config);

    void reset()    override;
    void power_on() override;
    void tick()     override;

    // Non-owning convenience accessors — pointers into owned_chips_
    MOS6502*    cpu()  const { return cpu_;  }
    vic_base_t* vic()  const { return vic_;  }
    mos6522_t*  via1() const { return via1_; }
    mos6522_t*  via2() const { return via2_; }

private:
    RAMChip*    ram_        = nullptr;
    ROMChip*    charrom_    = nullptr;
    ROMChip*    basic_rom_  = nullptr;
    ROMChip*    kernal_rom_ = nullptr;
    MOS6502*    cpu_        = nullptr;
    vic_base_t* vic_        = nullptr;
    mos6522_t*  via1_       = nullptr;
    mos6522_t*  via2_       = nullptr;

    bus_state_t mem_tick(bus_state_t s);
    bus_state_t io_tick(bus_state_t s);
    void        setup_expansion_map();
    void        setup_cartridge_pages(bool present);

    static uint8_t vic_mem_read   (void* ctx, uint16_t addr);
    static uint8_t vic_color_read (void* ctx, uint16_t addr);
    static uint8_t via2_port_a_read(void* ctx, uint8_t out);
    static uint8_t via2_port_b_read(void* ctx, uint8_t out);
};
```

`VIC20Board::initialize()` ends with:

```cpp
// Cache convenience pointers — ownership stays in owned_chips_
ram_        = chip_as<RAMChip>   (vic20_slot::kRam);
charrom_    = chip_as<ROMChip>   (vic20_slot::kCharRom);
basic_rom_  = chip_as<ROMChip>   (vic20_slot::kBasicRom);
kernal_rom_ = chip_as<ROMChip>   (vic20_slot::kKernalRom);
cpu_        = chip_as<MOS6502>   (vic20_slot::kCpu);
vic_        = first_chip<vic_base_t>({vic20_slot::kVicPal, vic20_slot::kVicNtsc});
via1_       = chip_as<mos6522_t> (vic20_slot::kVia1);
via2_       = chip_as<mos6522_t> (vic20_slot::kVia2);

// Ports
auto composite = std::make_unique<Port>(
    PortRegistry::instance().lookup(PortType::VIDEO_COMPOSITE));
composite->set_video_output(VideoOutput{
    .signal_type        = VideoSignalType::Composite,
    .width              = 284,
    .height             = 284,
    .refresh_rate_hz    = 50.125f,
    .dot_clock_mhz      = 4.43361875f,
    .pixel_aspect_ratio = 1.0f,
});
add_port(std::move(composite));

auto audio_jack = std::make_unique<Port>(
    PortRegistry::instance().lookup(PortType::AUDIO_MONO));
audio_jack->set_audio_output(AudioOutput{
    .signal_type    = AudioSignalType::Mono,
    .sample_rate_hz = 44100,
});
add_port(std::move(audio_jack));

add_port(make_control_port(0));
add_port(make_iec_port());
add_port(make_cassette_port());
add_port(make_user_port());
add_port(make_expansion_port());

register_board_components();
```

`VIC20System` retains only system-level concerns: configuration, ROM loading, file format handling, BASIC injection, and the `CommodoreSystem` interface. All hardware operations delegate to `VIC20Board`.

---

## System

```cpp
class System {
public:
    void add_board(std::unique_ptr<BoardBase> board);

    BoardBase*       primary_board()       { return boards_.front().get(); }
    const BoardBase* primary_board() const { return boards_.front().get(); }

    std::span<const std::unique_ptr<BoardBase>> boards() const { return boards_; }

    void reset();
    void power_on();

    void add_connection(std::unique_ptr<Connection> connection);

private:
    std::vector<std::unique_ptr<BoardBase>>  boards_;
    std::vector<std::unique_ptr<Connection>> connections_;
};
```

---

## Session

```cpp
class Session {
public:
    void add_system(std::string_view name, std::unique_ptr<System> system);

    System*       primary_system()       { return systems_.front().system.get(); }
    const System* primary_system() const { return systems_.front().system.get(); }

    size_t system_count() const { return systems_.size(); }

    void   set_focused_system(size_t index);
    size_t focused_system_index() const { return focused_index_; }

    void reset();
    void power_on();

private:
    struct Entry {
        std::string             name;
        std::unique_ptr<System> system;
    };
    std::vector<Entry>                        systems_;
    std::vector<std::unique_ptr<Connection>>  connections_;
    size_t                                    focused_index_ = 0;
};
```

---

## Connections

```cpp
class Connection {
public:
    virtual ~Connection() = default;
    virtual void connect   (Port* a, Port* b) = 0;
    virtual void disconnect() = 0;
};

class DirectConnection : public Connection {
public:
    void connect   (Port* a, Port* b) override;
    void disconnect() override;
private:
    Port* a_ = nullptr;
    Port* b_ = nullptr;
};

// Future
class ChannelConnection : public Connection {};  // cross-thread, lock-free queue
class IPCConnection     : public Connection {};  // cross-process
class NetworkConnection : public Connection {};  // cross-machine
```

`DirectConnection::connect()` registers `SignalChangeCallback`s on both ports so each sees the other's output via the existing open-collector AND mechanism.

---

## SessionGUI Output Buffer Attachment

Before starting the emu thread, `SessionGUI` walks all boards and attaches host buffers to every output port:

```cpp
void SessionGUI::attach_output_buffers() {
    for (auto& board : session_->primary_system()->boards()) {
        for (auto* component : board->components()) {
            auto* port = dynamic_cast<Port*>(component);
            if (!port) continue;
            if (auto* vo = port->video_output())
                vo->pixels = new uint32_t[vo->width * vo->height];
            if (auto* ao = port->audio_output()) {
                ao->samples  = new float[desired_capacity];
                ao->capacity = desired_capacity;
            }
        }
    }
}
```

Multiple video outputs on the same board each get their own buffer. The GUI presents them as selectable display sources.

---

## Drive1541 GUI Smell

`pending_drive_insert_` and `poll_drive_file_dialog_requests()` are removed from `SessionGUI` in the same commit that introduces `Drive1541Board`. Until then they stay untouched.

---

## File-Driven Systems

`GenericBusSpec` covers all file-driven systems: 16-bit address, 256-byte pages, one viewer, up to 32 chip ids, 8 MMIO handlers, 2 masked sub-tables. `GenericSystem` implements `System` and is constructed from a parsed declaration. Registered in `SystemRegistry` once. Single-board declarations have no explicit board section. Complex banking, indexed sub-tables, multi-viewer aliasing, and PLA logic are never expressed in files.

---

## Phase Plan

| Priority | Work item |b
|----------|-----------|
| 1 | ~~`ComponentBase`; `ChipBase` and `Port` derive from it~~ |
| 2 | ~~Rename `GenericEmulatorGUI` → `EmulatorHost`; `SystemGUI` → `SessionGUI`~~ |
| 3 | ~~Rename `EmulatedSystem` → `System`~~ |
| 4 | ~~`PortType` A/V output variants; `VideoOutput` / `AudioOutput` descriptors; `Port` optional output fields~~ |
| 5 | ~~Rename `BusMemory` → `Board`~~; ~~`BoardBase` non-owning component index~~; ~~extract `BusMap` from address-decode logic~~; ~~chip and port ownership on `Board`~~ |
| 6 | Deferred — `VIC20Board` migration; `VIC20System` stripped to system-level concerns |
| 7 | ~~`Session` composes systems~~; ~~`System` composes boards~~; `SessionGUI` owns `Session` |
| 8 | ~~`PortRegistry`; `REGISTER_PORT`; standard ports self-register~~ |
| 9 | ~~`DirectConnection`; inter-board wiring~~ |
| 10 | Internal device auto-attachment via `DeviceRegistry` during board init |
| 11 | `power_on()` lifecycle; unified `Board`-level reset path |
| 12 | `GenericBusSpec`; `GenericSystem`; file parser; register with `SystemRegistry` |
| 13 | `BoardThread`; secondary board scheduling; `ChannelConnection` |
| 14 | `Drive1541Board`; disk-slot port; remove `SessionGUI` drive smell |
| 15 | Multi-system `Session`; framebuffer composition; audio mixing; input dispatch |
| Deferred | `IPCConnection`; `NetworkConnection`; cross-process / cross-machine sessions |