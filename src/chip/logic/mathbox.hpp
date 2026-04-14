#pragma once
/*
 * mathbox.hpp — Atari Math Box (Battlezone, Red Baron, Tempest)
 *
 * The Math Box is a custom hardware computation module built from
 * AM2901 bit-slice processors with custom microcode PROMs.  It
 * provides 16-bit multiply, divide, and rotation operations used
 * by Battlezone (3D tank positioning), Red Baron (3D flight), and
 * Tempest (tube perspective transformation).
 *
 * Interface:
 *   - go_w(offset, data): Write to one of 32 addresses ($00-$1F).
 *     Each address either loads a register or triggers a computation.
 *     The result is stored in m_result and immediately available.
 *   - status_r(): Returns 0x00 (bit 7 = 0 = computation complete).
 *     In our emulation, all operations are instantaneous.
 *   - lo_r() / hi_r(): Return low/high byte of the last result.
 *
 * Based on MAME's mathbox.cpp by Eric Smith.
 */

#include "chip/logic/logic_chip_base.hpp"
#include <cstdint>
#include <cstring>

class MathBox : public LogicChipBase {
public:
    MathBox()
        : LogicChipBase(ChipInfo{"MB", "Atari", "Atari Math Box"}) {
        reset();
    }

    void reset() override {
        result_ = 0;
        std::memset(reg_, 0, sizeof(reg_));
    }

    // ── Bus interface ────────────────────────────────────────────

    /// Always returns 0x00: bit 7 = 0 means computation is complete.
    uint8_t status_r() const { return 0x00; }

    /// Low byte of the most recent computation result.
    uint8_t lo_r() const { return result_ & 0xFF; }

    /// High byte of the most recent computation result.
    uint8_t hi_r() const { return (result_ >> 8) & 0xFF; }

    /// Write to math box register / trigger computation.
    /// offset: 5-bit address ($00-$1F), data: 8-bit value.
    void go_w(uint8_t offset, uint8_t data) {
        int32_t mb_temp;
        int16_t mb_q;
        int msb;

        switch (offset & 0x1F) {
            // ── Register loads ──────────────────────────────────
            case 0x00: result_ = reg_[0] = (reg_[0] & 0xFF00) | data;          break;
            case 0x01: result_ = reg_[0] = (reg_[0] & 0x00FF) | (data << 8);   break;
            case 0x02: result_ = reg_[1] = (reg_[1] & 0xFF00) | data;          break;
            case 0x03: result_ = reg_[1] = (reg_[1] & 0x00FF) | (data << 8);   break;
            case 0x04: result_ = reg_[2] = (reg_[2] & 0xFF00) | data;          break;
            case 0x05: result_ = reg_[2] = (reg_[2] & 0x00FF) | (data << 8);   break;
            case 0x06: result_ = reg_[3] = (reg_[3] & 0xFF00) | data;          break;
            case 0x07: result_ = reg_[3] = (reg_[3] & 0x00FF) | (data << 8);   break;
            case 0x08: result_ = reg_[4] = (reg_[4] & 0xFF00) | data;          break;
            case 0x09: result_ = reg_[4] = (reg_[4] & 0x00FF) | (data << 8);   break;
            case 0x0A: result_ = reg_[5] = (reg_[5] & 0xFF00) | data;          break;
            // Note: no function loads low part of reg_[5] without performing a computation

            case 0x0C: result_ = reg_[6] = data; break;
            // Note: no function loads high part of reg_[6]

            case 0x0D: result_ = reg_[0xA] = (reg_[0xA] & 0xFF00) | data;        break;
            case 0x0E: result_ = reg_[0xA] = (reg_[0xA] & 0x00FF) | (data << 8); break;
            case 0x0F: result_ = reg_[0xB] = (reg_[0xB] & 0xFF00) | data;        break;
            case 0x10: result_ = reg_[0xB] = (reg_[0xB] & 0x00FF) | (data << 8); break;

            case 0x15: result_ = reg_[7] = (reg_[7] & 0xFF00) | data;          break;
            case 0x16: result_ = reg_[7] = (reg_[7] & 0x00FF) | (data << 8);   break;

            case 0x1A: result_ = reg_[8] = (reg_[8] & 0xFF00) | data;          break;
            case 0x1B: result_ = reg_[8] = (reg_[8] & 0x00FF) | (data << 8);   break;

            // ── Register read-back ─────────────────────────────
            case 0x17: result_ = reg_[7]; break;
            case 0x19: result_ = reg_[8]; break;
            case 0x18: result_ = reg_[9]; break;

            // ── Rotation / perspective (command 0x0B) ──────────
            case 0x0B:
                reg_[5] = (reg_[5] & 0x00FF) | (data << 8);
                reg_[0xF] = static_cast<int16_t>(0xFFFF);
                reg_[4] -= reg_[2];
                reg_[5] -= reg_[3];
                goto step_048;

            // ── Full rotation (command 0x11) ───────────────────
            case 0x11:
                reg_[5] = (reg_[5] & 0x00FF) | (data << 8);
                reg_[0xF] = 0x0000;
                goto step_048;

            // ── Rotation part 2 (command 0x12) ─────────────────
            case 0x12:
                mb_temp = static_cast<int32_t>(reg_[1]) * static_cast<int32_t>(reg_[4]);
                reg_[0xC] = mb_temp >> 16;
                reg_[9] = mb_temp & 0xFFFF;

                mb_temp = static_cast<int32_t>(reg_[0]) * static_cast<int32_t>(reg_[5]);
                reg_[8] = mb_temp >> 16;
                mb_q = mb_temp & 0xFFFF;

                reg_[8] += reg_[0xC];

                // Rounding
                reg_[9] = (reg_[9] >> 1) & 0x7FFF;
                reg_[0xC] = (mb_q >> 1) & 0x7FFF;
                reg_[9] += reg_[0xC];
                if (reg_[9] < 0)
                    reg_[8]++;
                reg_[9] <<= 1;

                result_ = reg_[8];

                if (reg_[0xF] < 0)
                    break;

                reg_[8] += reg_[3];
                reg_[9] &= static_cast<int16_t>(0xFF00);
                // fall through to command 0x13
                [[fallthrough]];

            // ── Division (command 0x13) ────────────────────────
            case 0x13:
                reg_[0xC] = reg_[9];
                mb_q = reg_[8];
                goto step_0bf;

            // ── Division with reg_A/B (command 0x14) ───────────
            case 0x14:
                reg_[0xC] = reg_[0xA];
                mb_q = reg_[0xB];
                goto step_0bf;

            // ── Window test (command 0x1C) ─────────────────────
            case 0x1C:
                reg_[5] = (reg_[5] & 0x00FF) | (data << 8);
                do {
                    reg_[0xE] = (reg_[4] + reg_[7]) >> 1;
                    reg_[0xF] = (reg_[5] + reg_[8]) >> 1;
                    if ((reg_[0xB] < reg_[0xE]) && (reg_[0xF] < reg_[0xE]) &&
                        ((reg_[0xE] + reg_[0xF]) >= 0)) {
                        reg_[7] = reg_[0xE];
                        reg_[8] = reg_[0xF];
                    } else {
                        reg_[4] = reg_[0xE];
                        reg_[5] = reg_[0xF];
                    }
                } while (--reg_[6] >= 0);
                result_ = reg_[8];
                break;

            // ── Distance calculation (command 0x1D) ────────────
            case 0x1D:
                reg_[3] = (reg_[3] & 0x00FF) | (data << 8);
                reg_[2] -= reg_[0];
                if (reg_[2] < 0) reg_[2] = -reg_[2];
                reg_[3] -= reg_[1];
                if (reg_[3] < 0) reg_[3] = -reg_[3];
                // fall through to command 0x1E
                [[fallthrough]];

            // ── Max + 3/8 min approximation (command 0x1E) ─────
            case 0x1E:
                if (reg_[3] >= reg_[2]) {
                    reg_[0xC] = reg_[2];
                    reg_[0xD] = reg_[3];
                } else {
                    reg_[0xD] = reg_[2];
                    reg_[0xC] = reg_[3];
                }
                reg_[0xC] >>= 2;
                reg_[0xD] += reg_[0xC];
                reg_[0xC] >>= 1;
                result_ = reg_[0xD] = (reg_[0xC] + reg_[0xD]);
                break;

            // ── Self-test (command 0x1F) ───────────────────────
            case 0x1F:
                break;

            default:
                break;
        }
        return;

        // ── Shared subroutine: rotation step 048 ──────────────────
    step_048:
        mb_temp = static_cast<int32_t>(reg_[0]) * static_cast<int32_t>(reg_[4]);
        reg_[0xC] = mb_temp >> 16;
        reg_[0xE] = mb_temp & 0xFFFF;

        mb_temp = static_cast<int32_t>(-reg_[1]) * static_cast<int32_t>(reg_[5]);
        reg_[7] = mb_temp >> 16;
        mb_q = mb_temp & 0xFFFF;

        reg_[7] += reg_[0xC];

        // Rounding
        reg_[0xE] = (reg_[0xE] >> 1) & 0x7FFF;
        reg_[0xC] = (mb_q >> 1) & 0x7FFF;
        mb_q = reg_[0xC] + reg_[0xE];
        if (mb_q < 0)
            reg_[7]++;

        result_ = reg_[7];

        if (reg_[0xF] < 0)
            return;

        reg_[7] += reg_[2];

        // Continue into command 0x12 logic
        mb_temp = static_cast<int32_t>(reg_[1]) * static_cast<int32_t>(reg_[4]);
        reg_[0xC] = mb_temp >> 16;
        reg_[9] = mb_temp & 0xFFFF;

        mb_temp = static_cast<int32_t>(reg_[0]) * static_cast<int32_t>(reg_[5]);
        reg_[8] = mb_temp >> 16;
        mb_q = mb_temp & 0xFFFF;

        reg_[8] += reg_[0xC];

        // Rounding
        reg_[9] = (reg_[9] >> 1) & 0x7FFF;
        reg_[0xC] = (mb_q >> 1) & 0x7FFF;
        reg_[9] += reg_[0xC];
        if (reg_[9] < 0)
            reg_[8]++;
        reg_[9] <<= 1;

        result_ = reg_[8];

        if (reg_[0xF] < 0)
            return;

        reg_[8] += reg_[3];
        reg_[9] &= static_cast<int16_t>(0xFF00);

        // Continue into command 0x13 (division)
        reg_[0xC] = reg_[9];
        mb_q = reg_[8];
        // fall through to step_0bf

        // ── Shared subroutine: division step 0bf ──────────────────
    step_0bf:
        reg_[0xE] = reg_[7] ^ mb_q;   // save sign of result
        reg_[0xD] = mb_q;
        if (mb_q >= 0) {
            mb_q = reg_[0xC];
        } else {
            reg_[0xD] = -mb_q - 1;
            mb_q = -reg_[0xC] - 1;
            if ((mb_q < 0) && ((mb_q + 1) < 0))
                reg_[0xD]++;
            mb_q++;
        }

        // abs(reg_[7])
        if (reg_[7] >= 0)
            reg_[0xC] = reg_[7];
        else
            reg_[0xC] = -reg_[7];

        reg_[0xF] = reg_[6];   // step counter

        do {
            reg_[0xD] -= reg_[0xC];
            msb = ((mb_q & 0x8000) != 0);
            mb_q <<= 1;
            if (reg_[0xD] >= 0)
                mb_q++;
            else
                reg_[0xD] += reg_[0xC];
            reg_[0xD] <<= 1;
            reg_[0xD] += msb;
        } while (--reg_[0xF] >= 0);

        if (reg_[0xE] >= 0)
            result_ = mb_q;
        else
            result_ = -mb_q;
        return;
    }

private:
    int16_t result_ = 0;       // Last computation result
    int16_t reg_[16] = {};     // 16 working registers
};
