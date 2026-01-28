#pragma once

#include <cstdint>
#include <type_traits>
#include <array>
#include <stdexcept>
#include <cstring>

// =============================================================================
// DYNAMIC VERSION: Runtime allocation with small-vector optimization
// =============================================================================
template<typename StorageT = uint64_t>
class ShiftRegister {
    static_assert(std::is_unsigned_v<StorageT>, "Storage type must be unsigned");
    
    struct Pipe {
        uint8_t start;           // LSB position (injection point)
        uint8_t width;           // Bit width
        StorageT lsb_mask;       // Injection point
        StorageT msb_mask;       // Check point (bit about to exit)
        StorageT data_mask;      // Full pipe mask
        StorageT width_mask;     // (1 << width) - 1, unshifted for reuse
    };
    
    static constexpr size_t InlinePipeCount = 8;
    static constexpr uint8_t StorageBits = sizeof(StorageT) * 8;
    static constexpr uint8_t MinPipeWidth = 2;
    static constexpr uint8_t MaxPipes = StorageBits / 3;  // min 2 bits + 1 spacing
    
    StorageT storage_ = 0;
    StorageT spacing_mask_ = 0;
    
    alignas(alignof(Pipe)) std::array<Pipe, InlinePipeCount> inline_pipes_;
    Pipe* pipes_ = inline_pipes_.data();
    uint8_t pipe_count_ = 0;
    uint8_t capacity_ = InlinePipeCount;
    uint8_t next_bit_ = 0;

public:
    ShiftRegister() = default;
    ~ShiftRegister() { if (pipes_ != inline_pipes_.data()) delete[] pipes_; }
    
    ShiftRegister(const ShiftRegister&) = delete;
    ShiftRegister& operator=(const ShiftRegister&) = delete;
    
    uint8_t ReservePipe(uint8_t bits) {
        if (bits < MinPipeWidth) [[unlikely]]
            throw std::invalid_argument("Pipe width must be >= 2");
        if (next_bit_ + bits > StorageBits) [[unlikely]]
            throw std::overflow_error("Storage exhausted");
        if (pipe_count_ >= MaxPipes) [[unlikely]]
            throw std::overflow_error("Too many pipes");
            
        if (pipe_count_ >= capacity_) [[unlikely]] {
            uint8_t new_cap = capacity_ * 2;
            Pipe* new_pipes = new Pipe[new_cap];
            std::memcpy(new_pipes, pipes_, pipe_count_ * sizeof(Pipe));
            if (pipes_ != inline_pipes_.data()) delete[] pipes_;
            pipes_ = new_pipes;
            capacity_ = new_cap;
        }
        
        Pipe& p = pipes_[pipe_count_];
        p.start = next_bit_;
        p.width = bits;
        p.lsb_mask = StorageT(1) << next_bit_;
        p.msb_mask = StorageT(1) << (next_bit_ + bits - 1);
        
        StorageT width_mask = (bits >= StorageBits) ? ~StorageT(0) : ((StorageT(1) << bits) - 1);
        p.width_mask = width_mask;
        p.data_mask = width_mask << next_bit_;
        
        next_bit_ += bits;
        if (next_bit_ < StorageBits) [[likely]] {
            spacing_mask_ |= (StorageT(1) << next_bit_);
            next_bit_ += 1;
        }
        
        return pipe_count_++;
    }
    
    // Shift all pipes left by one (LSB -> MSB)
    [[gnu::always_inline, gnu::hot]]
    constexpr void Shift() noexcept {
        storage_ <<= 1;
        storage_ &= ~spacing_mask_;
    }
    
    // Inject single bit (set to 1) - for when you know you want to set
    [[gnu::always_inline]]
    constexpr void Inject(uint8_t pipe_id) noexcept {
        storage_ |= pipes_[pipe_id].lsb_mask;
    }
    
    // Inject multi-bit value at LSB (template to avoid ambiguity)
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    [[gnu::always_inline]]
    constexpr void Inject(uint8_t pipe_id, T value) noexcept {
        const Pipe& p = pipes_[pipe_id];
        const StorageT val = static_cast<StorageT>(value);
        storage_ = (storage_ & ~p.data_mask) | ((val << p.start) & p.data_mask);
    }
    
    // Check MSB (bit about to shift out)
    [[gnu::always_inline, nodiscard]]
    constexpr bool Check(uint8_t pipe_id) const noexcept {
        return (storage_ & pipes_[pipe_id].msb_mask) != 0;
    }
    
    // Read full pipe value (right-aligned)
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Read(uint8_t pipe_id) const noexcept {
        const Pipe& p = pipes_[pipe_id];
        return (storage_ >> p.start) & p.width_mask;
    }
    
    // Extract: atomically read and clear (consume pattern)
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Extract(uint8_t pipe_id) noexcept {
        const Pipe& p = pipes_[pipe_id];
        const StorageT val = (storage_ >> p.start) & p.width_mask;
        storage_ &= ~p.data_mask;
        return val;
    }
    
    // Clear specific pipe
    [[gnu::always_inline]]
    constexpr void ClearPipe(uint8_t pipe_id) noexcept {
        storage_ &= ~pipes_[pipe_id].data_mask;
    }
    
    // Check if any data present in register
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any() const noexcept {
        return storage_ != 0;
    }
    
    // Check if specific pipe has data
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any(uint8_t pipe_id) const noexcept {
        return (storage_ & pipes_[pipe_id].data_mask) != 0;
    }
    
    // Population count
    [[nodiscard]]
    constexpr int PopCount() const noexcept {
        // Builtin popcount for GCC/Clang
        if constexpr (sizeof(StorageT) == 8) {
            return __builtin_popcountll(storage_);
        } else if constexpr (sizeof(StorageT) == 4) {
            return __builtin_popcount(storage_);
        } else {
            return __builtin_popcount(storage_);
        }
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Mask(uint8_t pipe_id) const noexcept {
        return pipes_[pipe_id].data_mask;
    }
    
    [[nodiscard]]
    constexpr StorageT ComputePipeMask() const noexcept {
        StorageT mask = 0;
        for (uint8_t i = 0; i < pipe_count_; i++) {
            mask |= pipes_[i].data_mask;
        }
        return mask;
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Raw() const noexcept { return storage_; }
    
    [[gnu::always_inline]]
    constexpr void Raw(StorageT val) noexcept { 
        storage_ = val & ComputePipeMask(); 
    }
    
    [[gnu::always_inline]]
    constexpr void Clear() noexcept { storage_ = 0; }
    
    // Accessors
    [[nodiscard]] constexpr uint8_t PipeStart(uint8_t id) const noexcept { return pipes_[id].start; }
    [[nodiscard]] constexpr uint8_t PipeWidth(uint8_t id) const noexcept { return pipes_[id].width; }
    [[nodiscard]] constexpr uint8_t PipeCount() const noexcept { return pipe_count_; }
    [[nodiscard]] constexpr uint8_t RemainingBits() const noexcept { return StorageBits - next_bit_; }
    [[nodiscard]] static constexpr uint8_t GetMaxPipes() noexcept { return MaxPipes; }
};

// =============================================================================
// STATIC VERSION: Zero-overhead compile-time layout
// =============================================================================
template<typename StorageT, uint8_t... PipeWidths>
class StaticShiftRegister {
    static_assert(std::is_unsigned_v<StorageT>, "Storage type must be unsigned");
    static_assert(sizeof...(PipeWidths) > 0, "Must specify at least one pipe");
    static_assert(((PipeWidths >= 2) && ...), "All pipe widths must be >= 2");
    
    static constexpr uint8_t StorageBits = sizeof(StorageT) * 8;
    static constexpr uint8_t PipeCount = sizeof...(PipeWidths);
    static constexpr std::array<uint8_t, PipeCount> WidthArray{PipeWidths...};
    
    template<size_t N>
    static constexpr uint8_t GetWidth() { return WidthArray[N]; }
    
    // Efficient fold-expression offset computation
    template<size_t Target, size_t... Is>
    static constexpr uint8_t ComputeOffsetImpl(std::index_sequence<Is...>) {
        return ((Is < Target ? (GetWidth<Is>() + 1) : 0) + ...);
    }
    
    template<uint8_t Index>
    static constexpr uint8_t ComputeOffset() {
        return ComputeOffsetImpl<Index>(std::make_index_sequence<PipeCount>{});
    }
    
    // Pipe descriptor - all constants computed at compile time
    template<uint8_t Index>
    struct PipeDesc {
        static constexpr uint8_t Width = GetWidth<Index>();
        static constexpr uint8_t Start = ComputeOffset<Index>();
        static constexpr uint8_t End = Start + Width - 1;
        static constexpr StorageT LsbMask = StorageT(1) << Start;
        static constexpr StorageT MsbMask = StorageT(1) << End;
        static constexpr StorageT WidthMask = (Width >= StorageBits) ? ~StorageT(0) : 
                                              ((StorageT(1) << Width) - 1);
        static constexpr StorageT DataMask = WidthMask << Start;
        
        static_assert(End < StorageBits, "Pipe exceeds storage");
    };
    
    // Pre-compute masks at compile time using fold expressions
    template<size_t... Is>
    static constexpr StorageT ComputePipeMask(std::index_sequence<Is...>) {
        return (PipeDesc<Is>::DataMask | ...);
    }
    
    template<size_t... Is>
    static constexpr StorageT ComputeSpacingMask(std::index_sequence<Is...>) {
        return (((PipeDesc<Is>::End + 1 < StorageBits) ? 
                (StorageT(1) << (PipeDesc<Is>::End + 1)) : 0) | ...);
    }
    
    static constexpr StorageT PipeMask = 
        ComputePipeMask(std::make_index_sequence<PipeCount>{});
    static constexpr StorageT SpacingMask = 
        ComputeSpacingMask(std::make_index_sequence<PipeCount>{});
    static constexpr bool HasSpacing = (SpacingMask != 0);
    
    StorageT storage_ = 0;

public:
    // Zero-storage pipe handle - pure type tag (no data)
    template<uint8_t PipeIndex>
    class Pipe {
        static_assert(PipeIndex < PipeCount, "Pipe index out of bounds");
        
    public:
        using P = PipeDesc<PipeIndex>;
        static constexpr uint8_t Index = PipeIndex;
        
        constexpr Pipe() noexcept = default;
        
        [[nodiscard]] static constexpr StorageT Mask() noexcept { return P::DataMask; }
        [[nodiscard]] static constexpr uint8_t Width() noexcept { return P::Width; }
        [[nodiscard]] static constexpr uint8_t Start() noexcept { return P::Start; }
        [[nodiscard]] static constexpr uint8_t End() noexcept { return P::End; }
    };
    
    // Pipe operations - pass Pipe as parameter to capture index at compile time
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Inject(Pipe<PipeIndex>) noexcept {
        storage_ |= PipeDesc<PipeIndex>::LsbMask;
    }
    
    template<uint8_t PipeIndex, typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    [[gnu::always_inline]]
    constexpr void Inject(Pipe<PipeIndex>, T value) noexcept {
        using P = PipeDesc<PipeIndex>;
        const StorageT val = static_cast<StorageT>(value);
        if constexpr (P::Width >= StorageBits) {
            storage_ = val;
        } else {
            storage_ = (storage_ & ~P::DataMask) | ((val << P::Start) & P::DataMask);
        }
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline, nodiscard]]
    constexpr bool Check(Pipe<PipeIndex>) const noexcept {
        return (storage_ & PipeDesc<PipeIndex>::MsbMask) != 0;
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Read(Pipe<PipeIndex>) const noexcept {
        using P = PipeDesc<PipeIndex>;
        if constexpr (P::Start == 0) {
            return storage_ & P::WidthMask;
        } else {
            return (storage_ >> P::Start) & P::WidthMask;
        }
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Extract(Pipe<PipeIndex>) noexcept {
        using P = PipeDesc<PipeIndex>;
        const StorageT val = Read(Pipe<PipeIndex>{});
        storage_ &= ~P::DataMask;
        return val;
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Clear(Pipe<PipeIndex>) noexcept {
        storage_ &= ~PipeDesc<PipeIndex>::DataMask;
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any(Pipe<PipeIndex>) const noexcept {
        return (storage_ & PipeDesc<PipeIndex>::DataMask) != 0;
    }
    
    // Shift all pipes left by one
    [[gnu::always_inline, gnu::hot]]
    constexpr void Shift() noexcept {
        storage_ <<= 1;
        if constexpr (HasSpacing) {
            storage_ &= ~SpacingMask;
        }
    }
    
    // Check if any data present
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any() const noexcept { 
        return storage_ != 0; 
    }
    
    [[gnu::always_inline]]
    constexpr void Clear() noexcept { 
        storage_ = 0; 
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Raw() const noexcept { 
        return storage_; 
    }
    
    [[gnu::always_inline]]
    constexpr void Raw(StorageT val) noexcept { 
        storage_ = val & PipeMask; 
    }
    
    [[nodiscard]] static constexpr uint8_t GetPipeCount() noexcept { return PipeCount; }
    [[nodiscard]] static constexpr StorageT GetPipeMask() noexcept { return PipeMask; }
};
