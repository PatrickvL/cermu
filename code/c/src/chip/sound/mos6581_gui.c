#include "mos6581.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

static const char* waveform_names[] = {
    "None",
    "Triangle", 
    "Sawtooth",
    "Sawtooth+Triangle",
    "Pulse",
    "Pulse+Triangle", 
    "Pulse+Sawtooth",
    "Pulse+Sawtooth+Triangle",
    "Noise"
};

static const char* envelope_cycle_names[] = {
    "Attack",
    "Decay", 
    "Sustain",
    "Release",
    "Off"
};

static void render_voice_debug(voice_t* voice, int voice_num) {
    igPushID_Int(voice_num);
    
    char voice_header[32];
    snprintf(voice_header, sizeof(voice_header), "Voice %d", voice_num);
    
    if (igCollapsingHeader_BoolPtr(voice_header, NULL, 0)) {
        igIndent(16.0f);
        
        // Write-only voice register values :
        igText("Frequency: $%04X (%d)", voice->frequency, voice->frequency);
        igText("Pulse Waveform Width: $%04X (%d)", voice->pulse_waveform_width, voice->pulse_waveform_width);
        
        // Values updated by WriteVoiceControlRegisterValue()
        igText("Gated: %s", voice->gated ? "Yes" : "No");
        igText("Synchronize: %s", voice->synchronize ? "Yes" : "No");
        igText("Ring Modulation: %s", voice->ring_modulation ? "Yes" : "No");
        igText("Test: %s", voice->test ? "Yes" : "No");
        const char* waveform_name = ((int)voice->waveform < 9) ? waveform_names[(int)voice->waveform] : "Unknown";
        igText("Waveform: %s (%d)", waveform_name, (int)voice->waveform);
        
        igSeparator();
        
        // Outside readable variables (albeit after shifting)
        igText("Envelope Amplitude: $%04X (%d)", voice->envelope_amplitude, voice->envelope_amplitude);
        igText("Oscillator Waveform: $%04X (%d)", voice->oscillator_waveform, voice->oscillator_waveform);
        igText("Result: %d", voice->result);
        
        igSeparator();
        
        // Internal state
        igText("Waveform Accumulator: $%06X (%d)", voice->waveform_accumulator, voice->waveform_accumulator);
        const char* cycle_name = ((int)voice->envelope_cycle < 5) ? envelope_cycle_names[(int)voice->envelope_cycle] : "Unknown";
        igText("Envelope Cycle: %s (%d)", cycle_name, (int)voice->envelope_cycle);
        igText("Envelope Next Level: %d", voice->envelope_next_level);
        igText("Sustain Level: %d", voice->sustain_level);
        
        // Envelope Deltas lookup table
        igText("Envelope Deltas:");
        igIndent(16.0f);
        igText("  Attack: %d", voice->envelope_deltas[CYCLE_ATTACK]);
        igText("  Decay: %d", voice->envelope_deltas[CYCLE_DECAY]);
        igText("  Sustain: %d", voice->envelope_deltas[CYCLE_SUSTAIN]);
        igText("  Release: %d", voice->envelope_deltas[CYCLE_RELEASE]);
        igText("  Off: %d", voice->envelope_deltas[CYCLE_OFF]);
        igUnindent(16.0f);
        
        igText("CPU Clock: %.0f Hz", voice->cpu_clock);

        igUnindent(16.0f);
    }
    
    igPopID();
}

// ============================================================================
// MOS6581 SID GUI DEBUG WINDOW
// ============================================================================
void mos6581_render_debug_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", sid->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("MOS 6581 SID (Sound Interface Device)");
    igText("SID MOS 6581 DIP has 28 pins");
    igSeparator();
    
    // Voices section
    if (igCollapsingHeader_BoolPtr("Voices", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        render_voice_debug(&sid->voice1, 1);
        render_voice_debug(&sid->voice2, 2);
        render_voice_debug(&sid->voice3, 3);
    }
    
    igSeparator();
    
    // Filter and Global Settings
    if (igCollapsingHeader_BoolPtr("Filter & Global Settings", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Write-only register values :
        igText("Filter Cutoff Frequency: $%03X (%d)", sid->filter_cutoff_frequency, sid->filter_cutoff_frequency);
        igText("Filter Voice 1: %s", sid->filter_voice1 ? "Yes" : "No");
        igText("Filter Voice 2: %s", sid->filter_voice2 ? "Yes" : "No");
        igText("Filter Voice 3: %s", sid->filter_voice3 ? "Yes" : "No");
        igText("Filter Voice 4 (External): %s", sid->filter_voice4 ? "Yes" : "No");
        igText("Filter Resonance: %d (0-15)", sid->filter_resonance);
        igText("Volume: %d (0-15)", sid->volume);
        igText("Low Pass Enabled: %s", sid->low_pass_enabled ? "Yes" : "No");
        igText("Band Pass Enabled: %s", sid->band_pass_enabled ? "Yes" : "No");
        igText("High Pass Enabled: %s", sid->high_pass_enabled ? "Yes" : "No");
        igText("Voice 3 Disabled: %s", sid->voice3_disabled ? "Yes" : "No");
        
        igSeparator();
        
        // Internal state
        igText("Filter Voice Count: %d", sid->filter_voice_count);
        igText("Sample Index: %d", sid->sample_index);
        igText("Sample Buffer Size: %d", SAMPLE_BUFFER_SIZE);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Register Map
    if (igCollapsingHeader_BoolPtr("Register Map", NULL, 0)) {
        igIndent(16.0f);
        
        igText("Voice 1 Registers:");
        igIndent(16.0f);
        igText("$D400: FRELO1 - Frequency Control (low byte)");
        igText("$D401: FREHI1 - Frequency Control (high byte)"); 
        igText("$D402: PWLO1 - Pulse Waveform Width (low byte)");
        igText("$D403: PWHI1 - Pulse Waveform Width (high nybble)");
        igText("$D404: VCREG1 - Voice Control Register");
        igText("$D405: ATDCY1 - Attack/Decay Register");
        igText("$D406: SUREL1 - Sustain/Release Control Register");
        igUnindent(16.0f);
        
        igText("Voice 2 Registers:");
        igIndent(16.0f);
        igText("$D407: FRELO2 - Frequency Control (low byte)");
        igText("$D408: FREHI2 - Frequency Control (high byte)");
        igText("$D409: PWLO2 - Pulse Waveform Width (low byte)");
        igText("$D40A: PWHI2 - Pulse Waveform Width (high nybble)");
        igText("$D40B: VCREG2 - Voice Control Register");
        igText("$D40C: ATDCY2 - Attack/Decay Register");
        igText("$D40D: SUREL2 - Sustain/Release Control Register");
        igUnindent(16.0f);
        
        igText("Voice 3 Registers:");
        igIndent(16.0f);
        igText("$D40E: FRELO3 - Frequency Control (low byte)");
        igText("$D40F: FREHI3 - Frequency Control (high byte)");
        igText("$D410: PWLO3 - Pulse Waveform Width (low byte)");
        igText("$D411: PWHI3 - Pulse Waveform Width (high nybble)");
        igText("$D412: VCREG3 - Voice Control Register");
        igText("$D413: ATDCY3 - Attack/Decay Register");
        igText("$D414: SUREL3 - Sustain/Release Control Register");
        igUnindent(16.0f);
        
        igText("Filter & Global Registers:");
        igIndent(16.0f);
        igText("$D415: CUTLO - Filter Cutoff Frequency (low 3 bits)");
        igText("$D416: CUTHI - Filter Cutoff Frequency (high byte)");
        igText("$D417: RESON - Filter Resonance Control Register");
        igText("$D418: SIGVOL - Volume and Filter Select Register");
        igUnindent(16.0f);
        
        igText("Read-only Registers:");
        igIndent(16.0f);
        igText("$D419: POTX - Read Game Paddle 1 (or 3) Position");
        igText("$D41A: POTY - Read Game Paddle 2 (or 4) Position");
        igText("$D41B: OSC3 - Read Oscillator 3/Random Number Generator");
        igText("$D41C: ENV3 - Envelope Generator 3 Output");
        igUnindent(16.0f);
        
        igText("Unmapped Registers:");
        igIndent(16.0f);
        igText("$D41D-$D41F: Unmapped (always return $FF)");
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }

    igEnd();
}

// ============================================================================
// MOS6581 SID GUI SETTINGS WINDOW
// ============================================================================
void mos6581_render_settings_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", sid->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("SID Configuration");
    igSeparator();
    
    igText("Chip Type: MOS6581 SID");
    igText("Base Address: $D400-$D7FF");
    igText("Register Size: 32 bytes (repeated each 32 bytes)");
    igText("Address Mask: $1F (31)");
    
    igSeparator();
    
    // Pin Configuration
    if (igCollapsingHeader_BoolPtr("Pin Configuration", NULL, 0)) {
        igIndent(16.0f);
        igText("SID MOS 6581 DIP has 28 pins:");
        igText("CAP1A/CAP1B (1,2) - Capacitor connections");
        igText("CAP2A/CAP2B (3,4) - Capacitor connections");
        igText("/RES (5) - Reset Input");
        igText("phi2 (6) - Clock Input");
        igText("R/W (7) - Read/Write Input");
        igText("/CS (8) - Chip Select");
        igText("A0-A4 (9-13) - Address Inputs (5 bits)");
        igText("GND (14) - Ground");
        igText("D0-D7 (15-22) - Data Bus (8 bits)");
        igText("POT_Y/POT_X (23,24) - Paddle inputs");
        igText("Vcc/EXT_IN (25,26) - Power/External input");
        igText("AUDIO_OUT/Vdd (27,28) - Audio output/Power");
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Sound Settings
    if (igCollapsingHeader_BoolPtr("Sound Settings", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        static bool sound_enabled = true;
        igCheckbox("Sound Enabled", &sound_enabled);
        
        static float master_volume = 1.0f;
        igSliderFloat("Master Volume", &master_volume, 0.0f, 1.0f, "%.2f", 0);
        
        static float cpu_clock = 1000000.0f;
        igInputFloat("CPU Clock (Hz)", &cpu_clock, 1000.0f, 10000.0f, "%.0f", 0);
        
        igSeparator();
        
        igText("Filter Settings");
        static bool filter_enabled = true;
        igCheckbox("Filter Enabled", &filter_enabled);
        
        static bool accurate_sid = false;
        igCheckbox("Accurate SID Emulation", &accurate_sid);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Voice Configuration
    if (igCollapsingHeader_BoolPtr("Voice Configuration", NULL, 0)) {
        igIndent(16.0f);
          for (int i = 1; i <= 3; i++) {
            igPushID_Int(i);
            
            char voice_header[32];
            snprintf(voice_header, sizeof(voice_header), "Voice %d Settings", i); // Made unique
            
            if (igCollapsingHeader_BoolPtr(voice_header, NULL, 0)) {
                igIndent(16.0f);
                
                static bool voice_enabled = true;
                igCheckbox("Enabled", &voice_enabled);
                
                static float voice_volume = 1.0f;
                igSliderFloat("Volume", &voice_volume, 0.0f, 1.0f, "%.2f", 0);
                  igUnindent(16.0f);
            }
            
            igPopID();
        }
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    if (igButton("Reset SID", (ImVec2){0, 0})) {
        mos6581_reset(sid);
    }
    
    igSameLine(0, -1);
    
    if (igButton("Test Sound", (ImVec2){0, 0})) {
        // TODO: Generate test sound
    }

    igEnd();
}