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
        uint8_t start;
        uint8_t width;
        StorageT lsb_mask;
        StorageT msb_mask;
        StorageT data_mask;
        StorageT width_mask;
    };
    
    static constexpr size_t InlinePipeCount = 8;
    static constexpr uint8_t StorageBits = sizeof(StorageT) * 8;
    static constexpr uint8_t MinPipeWidth = 2;
    static constexpr uint8_t MaxPipes = StorageBits / 3;
    
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
    
    [[gnu::always_inline, gnu::hot]]
    constexpr void Shift() noexcept {
        storage_ <<= 1;
        storage_ &= ~spacing_mask_;
    }
    
    [[gnu::always_inline]]
    constexpr void Inject(uint8_t pipe_id, bool value) noexcept {
        const StorageT mask = pipes_[pipe_id].lsb_mask;
        storage_ = (storage_ & ~mask) | (static_cast<StorageT>(value) * mask);
    }
    
    [[gnu::always_inline]]
    constexpr void Inject(uint8_t pipe_id) noexcept {
        storage_ |= pipes_[pipe_id].lsb_mask;
    }
    
    [[gnu::always_inline]]
    constexpr void Inject(uint8_t pipe_id, StorageT value) noexcept {
        const Pipe& p = pipes_[pipe_id];
        storage_ = (storage_ & ~p.data_mask) | ((value << p.start) & p.data_mask);
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr bool Check(uint8_t pipe_id) const noexcept {
        return (storage_ & pipes_[pipe_id].msb_mask) != 0;
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Read(uint8_t pipe_id) const noexcept {
        const Pipe& p = pipes_[pipe_id];
        return (storage_ >> p.start) & p.width_mask;
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr StorageT Extract(uint8_t pipe_id) noexcept {
        const Pipe& p = pipes_[pipe_id];
        const StorageT val = (storage_ >> p.start) & p.width_mask;
        storage_ &= ~p.data_mask;
        return val;
    }
    
    [[gnu::always_inline]]
    constexpr void ClearPipe(uint8_t pipe_id) noexcept {
        storage_ &= ~pipes_[pipe_id].data_mask;
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any() const noexcept {
        return storage_ != 0;
    }
    
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any(uint8_t pipe_id) const noexcept {
        return (storage_ & pipes_[pipe_id].data_mask) != 0;
    }
    
    [[nodiscard]]
    constexpr int PopCount() const noexcept {
        if constexpr (sizeof(StorageT) == 8) {
            return __builtin_popcountll(storage_);
        } else if constexpr (sizeof(StorageT) <= 4) {
            return __builtin_popcount(static_cast<unsigned int>(storage_));
        } else {
            int count = 0;
            StorageT x = storage_;
            while (x) {
                count += x & 1;
                x >>= 1;
            }
            return count;
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
    
    [[nodiscard]] constexpr uint8_t PipeStart(uint8_t id) const noexcept { return pipes_[id].start; }
    [[nodiscard]] constexpr uint8_t PipeWidth(uint8_t id) const noexcept { return pipes_[id].width; }
    [[nodiscard]] constexpr uint8_t PipeCount() const noexcept { return pipe_count_; }
    [[nodiscard]] constexpr uint8_t RemainingBits() const noexcept { return StorageBits - next_bit_; }
    [[nodiscard]] static constexpr uint8_t GetMaxPipes() noexcept { return MaxPipes; }
};

// =============================================================================
// STATIC VERSION: Zero-storage type-tag approach
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
    
    template<size_t Target, size_t... Is>
    static constexpr uint8_t ComputeOffsetImpl(std::index_sequence<Is...>) {
        return ((Is < Target ? (GetWidth<Is>() + 1) : 0) + ...);
    }
    
    template<uint8_t Index>
    static constexpr uint8_t ComputeOffset() {
        return ComputeOffsetImpl<Index>(std::make_index_sequence<PipeCount>{});
    }
    
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
    // Zero-storage type tag - pure compile-time entity
    template<uint8_t PipeIndex>
    struct Pipe {
        static_assert(PipeIndex < PipeCount, "Pipe index out of bounds");
        using P = PipeDesc<PipeIndex>;
        
        static constexpr uint8_t Index = PipeIndex;
        static constexpr StorageT Mask = P::DataMask;
        static constexpr uint8_t Width = P::Width;
        static constexpr uint8_t Start = P::Start;
        static constexpr uint8_t End = P::End;
    };
    
    // Operations take type-tag by value (zero cost, just for template deduction)
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Inject(Pipe<PipeIndex>, bool value) noexcept {
        using P = PipeDesc<PipeIndex>;
        storage_ = (storage_ & ~P::LsbMask) | (static_cast<StorageT>(value) * P::LsbMask);
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Inject(Pipe<PipeIndex>) noexcept {
        storage_ |= PipeDesc<PipeIndex>::LsbMask;
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Inject(Pipe<PipeIndex>, StorageT value) noexcept {
        using P = PipeDesc<PipeIndex>;
        if constexpr (P::Width >= StorageBits) {
            storage_ = value;
        } else {
            storage_ = (storage_ & ~P::DataMask) | ((value << P::Start) & P::DataMask);
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
    constexpr StorageT Extract(Pipe<PipeIndex> pipe) noexcept {
        const StorageT val = Read(pipe);
        storage_ &= ~PipeDesc<PipeIndex>::DataMask;
        return val;
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Clear(Pipe<PipeIndex>) noexcept {
        storage_ &= ~PipeDesc<PipeIndex>::DataMask;
    }
    
    // Fill entire pipeline with 1s for continuous signals
    // This ensures Check() returns true immediately and after every shift
    // Matches old behavior: set entire pipeline mask as feed
    template<uint8_t PipeIndex>
    [[gnu::always_inline]]
    constexpr void Feed(Pipe<PipeIndex>) noexcept {
        storage_ |= PipeDesc<PipeIndex>::DataMask;
    }
    
    template<uint8_t PipeIndex>
    [[gnu::always_inline, nodiscard]]
    constexpr bool Any(Pipe<PipeIndex>) const noexcept {
        return (storage_ & PipeDesc<PipeIndex>::DataMask) != 0;
    }
    
    [[gnu::always_inline, gnu::hot]]
    constexpr void Shift() noexcept {
        storage_ <<= 1;
        if constexpr (HasSpacing) {
            storage_ &= ~SpacingMask;
        }
    }
    
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