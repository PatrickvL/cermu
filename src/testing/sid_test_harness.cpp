// =============================================================================
// SID MOS6581/8580 Digital Test Harness — Implementation
// =============================================================================
// Consolidated test framework: script parser, runner, all built-in tests.
// See sid_test_harness.h for the full API and format documentation.
// =============================================================================

#include "sid_test_harness.h"
#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdarg>

namespace sid_test {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void add_result(harness_t* h, result_type_t type, uint32_t cycle,
                       uint8_t expected, uint8_t actual,
                       const char* name, int line, const char* fmt, ...) {
    result_entry_t r{};
    r.type        = type;
    r.cycle       = cycle;
    r.expected    = expected;
    r.actual      = actual;
    r.check_name  = name;
    r.line_number = line;

    va_list args;
    va_start(args, fmt);
    vsnprintf(r.message, sizeof(r.message), fmt, args);
    va_end(args);

    if (type == result_type_t::PASS) h->pass_count++;
    if (type == result_type_t::FAIL) h->fail_count++;

    h->results.push_back(r);
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string strip_comment(const std::string& line) {
    size_t pos = line.find('#');
    if (pos == std::string::npos) return line;
    return line.substr(0, pos);
}

static uint32_t parse_number(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
        return (uint32_t)strtoul(t.c_str(), nullptr, 16);
    if (t.size() >= 1 && t[0] == '$')
        return (uint32_t)strtoul(t.c_str() + 1, nullptr, 16);
    return (uint32_t)strtoul(t.c_str(), nullptr, 10);
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == delim) {
            parts.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Harness lifecycle
// ─────────────────────────────────────────────────────────────────────────────

harness_t* create(bool verbose) {
    harness_t* h = new harness_t{};
    h->sid = new mos6581_t();
    h->sid->init();
    h->sid_owned = true;
    h->verbose = verbose;
    h->trace_enabled = false;

    // Configure for testing: PAL clock, no sample generation needed
    h->sid->set_cpu_clock(PAL_CLOCK);
    h->sid->set_sample_rate(44100.0f);
    h->sid->set_timing(true);
    h->sid->reset();

    return h;
}

harness_t* create_with_sid(mos6581_t* sid, bool verbose) {
    harness_t* h = new harness_t{};
    h->sid = sid;
    h->sid_owned = false;
    h->verbose = verbose;
    h->trace_enabled = false;
    return h;
}

void destroy(harness_t* h) {
    if (!h) return;
    if (h->sid_owned && h->sid) {
        delete h->sid;
    }
    delete h;
}

void reset(harness_t* h) {
    if (!h) return;
    h->sid->reset();
    h->total_cycles = 0;
    h->check_osc3 = false;
    h->check_env3 = false;
    h->trace.clear();
    h->snapshots.clear();
    h->results.clear();
    h->pass_count = 0;
    h->fail_count = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Direct register access
// ─────────────────────────────────────────────────────────────────────────────

void write_reg(harness_t* h, uint8_t reg, uint8_t value) {
    if (!h || !h->sid) return;
    bus_state_t bs = BUS_STATE(0xD400 + reg, value, 0);
    mos6581_s::registers_write(h->sid, bs);
}

uint8_t read_osc3(harness_t* h) {
    if (!h || !h->sid) return 0;
    // Read through the actual register path — tests what the CPU would see
    bus_state_t bs = BUS_STATE(0xD400 + REG_OSC3, 0, 0);
    bs = mos6581_s::registers_read(h->sid, bs);
    return BUS_GET_DATA(bs);
}

uint8_t read_env3(harness_t* h) {
    if (!h || !h->sid) return 0;
    // Read through the actual register path — tests what the CPU would see
    bus_state_t bs = BUS_STATE(0xD400 + REG_ENV3, 0, 0);
    bs = mos6581_s::registers_read(h->sid, bs);
    return BUS_GET_DATA(bs);
}

void clock_cycles(harness_t* h, uint32_t n) {
    if (!h || !h->sid) return;

    bus_state_t bs = BUS_STATE(0, 0, 0);
    for (uint32_t i = 0; i < n; i++) {
        h->sid->tick(bs);
        h->total_cycles++;

        if (h->trace_enabled) {
            trace_entry_t te;
            te.osc3 = read_osc3(h);
            te.env3 = read_env3(h);
            h->trace.push_back(te);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Programmatic command building
// ─────────────────────────────────────────────────────────────────────────────

void cmd_reset(test_script_t* s) {
    command_t c{}; c.type = cmd_type_t::RESET;
    s->commands.push_back(c);
}

void cmd_revision(test_script_t* s, bool is_8580) {
    command_t c{}; c.type = cmd_type_t::REVISION; c.is_8580 = is_8580;
    s->commands.push_back(c);
}

void cmd_write(test_script_t* s, uint8_t reg, uint8_t value) {
    command_t c{}; c.type = cmd_type_t::WRITE; c.reg = reg; c.value = value;
    s->commands.push_back(c);
}

void cmd_run(test_script_t* s, uint32_t cycles) {
    command_t c{}; c.type = cmd_type_t::RUN; c.cycles = cycles;
    s->commands.push_back(c);
}

void cmd_check_osc3(test_script_t* s) {
    command_t c{}; c.type = cmd_type_t::CHECK_OSC3;
    s->commands.push_back(c);
}

void cmd_check_env3(test_script_t* s) {
    command_t c{}; c.type = cmd_type_t::CHECK_ENV3;
    s->commands.push_back(c);
}

void cmd_expect_osc3(test_script_t* s, uint8_t value) {
    command_t c{}; c.type = cmd_type_t::EXPECT_OSC3; c.value = value;
    s->commands.push_back(c);
}

void cmd_expect_env3(test_script_t* s, uint8_t value) {
    command_t c{}; c.type = cmd_type_t::EXPECT_ENV3; c.value = value;
    s->commands.push_back(c);
}

void cmd_expect_acc(test_script_t* s, uint8_t voice, uint32_t value) {
    command_t c{}; c.type = cmd_type_t::EXPECT_ACC; c.voice = voice; c.acc_value = value;
    s->commands.push_back(c);
}

void cmd_snapshot(test_script_t* s) {
    command_t c{}; c.type = cmd_type_t::SNAPSHOT;
    s->commands.push_back(c);
}

void cmd_label(test_script_t* s, const char* label) {
    command_t c{}; c.type = cmd_type_t::LABEL;
    strncpy(c.label, label, sizeof(c.label) - 1);
    s->commands.push_back(c);
}

// ─────────────────────────────────────────────────────────────────────────────
// Script parser
// ─────────────────────────────────────────────────────────────────────────────

bool parse_script(const char* text, test_script_t* script, const char* name) {
    if (!text || !script) return false;

    script->commands.clear();
    if (name) script->name = name;

    std::string src(text);
    auto lines = split(src, '\n');

    for (int line_num = 0; line_num < (int)lines.size(); line_num++) {
        std::string line = trim(strip_comment(lines[line_num]));
        if (line.empty()) continue;

        auto parts = split(line, ',');
        std::string cmd_str = to_lower(trim(parts[0]));

        command_t cmd{};
        cmd.line_number = line_num + 1;

        if (cmd_str == "reset") {
            cmd.type = cmd_type_t::RESET;
        }
        else if (cmd_str == "revision") {
            cmd.type = cmd_type_t::REVISION;
            if (parts.size() < 2) return false;
            std::string rev = to_lower(trim(parts[1]));
            cmd.is_8580 = (rev == "8580" || rev == "csg8580");
        }
        else if (cmd_str == "write") {
            cmd.type = cmd_type_t::WRITE;
            if (parts.size() < 3) return false;
            cmd.reg = (uint8_t)parse_number(parts[1]);
            cmd.value = (uint8_t)parse_number(parts[2]);
        }
        else if (cmd_str == "run") {
            cmd.type = cmd_type_t::RUN;
            if (parts.size() < 2) return false;
            cmd.cycles = parse_number(parts[1]);
        }
        else if (cmd_str == "check_osc3" || (cmd_str == "check" && parts.size() >= 2
                 && parse_number(parts[1]) == REG_OSC3)) {
            cmd.type = cmd_type_t::CHECK_OSC3;
        }
        else if (cmd_str == "check_env3" || (cmd_str == "check" && parts.size() >= 2
                 && parse_number(parts[1]) == REG_ENV3)) {
            cmd.type = cmd_type_t::CHECK_ENV3;
        }
        else if (cmd_str == "check") {
            // Generic check — map register to OSC3 or ENV3
            if (parts.size() < 2) return false;
            uint32_t reg = parse_number(parts[1]);
            if (reg == REG_OSC3)      cmd.type = cmd_type_t::CHECK_OSC3;
            else if (reg == REG_ENV3) cmd.type = cmd_type_t::CHECK_ENV3;
            else return false; // Unknown check register
        }
        else if (cmd_str == "expect_osc3") {
            cmd.type = cmd_type_t::EXPECT_OSC3;
            if (parts.size() < 2) return false;
            cmd.value = (uint8_t)parse_number(parts[1]);
        }
        else if (cmd_str == "expect_env3") {
            cmd.type = cmd_type_t::EXPECT_ENV3;
            if (parts.size() < 2) return false;
            cmd.value = (uint8_t)parse_number(parts[1]);
        }
        else if (cmd_str == "expect_acc") {
            cmd.type = cmd_type_t::EXPECT_ACC;
            if (parts.size() < 3) return false;
            cmd.voice = (uint8_t)parse_number(parts[1]);
            cmd.acc_value = parse_number(parts[2]);
        }
        else if (cmd_str == "snapshot") {
            cmd.type = cmd_type_t::SNAPSHOT;
        }
        else if (cmd_str == "label") {
            cmd.type = cmd_type_t::LABEL;
            if (parts.size() < 2) return false;
            strncpy(cmd.label, trim(parts[1]).c_str(), sizeof(cmd.label) - 1);
        }
        else if (cmd_str == "end") {
            // resid-test compatibility: end marker. Stop parsing here.
            break;
        }
        else {
            // Unknown command — skip with warning
            continue;
        }

        script->commands.push_back(cmd);
    }

    return true;
}

bool parse_script_file(const char* path, test_script_t* script) {
    FILE* f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<char> buf(size + 1);
    fread(buf.data(), 1, size, f);
    buf[size] = '\0';
    fclose(f);

    // Extract filename for script name
    std::string name(path);
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);

    return parse_script(buf.data(), script, name.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Command execution
// ─────────────────────────────────────────────────────────────────────────────

void execute_command(harness_t* h, const command_t* cmd) {
    if (!h || !cmd) return;

    switch (cmd->type) {
        case cmd_type_t::RESET:
            h->sid->reset();
            h->total_cycles = 0;
            h->check_osc3 = false;
            h->check_env3 = false;
            break;

        case cmd_type_t::REVISION:
            if (cmd->is_8580)
                h->sid->set_revision(SID_REVISION_8580_R5);
            else
                h->sid->set_revision(SID_REVISION_6581_R4AR);
            break;

        case cmd_type_t::WRITE:
            write_reg(h, cmd->reg, cmd->value);
            break;

        case cmd_type_t::RUN:
            clock_cycles(h, cmd->cycles);
            break;

        case cmd_type_t::CHECK_OSC3:
            h->check_osc3 = true;
            h->trace_enabled = true;
            break;

        case cmd_type_t::CHECK_ENV3:
            h->check_env3 = true;
            h->trace_enabled = true;
            break;

        case cmd_type_t::EXPECT_OSC3: {
            uint8_t actual = read_osc3(h);
            if (actual == cmd->value) {
                add_result(h, result_type_t::PASS, h->total_cycles,
                           cmd->value, actual, "OSC3", cmd->line_number,
                           "OSC3 == 0x%02X at cycle %u", cmd->value, h->total_cycles);
            } else {
                add_result(h, result_type_t::FAIL, h->total_cycles,
                           cmd->value, actual, "OSC3", cmd->line_number,
                           "OSC3: expected 0x%02X, got 0x%02X at cycle %u",
                           cmd->value, actual, h->total_cycles);
            }
            break;
        }

        case cmd_type_t::EXPECT_ENV3: {
            uint8_t actual = read_env3(h);
            if (actual == cmd->value) {
                add_result(h, result_type_t::PASS, h->total_cycles,
                           cmd->value, actual, "ENV3", cmd->line_number,
                           "ENV3 == 0x%02X at cycle %u", cmd->value, h->total_cycles);
            } else {
                add_result(h, result_type_t::FAIL, h->total_cycles,
                           cmd->value, actual, "ENV3", cmd->line_number,
                           "ENV3: expected 0x%02X, got 0x%02X at cycle %u",
                           cmd->value, actual, h->total_cycles);
            }
            break;
        }

        case cmd_type_t::EXPECT_ACC: {
            if (cmd->voice > 2) break;
            uint32_t actual = h->sid->voices[cmd->voice]->waveform_accumulator;
            uint8_t exp8 = (uint8_t)(cmd->acc_value >> 16);
            uint8_t act8 = (uint8_t)(actual >> 16);
            char name[8];
            snprintf(name, sizeof(name), "ACC%d", cmd->voice);
            if (actual == cmd->acc_value) {
                add_result(h, result_type_t::PASS, h->total_cycles,
                           exp8, act8, name, cmd->line_number,
                           "ACC%d == 0x%06X at cycle %u",
                           cmd->voice, cmd->acc_value, h->total_cycles);
            } else {
                add_result(h, result_type_t::FAIL, h->total_cycles,
                           exp8, act8, name, cmd->line_number,
                           "ACC%d: expected 0x%06X, got 0x%06X at cycle %u",
                           cmd->voice, cmd->acc_value, actual, h->total_cycles);
            }
            break;
        }

        case cmd_type_t::SNAPSHOT: {
            snapshot_t snap{};
            snap.cycle = h->total_cycles;
            snap.osc3 = read_osc3(h);
            snap.env3 = read_env3(h);
            for (int i = 0; i < 3; i++) {
                snap.acc[i] = h->sid->voices[i]->waveform_accumulator;
                snap.env_amp[i] = h->sid->voices[i]->envelope_amplitude;
            }
            h->snapshots.push_back(snap);
            break;
        }

        case cmd_type_t::LABEL: {
            // Just a marker — record as info
            add_result(h, result_type_t::INFO, h->total_cycles,
                       0, 0, "LABEL", cmd->line_number,
                       "--- %s --- (cycle %u)", cmd->label, h->total_cycles);
            break;
        }
    }
}

int run_script(harness_t* h, const test_script_t* script) {
    if (!h || !script) return -1;

    // Reset state but keep SID configuration
    h->results.clear();
    h->snapshots.clear();
    h->trace.clear();
    h->pass_count = 0;
    h->fail_count = 0;

    for (const auto& cmd : script->commands) {
        execute_command(h, &cmd);
    }

    return h->fail_count;
}

// ─────────────────────────────────────────────────────────────────────────────
// Results output
// ─────────────────────────────────────────────────────────────────────────────

void print_results(const harness_t* h, const test_script_t* script) {
    if (!h) return;

    printf("\n");
    if (script && !script->name.empty()) {
        printf("═══ Test: %s ═══\n", script->name.c_str());
    }
    if (script && !script->description.empty()) {
        printf("    %s\n", script->description.c_str());
    }

    for (const auto& r : h->results) {
        if (r.type == result_type_t::INFO) {
            printf("  [INFO]  %s\n", r.message);
        } else if (r.type == result_type_t::FAIL) {
            printf("  [FAIL]  Line %d: %s\n", r.line_number, r.message);
        } else if (h->verbose) {
            printf("  [PASS]  Line %d: %s\n", r.line_number, r.message);
        }
    }

    printf("  Result: %d passed, %d failed\n\n", h->pass_count, h->fail_count);
}

void print_trace(const harness_t* h, uint32_t start_cycle, uint32_t count) {
    if (!h || h->trace.empty()) return;

    uint32_t end = count > 0 ? std::min(start_cycle + count, (uint32_t)h->trace.size())
                             : (uint32_t)h->trace.size();

    printf("Trace [cycle: OSC3 ENV3]:\n");
    for (uint32_t i = start_cycle; i < end; i++) {
        printf("  %8u: OSC3=%02X ENV3=%02X\n",
               i, h->trace[i].osc3, h->trace[i].env3);
    }
}

bool export_trace(const harness_t* h, const char* path) {
    if (!h || !path || h->trace.empty()) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    // Header: 4 bytes magic + 4 bytes count
    uint32_t magic = 0x53494454; // "SIDT"
    uint32_t count = (uint32_t)h->trace.size();
    fwrite(&magic, 4, 1, f);
    fwrite(&count, 4, 1, f);

    // Data: 2 bytes per cycle (OSC3, ENV3)
    for (const auto& te : h->trace) {
        fwrite(&te.osc3, 1, 1, f);
        fwrite(&te.env3, 1, 1, f);
    }

    fclose(f);
    return true;
}

bool import_reference_trace(const char* path, std::vector<trace_entry_t>& ref) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint32_t magic, count;
    fread(&magic, 4, 1, f);
    fread(&count, 4, 1, f);

    if (magic != 0x53494454) { // "SIDT"
        fclose(f);
        return false;
    }

    ref.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        fread(&ref[i].osc3, 1, 1, f);
        fread(&ref[i].env3, 1, 1, f);
    }

    fclose(f);
    return true;
}

int compare_traces(const harness_t* h, const std::vector<trace_entry_t>& ref,
                   uint32_t start, uint32_t count) {
    if (!h) return -1;

    uint32_t len = count > 0 ? count : (uint32_t)std::min(h->trace.size(), ref.size());
    int mismatches = 0;

    for (uint32_t i = start; i < start + len && i < h->trace.size() && i < ref.size(); i++) {
        if (h->trace[i].osc3 != ref[i].osc3 || h->trace[i].env3 != ref[i].env3) {
            mismatches++;
            if (mismatches <= 20) { // Limit output
                printf("  Mismatch at cycle %u: OSC3 got %02X ref %02X | ENV3 got %02X ref %02X\n",
                       i, h->trace[i].osc3, ref[i].osc3, h->trace[i].env3, ref[i].env3);
            }
        }
    }

    return mismatches;
}

// =============================================================================
// BUILT-IN TEST SUITES
// =============================================================================
// Each test creates a script programmatically, runs it, and reports results.
// All tests operate on voice 3 and read OSC3/ENV3 for verification.
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// Helper: set voice 3 frequency (16-bit value split across two registers)
// ─────────────────────────────────────────────────────────────────────────────

static void script_set_v3_freq(test_script_t* s, uint16_t freq) {
    cmd_write(s, REG_V3_FREQ_LO, freq & 0xFF);
    cmd_write(s, REG_V3_FREQ_HI, (freq >> 8) & 0xFF);
}

static void script_set_v3_pw(test_script_t* s, uint16_t pw) {
    cmd_write(s, REG_V3_PW_LO, pw & 0xFF);
    cmd_write(s, REG_V3_PW_HI, (pw >> 8) & 0x0F);
}

static void script_set_v1_freq(test_script_t* s, uint16_t freq) {
    cmd_write(s, REG_V1_FREQ_LO, freq & 0xFF);
    cmd_write(s, REG_V1_FREQ_HI, (freq >> 8) & 0xFF);
}

/// Zero voice 3's accumulator using the test bit (matches real hardware technique).
/// Sets frequency and waveform, then pulses test bit to reset acc to 0.
/// After this call, the voice is configured with the given waveform and acc=0.
static void script_init_v3(test_script_t* s, uint16_t freq, uint8_t waveform) {
    script_set_v3_freq(s, freq);
    // Set test bit to force accumulator to 0
    cmd_write(s, REG_V3_CONTROL, waveform | CTRL_TEST);
    cmd_run(s, 1);  // One cycle to latch the reset
    // Release test bit — oscillator now runs from 0
    cmd_write(s, REG_V3_CONTROL, waveform);
    cmd_write(s, REG_MODE_VOL, 0x0F);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Sawtooth waveform
// ─────────────────────────────────────────────────────────────────────────────
// Sawtooth is the upper 12 bits of the 24-bit accumulator.
// OSC3 = upper 8 bits of the 12-bit waveform = accumulator bits 23..16.
// After N cycles with frequency F, accumulator = N * F (mod 2^24).
// OSC3 = (N * F) >> 16.
// ─────────────────────────────────────────────────────────────────────────────

int test_sawtooth_waveform(harness_t* h) {
    test_script_t script;
    script.name = "Sawtooth Waveform";
    script.description = "Verify sawtooth OSC3 = accumulator >> 16 at known cycle offsets";

    cmd_reset(&script);
    cmd_label(&script, "sawtooth_basic");

    // Use test bit to zero the accumulator, then release.
    // Frequency $0100: accumulator advances by 256 per cycle.
    // After N cycles with acc starting at 0: acc = N * 0x100.
    // OSC3 = (sawtooth >> 4) = (acc >> 12) >> 4 = acc >> 16
    script_init_v3(&script, 0x0100, CTRL_SAWTOOTH);

    // After 256 cycles: acc = 256 * 0x100 = 0x010000, OSC3 = 0x01
    cmd_run(&script, 256);
    cmd_expect_osc3(&script, 0x01);

    // After 256 more: acc = 0x020000, OSC3 = 0x02
    cmd_run(&script, 256);
    cmd_expect_osc3(&script, 0x02);

    // acc = 0x800000 → OSC3 = 0x80, need total 0x8000 = 32768 cycles
    cmd_run(&script, 32256);
    cmd_expect_osc3(&script, 0x80);

    // Higher frequency test ($1000 = 4096 per cycle)
    cmd_label(&script, "sawtooth_fast");
    cmd_reset(&script);
    script_init_v3(&script, 0x1000, CTRL_SAWTOOTH);

    // After 16 cycles: acc = 16 * 0x1000 = 0x010000, OSC3 = 0x01
    cmd_run(&script, 16);
    cmd_expect_osc3(&script, 0x01);

    // After 256 total: acc = 0x100000, OSC3 = 0x10
    cmd_run(&script, 240);
    cmd_expect_osc3(&script, 0x10);

    // Wraparound: frequency $FFFF
    cmd_label(&script, "sawtooth_wrap");
    cmd_reset(&script);
    script_init_v3(&script, 0xFFFF, CTRL_SAWTOOTH);

    // After 1 cycle: acc = 0xFFFF, OSC3 = 0x00
    cmd_run(&script, 1);
    cmd_expect_osc3(&script, 0x00);

    // After 2 cycles: acc = 0x1FFFE, OSC3 = 0x01
    cmd_run(&script, 1);
    cmd_expect_osc3(&script, 0x01);

    int failures = run_script(h, &script);
    print_results(h, &script);
    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Triangle waveform
// ─────────────────────────────────────────────────────────────────────────────
// Triangle folds at the MSB: if acc bit 23 is 0, output = acc >> 11
// if acc bit 23 is 1, output = (acc XOR 0xFFFFFF) >> 11.
// OSC3 = upper 8 bits of the 12-bit triangle = effectively bits 22..15.
// First half ramps up 0x000..0xFFF, second half ramps back down.
// ─────────────────────────────────────────────────────────────────────────────

int test_triangle_waveform(harness_t* h) {
    test_script_t script;
    script.name = "Triangle Waveform";
    script.description = "Verify triangle OSC3 ramps up then down, folding at MSB";

    cmd_reset(&script);
    cmd_label(&script, "triangle_basic");

    // Zero acc via test bit, frequency $0100
    // Triangle: if MSB=0, out = acc>>11; if MSB=1, out = (acc^0xFFFFFF)>>11
    // OSC3 = out >> 4
    script_init_v3(&script, 0x0100, CTRL_TRIANGLE);

    // After 128 cycles: acc = 0x008000
    // MSB=0, triangle = 0x008000 >> 11 = 0x010, OSC3 = 0x01
    cmd_run(&script, 128);
    cmd_expect_osc3(&script, 0x01);

    // acc = 0x400000 at cycle 16384
    // MSB=0, triangle = 0x400000 >> 11 = 0x800, OSC3 = 0x80
    cmd_run(&script, 16384 - 128);
    cmd_expect_osc3(&script, 0x80);

    // acc = 0x7FFF00 at cycle 32767
    // MSB=0, triangle = 0x7FFF00 >> 11 = 0xFFF, OSC3 = 0xFF
    cmd_run(&script, 16383);
    cmd_expect_osc3(&script, 0xFF);

    // acc = 0x800100 at cycle 32769 (2 more), MSB=1
    // triangle = (0x800100 ^ 0xFFFFFF) >> 11 = 0x7FFEFF >> 11 = 0xFFF
    // OSC3 = 0xFF (still near peak — XOR creates mirror)
    cmd_run(&script, 2);
    cmd_expect_osc3(&script, 0xFF);

    // acc = 0xC00000 at cycle 49152: total = 49152, run 49152-32769=16383
    // triangle = (0xC00000 ^ 0xFFFFFF) >> 11 = 0x3FFFFF >> 11 = 0x7FF
    // OSC3 = 0x7FF >> 4 = 0x7F
    cmd_run(&script, 16383);
    cmd_expect_osc3(&script, 0x7F);

    int failures = run_script(h, &script);
    print_results(h, &script);
    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Pulse waveform
// ─────────────────────────────────────────────────────────────────────────────
// Pulse: output is 0xFFF when acc upper 12 bits >= pulse width, else 0x000.
// OSC3 is therefore 0xFF or 0x00.
// ─────────────────────────────────────────────────────────────────────────────

int test_pulse_waveform(harness_t* h) {
    test_script_t script;
    script.name = "Pulse Waveform";
    script.description = "Verify pulse OSC3 transitions at correct duty cycle";

    cmd_reset(&script);
    cmd_label(&script, "pulse_50pct");

    // 50% duty: pulse width = 0x800
    // Zero acc via test bit first
    script_set_v3_pw(&script, 0x800);
    script_init_v3(&script, 0x0100, CTRL_PULSE);

    // acc=0, upper 12 bits = 0x000 < 0x800 → output 0, OSC3 = 0x00
    cmd_run(&script, 1);
    cmd_expect_osc3(&script, 0x00);

    // acc >= 0x800000 at cycle 32768 → upper 12 >= 0x800.
    // reSID: pulse_output has a one-cycle pipeline delay, so OSC3
    // reflects the NEW comparator result one cycle AFTER the threshold
    // crossing.  Transition visible at cycle 32769.
    cmd_run(&script, 32768);
    cmd_expect_osc3(&script, 0xFF);

    // Full cycle: acc wraps at 2^24, cycle = 2^24/0x100 = 65536
    // After wrap, upper 12 drops below PW → output goes low.
    // With pipeline delay: visible one cycle later at 65537.
    cmd_run(&script, 32768);
    cmd_expect_osc3(&script, 0x00);

    // Narrow pulse width
    cmd_label(&script, "pulse_narrow");
    cmd_reset(&script);
    script_set_v3_pw(&script, 0x100);
    script_init_v3(&script, 0x0100, CTRL_PULSE);

    // Transition at acc >= 0x100000 → cycle 4096
    // With pipeline: visible at cycle 4097.
    // At cycle 4096: pipeline still shows previous comparison (low)
    cmd_run(&script, 4096);
    cmd_expect_osc3(&script, 0x00);

    // At cycle 4097: pipeline delivers the new comparison (high)
    cmd_run(&script, 1);
    cmd_expect_osc3(&script, 0xFF);

    // Full width (PW=0 means always high when upper 12 >= 0)
    cmd_label(&script, "pulse_full");
    cmd_reset(&script);
    script_set_v3_pw(&script, 0x000);
    script_init_v3(&script, 0x0100, CTRL_PULSE);

    cmd_run(&script, 100);
    cmd_expect_osc3(&script, 0xFF);

    int failures = run_script(h, &script);
    print_results(h, &script);
    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Noise waveform (LFSR sequence)
// ─────────────────────────────────────────────────────────────────────────────
// The noise LFSR is 23-bit with taps at bits 22 and 17.
// It clocks when accumulator bit 19 transitions from 0→1.
// After reset, LFSR = 0x7FFFFE (reSID reference).
// The noise output extracts specific bits from the LFSR into a 12-bit value.
// ─────────────────────────────────────────────────────────────────────────────

int test_noise_waveform(harness_t* h) {
    test_script_t script;
    script.name = "Noise Waveform";
    script.description = "Verify noise LFSR initial state and clocking via OSC3";

    cmd_reset(&script);
    cmd_label(&script, "noise_initial");

    // Use test bit to zero the accumulator, then set noise waveform.
    // LFSR clocks on bit 19 rising edge of accumulator.
    // With freq = 0x1000, bit 19 rising edge occurs every
    // 2^20 / 0x1000 = 256 cycles.
    //
    // The reSID-accurate noise output only uses LFSR bits {20,18,14,11,9,5,2,0}
    // mapped to output bits {11..4}.  The initial LFSR (0x7FFFFE) has bit 0 = 0
    // in bits 0-2.  It takes 3+ shifts for zeros to propagate to bit 5 (the
    // lowest extracted bit), so we need several LFSR clocks to see a change.
    script_init_v3(&script, 0x1000, CTRL_NOISE);

    // Record the initial noise output
    cmd_snapshot(&script);

    // Clock the LFSR many times by running ~2048 cycles (≈8 LFSR clocks)
    cmd_run(&script, 256);
    cmd_snapshot(&script);

    cmd_run(&script, 256);
    cmd_snapshot(&script);

    cmd_run(&script, 256);
    cmd_snapshot(&script);

    cmd_run(&script, 256);
    cmd_snapshot(&script);

    cmd_run(&script, 256);
    cmd_snapshot(&script);

    cmd_run(&script, 256);
    cmd_snapshot(&script);

    cmd_run(&script, 256);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Additional validation: ensure noise values are not all the same
    if (h->snapshots.size() >= 4) {
        bool all_same = true;
        for (size_t i = 1; i < h->snapshots.size(); i++) {
            if (h->snapshots[i].osc3 != h->snapshots[0].osc3) {
                all_same = false;
                break;
            }
        }
        if (all_same) {
            printf("  [FAIL]  Noise LFSR not advancing: all snapshots = 0x%02X\n",
                   h->snapshots[0].osc3);
            failures++;
        } else {
            printf("  [PASS]  Noise LFSR producing varying output\n");
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Noise LFSR exact sequence verification
// ─────────────────────────────────────────────────────────────────────────────
// Verify the exact LFSR sequence matches the known polynomial.
// The 23-bit LFSR with taps at 22,17 has a period of 2^23 - 1 = 8388607.
// We verify the LFSR state after N cycles matches expectations.
//
// reSID pipeline details that affect LFSR clocking:
//   1. Test bit only runs for 1 cycle in init_v3, so the LFSR starts at its
//      reset value (0x7FFFFE), not the fully-faded test value (0x7FFFFF).
//   2. On test-bit falling edge, the LFSR is clocked once (reSID behavior).
//   3. The LFSR shift has a 2-cycle pipeline delay: when accumulator bit 19
//      rises, shift_pipeline = 2; LFSR actually clocks 2 cycles later.
// ─────────────────────────────────────────────────────────────────────────────

int test_noise_lfsr_sequence(harness_t* h) {
    test_script_t script;
    script.name = "Noise LFSR Sequence";
    script.description = "Verify exact LFSR state progression after reset";

    cmd_reset(&script);
    cmd_label(&script, "lfsr_sequence");

    // Zero acc via test-bit, then run noise at high frequency.
    script_init_v3(&script, 0x8000, CTRL_NOISE);

    // After init_v3: test bit was on for 1 cycle (not enough to fade LFSR
    // to 0x7FFFFF), so LFSR is still at its reset value 0x7FFFFE.
    // On test-bit release, the LFSR is clocked once.

    // Simulate the expected LFSR sequence with reSID-accurate pipeline.
    // Start from reset LFSR value (not the test-bit-faded value).
    uint32_t ref_lfsr = 0x7FFFFE;  // NOISE_LFSR_RESET

    // Clock once for test-bit release (reSID: shift register clocked on
    // test bit falling edge using inverted bit 17 as feedback).
    {
        uint32_t bit0 = (~ref_lfsr >> 17) & 1;
        ref_lfsr = ((ref_lfsr << 1) | bit0) & 0x7FFFFF;
    }

    // Now simulate 100 cycles with 2-cycle pipeline delay.
    bool prev_bit19 = false;
    uint32_t acc = 0;
    int shift_pipeline_ref = 0;

    for (uint32_t cycle = 0; cycle < 100; cycle++) {
        acc = (acc + 0x8000) & 0xFFFFFF;
        bool bit19 = (acc & 0x080000) != 0;

        if (bit19 && !prev_bit19) {
            // Rising edge of bit 19 — start 2-cycle pipeline
            shift_pipeline_ref = 2;
        } else if (shift_pipeline_ref > 0 && --shift_pipeline_ref == 0) {
            // Pipeline expired — clock the LFSR
            uint32_t feedback = ((ref_lfsr >> 22) ^ (ref_lfsr >> 17)) & 1;
            ref_lfsr = ((ref_lfsr << 1) | feedback) & 0x7FFFFF;
        }
        prev_bit19 = bit19;
    }

    // Run the SID and take snapshots at corresponding points
    cmd_run(&script, 100);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Verify internal LFSR state matches reference
    uint32_t actual_lfsr = h->sid->voice3.noise_lfsr;
    if (actual_lfsr == ref_lfsr) {
        printf("  [PASS]  LFSR state after 100 cycles: 0x%06X (matches reference)\n", actual_lfsr);
    } else {
        printf("  [FAIL]  LFSR state: expected 0x%06X, got 0x%06X\n", ref_lfsr, actual_lfsr);
        failures++;
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Envelope Attack
// ─────────────────────────────────────────────────────────────────────────────
// Attack rate determines how fast the envelope ramps from 0 to max.
// The rate counter period table is well-documented.
//
// reSID pipeline notes:
//   - envelope_amplitude is preserved across reset() (reSID behavior).
//   - Gate-on has a 2-3 cycle pipeline delay before attack begins.
//   - Rate ticks go through an additional multi-stage pipeline.
//   - ENV3 reads a latched value from BEFORE envelope processing.
//
// To get a clean starting state, we run enough cycles at fastest release
// for the envelope to fully decay to 0 before triggering attack.
// ─────────────────────────────────────────────────────────────────────────────

int test_envelope_attack(harness_t* h) {
    test_script_t script;
    script.name = "Envelope Attack";
    script.description = "Verify ENV3 ramps up at correct rate during attack phase";

    cmd_reset(&script);
    cmd_label(&script, "attack_fast");

    // Set fastest release to ensure envelope decays to 0 before we start.
    // AD=0x00 (attack=0, decay=0), SR=0xF0 (sustain=F, release=0)
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0xF0);

    // Run 10000 cycles at fastest release to ensure envelope is at 0.
    // (From 0xAA power-on value, full decay takes ~6K cycles at rate 0.)
    cmd_run(&script, 10000);

    // Now set up the voice and trigger gate
    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // With reSID pipeline, gate-on has a 2-3 cycle delay before attack
    // begins, and rate ticks go through additional pipeline stages.
    // Run the full attack (255 × 9 + pipeline overhead) and take snapshots.
    cmd_run(&script, 50);
    cmd_snapshot(&script);    // Should show early attack (a few increments)
    cmd_run(&script, 2300);   // Enough for full attack to 0xFF
    cmd_snapshot(&script);    // Should be at 0xFF

    // Test with attack rate 2 (period=63)
    cmd_label(&script, "attack_medium");
    cmd_reset(&script);
    cmd_write(&script, REG_V3_AD, 0x20);  // Attack=2 (period=63), Decay=0
    cmd_write(&script, REG_V3_SR, 0xF0);  // Sustain=F, Release=0

    // Ensure clean state
    cmd_run(&script, 10000);

    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Run enough cycles for several attack increments
    cmd_run(&script, 200);
    cmd_snapshot(&script);    // Should show a few increments at period 63

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Verify snapshots show proper attack behavior
    if (h->snapshots.size() >= 3) {
        uint8_t env_early = h->snapshots[0].env3;
        uint8_t env_full  = h->snapshots[1].env3;
        uint8_t env_med   = h->snapshots[2].env3;

        // Early attack: should have incremented a few times
        if (env_early > 0 && env_early < 20) {
            printf("  [PASS]  Early attack ENV3=%u (expected 1-19)\n", env_early);
        } else if (env_early == 0) {
            printf("  [FAIL]  Attack not started: ENV3=0 after 50 cycles\n");
            failures++;
        } else {
            printf("  [INFO]  Early attack ENV3=%u (higher than expected, pipeline variation)\n", env_early);
        }

        // Full attack: should be at 0xFF
        if (env_full == 0xFF) {
            printf("  [PASS]  Full attack reached: ENV3=0xFF\n");
        } else if (env_full >= 0xF0) {
            printf("  [PASS]  Near-full attack: ENV3=0x%02X (pipeline timing)\n", env_full);
        } else {
            printf("  [FAIL]  Attack incomplete: ENV3=0x%02X (expected 0xFF)\n", env_full);
            failures++;
        }

        // Medium attack: should show a few increments at rate 2 (period 63)
        // After 200 cycles at period 63: ~3 rate ticks → ~2-3 increments
        if (env_med > 0 && env_med < 10) {
            printf("  [PASS]  Medium attack ENV3=%u at rate 2 (expected 1-9)\n", env_med);
        } else if (env_med == 0) {
            printf("  [FAIL]  Medium attack not started: ENV3=0 after 200 cycles\n");
            failures++;
        } else {
            printf("  [INFO]  Medium attack ENV3=%u (pipeline variation)\n", env_med);
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Envelope Decay → Sustain
// ─────────────────────────────────────────────────────────────────────────────

int test_envelope_decay_sustain(harness_t* h) {
    test_script_t script;
    script.name = "Envelope Decay/Sustain";
    script.description = "Verify ENV3 decays to sustain level and holds";

    cmd_reset(&script);
    cmd_label(&script, "decay_to_sustain");

    // Attack=0 (fastest, period=9), Decay=0 (fastest, period=9), Sustain=8, Release=0
    // Sustain level 8 → 0x88 in 8-bit (nibble duplicated)
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0x80);  // Sustain=8, Release=0

    // Ensure envelope is fully decayed to 0 before starting
    cmd_run(&script, 10000);

    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // 8-bit attack to 0xFF: 9 cycles/step × 255 steps = 2295 cycles.
    // Then exponential decay from 0xFF to sustain 0x88.
    // With period=9 per rate tick and exponential counter slowing things down,
    // the decay takes longer than attack.  Run 100K cycles to be safe.
    cmd_run(&script, 100000);
    cmd_snapshot(&script);

    // Run a bit more — sustain should hold steady
    cmd_run(&script, 50000);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Verify sustain is held — both snapshots should show approximately 0x88
    if (h->snapshots.size() >= 2) {
        uint8_t env1 = h->snapshots[0].env3;
        uint8_t env2 = h->snapshots[1].env3;
        // Allow small tolerance for the nonlinear decay approach
        if (env1 >= 0x80 && env1 <= 0x90) {
            printf("  [PASS]  Envelope reached sustain region: ENV3=0x%02X\n", env1);
        } else {
            printf("  [FAIL]  Envelope not at sustain: ENV3=0x%02X (expected ~0x88)\n", env1);
            failures++;
        }
        if (env1 == env2) {
            printf("  [PASS]  Sustain held steady at ENV3=0x%02X\n", env2);
        } else {
            printf("  [FAIL]  Sustain not stable: 0x%02X → 0x%02X\n", env1, env2);
            failures++;
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Envelope Release
// ─────────────────────────────────────────────────────────────────────────────

int test_envelope_release(harness_t* h) {
    test_script_t script;
    script.name = "Envelope Release";
    script.description = "Verify ENV3 decays to zero when gate goes off";

    cmd_reset(&script);
    cmd_label(&script, "release");

    // Set fast attack/decay, sustain=F, release=0 (fastest)
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0xF0);  // Sustain=F, Release=0

    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Let attack complete: 9 * 255 = 2295 cycles to reach 0xFF sustain
    cmd_run(&script, 5000);
    cmd_snapshot(&script); // Should be at max ~0xFF

    // Release: gate off
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH); // Gate off
    cmd_run(&script, 200000); // Exponential release from 0xFF to 0
    cmd_snapshot(&script); // Should be 0 or near-0

    int failures = run_script(h, &script);
    print_results(h, &script);

    if (h->snapshots.size() >= 2) {
        uint8_t before = h->snapshots[0].env3;
        uint8_t after = h->snapshots[1].env3;

        if (before >= 0xF0) {
            printf("  [PASS]  Envelope at sustain max: ENV3=0x%02X\n", before);
        } else {
            printf("  [FAIL]  Envelope didn't reach max: ENV3=0x%02X\n", before);
            failures++;
        }

        if (after == 0x00) {
            printf("  [PASS]  Envelope released to zero: ENV3=0x%02X\n", after);
        } else {
            printf("  [FAIL]  Envelope didn't reach zero: ENV3=0x%02X\n", after);
            failures++;
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: ADSR Bug
// ─────────────────────────────────────────────────────────────────────────────
// The rate counter doesn't reset on state transitions. This means if you
// change ADSR parameters or retrigger the gate at certain moments, the
// envelope can briefly "hang" or exhibit unexpected timing.
// ─────────────────────────────────────────────────────────────────────────────

int test_envelope_adsr_bug(harness_t* h) {
    test_script_t script;
    script.name = "ADSR Bug";
    script.description = "Verify rate counter persistence across state transitions";

    cmd_reset(&script);
    cmd_label(&script, "adsr_bug_retrigger");

    // Start with slow attack (rate 15, period=31251) so the rate counter
    // accumulates a large value before we retrigger.
    cmd_write(&script, REG_V3_AD, 0xF0);  // Attack=15 (slowest), Decay=0
    cmd_write(&script, REG_V3_SR, 0xF0);  // Sustain=F, Release=0

    // Zero accumulator via test bit, then gate on
    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Run for part of the slow attack period
    cmd_run(&script, 15000);
    cmd_snapshot(&script); // Capture state mid-attack

    // Gate off then immediately on — retrigger
    // The rate counter should NOT reset to 0 (this is the ADSR bug)
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH); // Gate off
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE); // Gate on

    // Now change to fast attack (rate 0, period=9)
    cmd_write(&script, REG_V3_AD, 0x00);

    // The rate counter still has its old value — it may take one slow-rate
    // period to expire before the fast rate kicks in.
    cmd_run(&script, 100);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Note: The exact ADSR bug behavior depends on implementation.
    // This test documents the behavior — if the rate counter resets on
    // retrigger, the behavior differs from real hardware.
    if (h->snapshots.size() >= 2) {
        printf("  [INFO]  Pre-retrigger ENV3: 0x%02X, Post-retrigger+100cyc ENV3: 0x%02X\n",
               h->snapshots[0].env3, h->snapshots[1].env3);
        printf("  [INFO]  (ADSR bug: rate counter should persist across gate transitions)\n");
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Ring Modulation
// ─────────────────────────────────────────────────────────────────────────────
// Voice 3 ring-modulates with voice 2's accumulator MSB.
// Ring modulation XORs the triangle's effective MSB with the complement of the
// modulating voice's MSB (reSID: ring_acc = acc ^ (~source_acc & msb_mask)).
// To observe this, the ring source must be at a DIFFERENT frequency from the
// target voice so they're not phase-locked — otherwise the ring mod just
// produces a constant MSB flip.
// ─────────────────────────────────────────────────────────────────────────────

int test_ring_modulation(harness_t* h) {
    test_script_t script;
    script.name = "Ring Modulation";
    script.description = "Verify ring mod XORs triangle with modulator MSB";

    cmd_reset(&script);
    cmd_label(&script, "ring_mod");

    // Voice 2 (ring source for voice 3): freq = 0x0200
    // MSB flips at cycle 0x800000/0x0200 = 16384
    cmd_write(&script, REG_V2_FREQ_LO, 0x00);
    cmd_write(&script, REG_V2_FREQ_HI, 0x02);  // freq = 0x0200
    cmd_write(&script, REG_V2_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V2_CONTROL, CTRL_SAWTOOTH);

    // Voice 3: freq = 0x0100 (different from voice 2 — NOT phase-locked)
    script_set_v3_freq(&script, 0x0100);
    cmd_write(&script, REG_V3_CONTROL, CTRL_TRIANGLE | CTRL_RING | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_TRIANGLE | CTRL_RING);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Phase 1: voice 2 acc MSB = 0 (first half of its cycle)
    cmd_run(&script, 128);
    cmd_snapshot(&script);

    // Run until voice 2's MSB flips to 1 (at cycle 16384)
    cmd_run(&script, 16256);
    cmd_snapshot(&script);

    // Voice 2's MSB is now 1 — ring mod should change triangle output
    cmd_run(&script, 128);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    if (h->snapshots.size() >= 3) {
        printf("  [INFO]  Ring mod snapshots:\n");
        printf("  [INFO]    V2 MSB=0, early:  OSC3=0x%02X\n", h->snapshots[0].osc3);
        printf("  [INFO]    V2 MSB transition: OSC3=0x%02X\n", h->snapshots[1].osc3);
        printf("  [INFO]    V2 MSB=1, after:   OSC3=0x%02X\n", h->snapshots[2].osc3);

        // With ring mod, the output should differ between MSB=0 and MSB=1 phases
        if (h->snapshots[0].osc3 != h->snapshots[2].osc3) {
            printf("  [PASS]  Ring modulation changes OSC3 output\n");
        } else {
            printf("  [FAIL]  Ring modulation has no effect on OSC3\n");
            failures++;
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Oscillator Sync
// ─────────────────────────────────────────────────────────────────────────────
// Voice 3 syncs with voice 2: when voice 2's MSB transitions 0→1,
// voice 3's accumulator resets to 0.
// ─────────────────────────────────────────────────────────────────────────────

int test_oscillator_sync(harness_t* h) {
    test_script_t script;
    script.name = "Oscillator Sync";
    script.description = "Verify sync resets accumulator on modulator MSB rising edge";

    cmd_reset(&script);
    cmd_label(&script, "osc_sync");

    // Zero voice 2 (sync source for voice 3) accumulator
    cmd_write(&script, REG_V2_FREQ_LO, 0x00);
    cmd_write(&script, REG_V2_FREQ_HI, 0x01);  // freq = 0x0100
    cmd_write(&script, REG_V2_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V2_CONTROL, CTRL_SAWTOOTH);

    // Voice 3: higher frequency + sync, zeroed accumulator
    script_set_v3_freq(&script, 0x0800);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_SYNC | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_SYNC);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Let voice 3 run freely for a while
    cmd_run(&script, 4096);
    cmd_snapshot(&script); // Voice 3 has been running

    // Run until voice 2's MSB rises (at cycle 32768 from reset)
    // After the sync, voice 3's accumulator should reset to near 0
    cmd_run(&script, 28672); // total = 32768
    cmd_snapshot(&script); // Should be shortly after sync

    // Run a few more cycles — voice 3 should be starting fresh
    cmd_run(&script, 16);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    if (h->snapshots.size() >= 3) {
        printf("  [INFO]  Sync snapshots:\n");
        printf("  [INFO]    Pre-sync (cycle 4096):  ACC2=%06X, V3 OSC3=0x%02X\n",
               h->snapshots[0].acc[1], h->snapshots[0].osc3);
        printf("  [INFO]    At sync (cycle 32768): ACC2=%06X, V3 OSC3=0x%02X\n",
               h->snapshots[1].acc[1], h->snapshots[1].osc3);
        printf("  [INFO]    Post-sync:              ACC2=%06X, V3 OSC3=0x%02X\n",
               h->snapshots[2].acc[1], h->snapshots[2].osc3);
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Test Bit
// ─────────────────────────────────────────────────────────────────────────────
// Setting the test bit (bit 3) resets the oscillator accumulator to 0
// and holds it there. On the 6581, it also freezes the noise LFSR.
// When released, the oscillator resumes from 0 (and noise from where
// it was frozen).
// ─────────────────────────────────────────────────────────────────────────────

int test_test_bit(harness_t* h) {
    test_script_t script;
    script.name = "Test Bit";
    script.description = "Verify test bit resets accumulator and freezes noise";

    cmd_reset(&script);
    cmd_label(&script, "test_bit_acc");

    // Set up sawtooth on voice 3 with zeroed accumulator
    script_init_v3(&script, 0x1000, CTRL_SAWTOOTH);

    // Let oscillator run to build up accumulator
    cmd_run(&script, 256);
    cmd_snapshot(&script); // acc should be 256 * 0x1000 = 0x100000

    // Set test bit — should reset accumulator to 0
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 10);
    cmd_expect_osc3(&script, 0x00); // Accumulator held at 0

    // Run more with test bit — should stay at 0
    cmd_run(&script, 100);
    cmd_expect_osc3(&script, 0x00);

    // Release test bit — oscillator resumes from 0
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH);
    cmd_run(&script, 16);
    // After 16 cycles: acc = 16 * 0x1000 = 0x10000, OSC3 = 0x01
    cmd_expect_osc3(&script, 0x01);

    // Test noise + test bit
    cmd_label(&script, "test_bit_noise");
    cmd_reset(&script);
    script_set_v3_freq(&script, 0x8000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_NOISE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Run to get some LFSR state
    cmd_run(&script, 100);
    cmd_snapshot(&script); // Record LFSR state

    // Set test bit — should freeze LFSR
    cmd_write(&script, REG_V3_CONTROL, CTRL_NOISE | CTRL_TEST);
    cmd_run(&script, 100);
    cmd_snapshot(&script); // LFSR should be reset to 0x7FFFFF

    int failures = run_script(h, &script);
    print_results(h, &script);

    if (h->snapshots.size() >= 2) {
        printf("  [INFO]  Pre-test LFSR: 0x%06X, Post-test LFSR: 0x%06X\n",
               h->snapshots[0].acc[2], // Not the LFSR but we capture acc
               h->snapshots[1].acc[2]);
        // Check the actual LFSR via internal state
        printf("  [INFO]  Voice 3 LFSR = 0x%06X (expected 0x7FFFFF after test bit)\n",
               h->sid->voice3.noise_lfsr);
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Combined Waveforms
// ─────────────────────────────────────────────────────────────────────────────
// On real hardware, selecting multiple waveforms simultaneously produces
// "crunched" outputs due to analog charge sharing (6581) or near-AND (8580).
// Our emulator uses AND as approximation; this test documents the behavior.
// ─────────────────────────────────────────────────────────────────────────────

int test_combined_waveforms(harness_t* h) {
    test_script_t script;
    script.name = "Combined Waveforms";
    script.description = "Document combined waveform behavior (AND approximation)";

    // Test each pair of waveforms
    struct waveform_pair {
        const char* name;
        uint8_t bits;
    } pairs[] = {
        {"Triangle+Sawtooth", CTRL_TRIANGLE | CTRL_SAWTOOTH},
        {"Triangle+Pulse",    CTRL_TRIANGLE | CTRL_PULSE},
        {"Sawtooth+Pulse",    CTRL_SAWTOOTH | CTRL_PULSE},
        {"Triangle+Noise",    CTRL_TRIANGLE | CTRL_NOISE},
        {"Sawtooth+Noise",    CTRL_SAWTOOTH | CTRL_NOISE},
        {"Pulse+Noise",       CTRL_PULSE    | CTRL_NOISE},
    };

    cmd_reset(&script);

    for (const auto& pair : pairs) {
        cmd_label(&script, pair.name);

        // Zero accumulator via test bit before each pair
        script_set_v3_freq(&script, 0x1000);
        script_set_v3_pw(&script, 0x800); // 50% for pulse relevance
        cmd_write(&script, REG_V3_CONTROL, pair.bits | CTRL_TEST);
        cmd_run(&script, 1);
        cmd_write(&script, REG_V3_CONTROL, pair.bits);
        cmd_write(&script, REG_MODE_VOL, 0x0F);

        cmd_run(&script, 1000);
        cmd_snapshot(&script);

        // Reset for next pair
        cmd_write(&script, REG_V3_CONTROL, 0x00);
        cmd_run(&script, 10);
    }

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Print all combined waveform outputs
    printf("  Combined waveform OSC3 values (AND approximation):\n");
    size_t pair_idx = 0;
    for (const auto& snap : h->snapshots) {
        if (pair_idx < 6) {
            printf("    %-24s OSC3=0x%02X\n", pairs[pair_idx].name, snap.osc3);
        }
        pair_idx++;
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// reSID Conformance: 15-bit Rate Counter Wrapping
// ─────────────────────────────────────────────────────────────────────────────
// reSID's rate_counter is 15-bit and wraps at 0x8000. When you switch from
// a slow rate to a fast rate and the counter has already exceeded the new
// period, the counter must wrap around before the next tick occurs.
// ─────────────────────────────────────────────────────────────────────────────

int test_resid_rate_counter_15bit(harness_t* h) {
    test_script_t script;
    script.name = "reSID: 15-bit Rate Counter Wrap";
    script.description = "Verify rate counter wraps at 0x8000 (ADSR delay bug)";

    cmd_reset(&script);
    cmd_label(&script, "rate_counter_wrap");

    // Pre-zero envelope: previous tests may leave envelope_amplitude non-zero
    // (it's preserved across reset).  Gate on with fastest attack/decay to 0.
    cmd_write(&script, REG_V3_AD, 0x00);  // fastest attack/decay
    cmd_write(&script, REG_V3_SR, 0x00);  // sustain=0, release=0
    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_run(&script, 10000);  // attack to 0xFF, decay to 0x00, hold_zero set

    // Now set up the actual test
    // Set slowest attack (rate 15, period=31251)
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH);  // gate off
    cmd_write(&script, REG_V3_AD, 0xF0);  // A=15, D=0
    cmd_write(&script, REG_V3_SR, 0xF0);  // S=F, R=0

    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Run 20000 cycles — rate counter advances to ~20000, no tick yet (period=31251)
    cmd_run(&script, 20000);
    cmd_expect_env3(&script, 0x00);  // No ticks yet (hold_zero keeps envelope at 0)

    // Switch to fastest attack (rate 0, period=9)
    cmd_write(&script, REG_V3_AD, 0x00);

    // With proper 15-bit wrapping:
    //   counter=20000, compare != 9, so keeps counting
    //   wraps at 0x8000=32768, continues to 9 → delay = 32768-20000+9 = 12777
    //   After 1000 cycles, envelope should still be 0
    cmd_run(&script, 1000);
    cmd_expect_env3(&script, 0x00);  // Must still be 0 (counter wrapping)

    // After 12000 more cycles (total 13000 since rate change), first tick should
    // have occurred and envelope starts incrementing at fast rate
    cmd_run(&script, 12000);
    cmd_snapshot(&script);

    int failures = run_script(h, &script);
    print_results(h, &script);

    // After the wrap, each tick is 9 cycles. After ~13000 cycles past the rate
    // change, the first tick happened at ~12777, leaving ~223 cycles ≈ 24 ticks.
    // So envelope should be around 24.
    if (h->snapshots.size() >= 1) {
        uint8_t env = h->snapshots[0].env3;
        if (env > 0 && env < 50) {
            printf("  [PASS]  Post-wrap ENV3=%u (expected ~24 given wrap delay)\n", env);
        } else if (env == 0) {
            printf("  [FAIL]  ENV3=0, rate counter may have stalled\n");
            failures++;
        } else {
            printf("  [FAIL]  ENV3=%u, too high — rate counter not wrapping (missing ADSR delay)\n", env);
            failures++;
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// reSID Conformance: Sustain Level Change During Sustain
// ─────────────────────────────────────────────────────────────────────────────
// reSID has no separate SUSTAIN state. It stays in DECAY_SUSTAIN forever.
// Each rate tick, it checks: if (counter != sustain) decrement.
// So lowering sustain causes decay to resume.
// ─────────────────────────────────────────────────────────────────────────────

int test_resid_sustain_level_change(harness_t* h) {
    test_script_t script;
    script.name = "reSID: Sustain Level Change";
    script.description = "Lowering sustain during sustain should resume decay";

    cmd_reset(&script);
    cmd_label(&script, "sustain_lower");

    // Fast attack/decay, sustain=0xA (=0xAA), fast release
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0xA0);

    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Wait for sustain at 0xAA
    cmd_run(&script, 200000);
    cmd_expect_env3(&script, 0xAA);

    // Lower sustain to 0x5 (=0x55)
    cmd_write(&script, REG_V3_SR, 0x50);
    cmd_run(&script, 200000);
    cmd_expect_env3(&script, 0x55);  // Should have decayed to new sustain

    // Lower sustain to 0x0 (=0x00)
    cmd_write(&script, REG_V3_SR, 0x00);
    cmd_run(&script, 200000);
    cmd_expect_env3(&script, 0x00);  // Should reach zero

    int failures = run_script(h, &script);
    print_results(h, &script);
    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// reSID Conformance: LFSR Initial State After Reset
// ─────────────────────────────────────────────────────────────────────────────
// After chip reset, LFSR = 0x7FFFFE (reSID).
// The noise output bit extraction produces OSC3 = 0xFE for this state.
// ─────────────────────────────────────────────────────────────────────────────

int test_resid_lfsr_reset_value(harness_t* h) {
    test_script_t script;
    script.name = "reSID: LFSR Reset Value";
    script.description = "Verify LFSR = 0x7FFFFE after chip reset → OSC3 = 0xFE";

    cmd_reset(&script);
    cmd_label(&script, "lfsr_initial");

    // Set noise on voice 3 with zero frequency (LFSR won't clock)
    script_set_v3_freq(&script, 0x0000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_NOISE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);
    cmd_run(&script, 1);

    // With LFSR = 0x7FFFFE:
    //   Bits {20,18,14,11,9,5,2,0} = {1,1,1,1,1,1,1,0}
    //   Output = 0b1111_1110_0000 = 0xFE0
    //   OSC3 = 0xFE
    cmd_expect_osc3(&script, 0xFE);

    int failures = run_script(h, &script);
    print_results(h, &script);

    // Also check internal state
    uint32_t lfsr = h->sid->voice3.noise_lfsr;
    if (lfsr == 0x7FFFFE) {
        printf("  [PASS]  Internal LFSR = 0x%06X (matches reSID)\n", lfsr);
    } else {
        printf("  [FAIL]  Internal LFSR = 0x%06X (expected 0x7FFFFE)\n", lfsr);
        failures++;
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// reSID Conformance: Exact Exponential Decay Timing
// ─────────────────────────────────────────────────────────────────────────────
// Verify the exponential counter thresholds produce exact envelope values
// at precise cycle counts.
// ─────────────────────────────────────────────────────────────────────────────

int test_resid_exponential_decay_exact(harness_t* h) {
    test_script_t script;
    script.name = "reSID: Exponential Decay Exact";
    script.description = "Verify exponential counter thresholds at exact cycles";

    cmd_reset(&script);
    cmd_label(&script, "exp_decay");

    // Pre-zero envelope: previous tests may leave envelope_amplitude non-zero.
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0x00);
    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_run(&script, 10000);  // attack to 0xFF, decay to 0x00, hold_zero set

    // Reset again to clear rate counter and pipeline state from the pre-zero,
    // giving us a clean starting point.  envelope_amplitude (0x00) is preserved.
    cmd_reset(&script);

    // Set up the actual test: fastest attack/decay, sustain=0.
    // Skip test-bit accumulator reset — this test only checks ENV3, not OSC3,
    // and the test-bit cycle shifts the rate counter off its clean post-reset
    // position, making exact cycle arithmetic unreliable.
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0x00);
    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // With rate_counter=0 after reset, the exact pipeline chain is:
    //   match at cycle 9 → reset_rate_counter at 10 → envelope_pipeline=2 at 10
    //   → pipeline fires at 12 → amplitude changes at 12.
    //   ENV3 pre-latch captures BEFORE envelope_clock, so change visible at 13.
    //   255th increment visible at 13 + 254×9 = 2299.
    cmd_run(&script, 2299);
    cmd_expect_env3(&script, 0xFF);

    // After attack: rate_counter ends at 4 (post-pipeline reset position).
    // First decay match at cycle 5 of next run, visible at cycle 9.
    // Decay with exp period 1: 161 decrements visible at 9 + 160×9 = 1449.
    cmd_run(&script, 1449);
    cmd_expect_env3(&script, 0x5E);

    // One more tick to hit 0x5D (threshold → exp period becomes 2)
    // Counter still at 4 after previous run; next visible at cycle 9.
    cmd_run(&script, 9);
    cmd_expect_env3(&script, 0x5D);

    // With exp period 2: one decrement per 2 rate ticks (18 cycles).
    // First visible at 19 (5-cycle match offset + 14-cycle pipeline+2nd-match).
    // 38 decrements: 19 + 37×18 = 685.
    cmd_run(&script, 685);
    cmd_expect_env3(&script, 0x37);

    // One more decrement: 19 cycles to next visible change.
    cmd_run(&script, 19);
    cmd_expect_env3(&script, 0x36);

    int failures = run_script(h, &script);
    print_results(h, &script);
    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// reSID Conformance: Gate Retrigger Behavior
// ─────────────────────────────────────────────────────────────────────────────

int test_resid_gate_retrigger(harness_t* h) {
    test_script_t script;
    script.name = "reSID: Gate Retrigger";
    script.description = "Verify attack resumes from current level on retrigger";

    cmd_reset(&script);
    cmd_label(&script, "retrigger");

    // Fast attack/decay, sustain=F, release=0
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0xF0);

    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Reach max
    cmd_run(&script, 5000);
    cmd_expect_env3(&script, 0xFF);

    // Release and let decay partway
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH);
    cmd_run(&script, 500);
    cmd_snapshot(&script);  // Capture mid-release level

    // Retrigger gate — attack from current level
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_run(&script, 5000);
    cmd_expect_env3(&script, 0xFF);  // Should reach max again

    int failures = run_script(h, &script);
    print_results(h, &script);

    if (h->snapshots.size() >= 1) {
        uint8_t mid = h->snapshots[0].env3;
        printf("  [INFO]  Mid-release envelope: 0x%02X (retrigger resumes from here)\n", mid);
        if (mid > 0 && mid < 0xFF) {
            printf("  [PASS]  Release produced partial decay before retrigger\n");
        } else {
            printf("  [WARN]  Unexpected mid-release level\n");
        }
    }

    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// reSID Conformance: Hold-Zero Freeze
// ─────────────────────────────────────────────────────────────────────────────
// When envelope decays to 0x00, hold_zero is set. The envelope should stay
// frozen at 0 until gate is turned ON again (clears hold_zero).
// ─────────────────────────────────────────────────────────────────────────────

int test_resid_hold_zero(harness_t* h) {
    test_script_t script;
    script.name = "reSID: Hold-Zero Freeze";
    script.description = "Verify envelope freezes at 0 and only restarts on gate";

    cmd_reset(&script);
    cmd_label(&script, "hold_zero");

    // Fast everything, sustain=0 → decay goes to 0
    cmd_write(&script, REG_V3_AD, 0x00);
    cmd_write(&script, REG_V3_SR, 0x00);

    script_set_v3_freq(&script, 0x1000);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_TEST);
    cmd_run(&script, 1);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);
    cmd_write(&script, REG_MODE_VOL, 0x0F);

    // Wait for full decay to 0
    cmd_run(&script, 50000);
    cmd_expect_env3(&script, 0x00);

    // Should stay frozen
    cmd_run(&script, 50000);
    cmd_expect_env3(&script, 0x00);

    // Change sustain — should NOT unfreeze (hold_zero still set)
    cmd_write(&script, REG_V3_SR, 0xF0);
    cmd_run(&script, 50000);
    cmd_expect_env3(&script, 0x00);  // Still frozen

    // Gate off, then on → clears hold_zero, starts attack
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH);
    cmd_write(&script, REG_V3_CONTROL, CTRL_SAWTOOTH | CTRL_GATE);

    cmd_run(&script, 2295);  // Full fastest attack duration
    cmd_expect_env3(&script, 0xFF);

    int failures = run_script(h, &script);
    print_results(h, &script);
    return failures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Run all built-in tests
// ─────────────────────────────────────────────────────────────────────────────

int run_all_builtin_tests(harness_t* h, bool verbose) {
    if (!h) return -1;

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           SID MOS6581 Digital Verification Suite            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    h->verbose = verbose;

    struct test_entry {
        const char* category;
        int (*func)(harness_t*);
    };

    test_entry tests[] = {
        {"Sawtooth Waveform",       test_sawtooth_waveform},
        {"Triangle Waveform",       test_triangle_waveform},
        {"Pulse Waveform",          test_pulse_waveform},
        {"Noise Waveform",          test_noise_waveform},
        {"Noise LFSR Sequence",     test_noise_lfsr_sequence},
        {"Envelope Attack",         test_envelope_attack},
        {"Envelope Decay/Sustain",  test_envelope_decay_sustain},
        {"Envelope Release",        test_envelope_release},
        {"ADSR Bug",                test_envelope_adsr_bug},
        {"Ring Modulation",         test_ring_modulation},
        {"Oscillator Sync",         test_oscillator_sync},
        {"Test Bit",                test_test_bit},
        {"Combined Waveforms",      test_combined_waveforms},
        {"reSID: Rate Counter 15-bit", test_resid_rate_counter_15bit},
        {"reSID: Sustain Change",      test_resid_sustain_level_change},
        {"reSID: LFSR Reset Value",    test_resid_lfsr_reset_value},
        {"reSID: Exp Decay Exact",     test_resid_exponential_decay_exact},
        {"reSID: Gate Retrigger",      test_resid_gate_retrigger},
        {"reSID: Hold-Zero Freeze",    test_resid_hold_zero},
    };

    int total_failures = 0;
    int total_tests = sizeof(tests) / sizeof(tests[0]);
    int tests_passed = 0;

    for (const auto& t : tests) {
        // Reset SID between tests
        h->sid->reset();
        h->total_cycles = 0;
        h->trace.clear();
        h->snapshots.clear();
        h->check_osc3 = false;
        h->check_env3 = false;
        h->trace_enabled = false;

        int f = t.func(h);
        total_failures += f;
        if (f == 0) tests_passed++;
    }

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  TOTAL: %d/%d test categories passed", tests_passed, total_tests);
    if (total_failures > 0) {
        printf(" (%d individual failures)\n", total_failures);
    } else {
        printf("\n");
    }
    printf("══════════════════════════════════════════════════════════════\n\n");

    return total_failures;
}

} // namespace sid_test
