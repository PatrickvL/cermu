/**
 * connector_icons.cpp — Embedded 16×16 pixel-art icons for connector types
 *
 * Each icon is a hand-crafted 16×16 monochrome bitmap stored as a uint16_t
 * array (one word per row, MSB = leftmost pixel).  At init() these are
 * expanded into RGBA GL textures with a configurable foreground colour.
 *
 * The bitmaps are deliberately simple silhouettes that read well at small
 * sizes in the ImGui menu bar.
 */

#include "core/cermu.hpp"
#include "gui/port_icons.hpp"
#include <cstring>
#include <cstdio>

namespace PortIcons {

// ============================================================================
// BITMAP DATA — 16 rows of 16 bits each (MSB = leftmost pixel)
// ============================================================================

// Joystick (DB-9 control port) — stick-and-base silhouette
static const uint16_t ICON_JOYSTICK[16] = {
    0b0000001000000000,  //        #
    0b0000001000000000,  //        #
    0b0000011100000000,  //       ###
    0b0000011100000000,  //       ###
    0b0000111110000000,  //      #####
    0b0000111110000000,  //      #####
    0b0000011100000000,  //       ###
    0b0000011100000000,  //       ###
    0b0000011100000000,  //       ###
    0b0000011100000000,  //       ###
    0b0001111111000000,  //    #######
    0b0011111111100000,  //   #########
    0b0111111111110000,  //  ###########
    0b0111111111110000,  //  ###########
    0b0011111111100000,  //   #########
    0b0000000000000000,  //
};

// Floppy disk (IEC serial / 1541 drive)
static const uint16_t ICON_FLOPPY[16] = {
    0b0111111111111100,  //  ##########
    0b0111111111111100,  //  ##########
    0b0111100001111100,  //  ####  ####
    0b0111100001111100,  //  ####  ####
    0b0111111111111100,  //  ##########
    0b0111111111111100,  //  ##########
    0b0111111111111100,  //  ##########
    0b0111111111111100,  //  ##########
    0b0111100000011100,  //  ####   ##
    0b0111100000011100,  //  ####   ##
    0b0111100000011100,  //  ####   ##
    0b0111100000011100,  //  ####   ##
    0b0111100000011100,  //  ####   ##
    0b0111100000011100,  //  ####   ##
    0b0111111111111100,  //  ##########
    0b0000000000000000,  //
};

// Cassette tape — two reels in a housing
static const uint16_t ICON_CASSETTE[16] = {
    0b0000000000000000,  //
    0b0011111111111000,  //   ##########
    0b0011111111111000,  //   ##########
    0b0011000000011000,  //   ##      ##
    0b0011011001101000,  //   ## ##  ## 
    0b0011111111111000,  //   ##########
    0b0011111001111000,  //   #### ####
    0b0011110000111000,  //   ####  ###
    0b0011110000111000,  //   ####  ###
    0b0011111001111000,  //   #### ####
    0b0011111111111000,  //   ##########
    0b0011011001101000,  //   ## ##  ## 
    0b0011000000011000,  //   ##      ##
    0b0011111111111000,  //   ##########
    0b0011111111111000,  //   ##########
    0b0000000000000000,  //
};

// User port — DB-25 style plug with pins
static const uint16_t ICON_USERPORT[16] = {
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0111111111111100,  //  ##########
    0b0100000000000100,  //  #        #
    0b0101010101010100,  //  # # # # ##
    0b0100000000000100,  //  #        #
    0b0101010101010100,  //  # # # # ##
    0b0100000000000100,  //  #        #
    0b0111111111111100,  //  ##########
    0b0011111111111000,  //   ########
    0b0001111111110000,  //    ########
    0b0000111111100000,  //     ######
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0000000000000000,  //
};

// Cartridge (expansion port) — PCB with contacts
static const uint16_t ICON_CARTRIDGE[16] = {
    0b0001111111110000,  //    ########
    0b0001111111110000,  //    ########
    0b0001100001110000,  //    ##   ###
    0b0001111111110000,  //    ########
    0b0001100001110000,  //    ##   ###
    0b0001111111110000,  //    ########
    0b0001111111110000,  //    ########
    0b0001111111110000,  //    ########
    0b0001111111110000,  //    ########
    0b0001111111110000,  //    ########
    0b0011111111111000,  //   ##########
    0b0011111111111000,  //   ##########
    0b0010100101011000,  //   # #  # # #
    0b0010100101011000,  //   # #  # # #
    0b0010100101011000,  //   # #  # # #
    0b0000000000000000,  //
};

// NES controller pad
static const uint16_t ICON_NES_PAD[16] = {
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0111111111111100,  //  ###########
    0b1111111111111110,  // #############
    0b1100100000010110,  // ##  #    # ##
    0b1101110001010110,  // ## ###  # # ##
    0b1100100001110110,  // ##  #   ### ##
    0b1111111111111110,  // #############
    0b0111111111111100,  //  ###########
    0b0001111111100000,  //    ########
    0b0000011110000000,  //      ####
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0000000000000000,  //
    0b0000000000000000,  //
};

// Generic/fallback — gear/cog symbol
static const uint16_t ICON_GENERIC[16] = {
    0b0000011100000000,  //      ###
    0b0000011100000000,  //      ###
    0b0011111111000000,  //   ########
    0b0011111111000000,  //   ########
    0b1111100111110000,  // #####  #####
    0b1111000011110000,  // ####    ####
    0b0110000001100000,  //  ##      ##
    0b0110000001100000,  //  ##      ##
    0b1111000011110000,  // ####    ####
    0b1111100111110000,  // #####  #####
    0b0011111111000000,  //   ########
    0b0011111111000000,  //   ########
    0b0000011100000000,  //      ###
    0b0000011100000000,  //      ###
    0b0000000000000000,  //
    0b0000000000000000,  //
};

// CRT monitor — screen with stand
static const uint16_t ICON_MONITOR[16] = {
    0b0000000000000000,  //
    0b0111111111111100,  //  ###########
    0b0100000000000100,  //  #         #
    0b0100000000000100,  //  #         #
    0b0100000000000100,  //  #         #
    0b0100000000000100,  //  #         #
    0b0100000000000100,  //  #         #
    0b0100000000000100,  //  #         #
    0b0100000000000100,  //  #         #
    0b0111111111111100,  //  ###########
    0b0001111111110000,  //    ########
    0b0000011111000000,  //      #####
    0b0000001110000000,  //       ###
    0b0000111111100000,  //     #######
    0b0000000000000000,  //
    0b0000000000000000,  //
};

// Speaker — speaker cone silhouette
static const uint16_t ICON_SPEAKER[16] = {
    0b0000000000000000,  //
    0b0000001100000000,  //       ##
    0b0000011100000000,  //      ###
    0b0000111100100000,  //     ####  #
    0b0011111101000000,  //   ###### #
    0b0011111110100000,  //   ####### #
    0b0011111101000000,  //   ###### #
    0b0011111110100000,  //   ####### #
    0b0011111101000000,  //   ###### #
    0b0011111110100000,  //   ####### #
    0b0011111101000000,  //   ###### #
    0b0000111100100000,  //     ####  #
    0b0000011100000000,  //      ###
    0b0000001100000000,  //       ##
    0b0000000000000000,  //
    0b0000000000000000,  //
};

// ============================================================================
// TEXTURE STORAGE
// ============================================================================

static GLuint textures_[static_cast<int>(PortType::COUNT)] = {};
static bool   initialised_ = false;

/// Expand a 16×16 monochrome bitmap into an RGBA texture.
static GLuint create_icon_texture(const uint16_t bitmap[16],
                                  uint8_t r, uint8_t g, uint8_t b) {
    uint32_t pixels[16 * 16];
    for (int y = 0; y < 16; y++) {
        uint16_t row = bitmap[y];
        for (int x = 0; x < 16; x++) {
            bool set = (row >> (15 - x)) & 1;
            if (set) {
                pixels[y * 16 + x] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
            } else {
                pixels[y * 16 + x] = 0x00000000;  // transparent
            }
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return tex;
}

// ============================================================================
// PUBLIC API
// ============================================================================

void init() {
    if (initialised_) return;
    memset(textures_, 0, sizeof(textures_));

    // Distinct colours per connector family for quick visual identification
    textures_[static_cast<int>(PortType::CONTROL_PORT_DB9)]  = create_icon_texture(ICON_JOYSTICK,  180, 220, 255);  // light blue
    textures_[static_cast<int>(PortType::IEC_SERIAL)]        = create_icon_texture(ICON_FLOPPY,    255, 220, 130);  // amber
    textures_[static_cast<int>(PortType::CASSETTE_PORT)]     = create_icon_texture(ICON_CASSETTE,  200, 200, 200);  // silver
    textures_[static_cast<int>(PortType::USER_PORT)]         = create_icon_texture(ICON_USERPORT,  180, 255, 180);  // light green
    textures_[static_cast<int>(PortType::EXPANSION_PORT)]    = create_icon_texture(ICON_CARTRIDGE, 255, 180, 180);  // light red
    textures_[static_cast<int>(PortType::CONTROLLER_NES)]    = create_icon_texture(ICON_NES_PAD,   220, 180, 255);  // light purple
    textures_[static_cast<int>(PortType::CONTROLLER_SNES)]   = create_icon_texture(ICON_NES_PAD,   220, 180, 255);  // same shape
    textures_[static_cast<int>(PortType::CONTROLLER_ATARI)]  = create_icon_texture(ICON_JOYSTICK,  255, 200, 150);  // warm
    textures_[static_cast<int>(PortType::CUSTOM)]            = create_icon_texture(ICON_GENERIC,   200, 200, 200);  // gray

    // Video output connectors — monitor icon, colour-coded by signal quality
    textures_[static_cast<int>(PortType::VIDEO_COMPOSITE)]   = create_icon_texture(ICON_MONITOR,   255, 230, 130);  // yellow (basic)
    textures_[static_cast<int>(PortType::VIDEO_SVIDEO)]      = create_icon_texture(ICON_MONITOR,   200, 255, 180);  // green (better)
    textures_[static_cast<int>(PortType::VIDEO_RGB)]         = create_icon_texture(ICON_MONITOR,   180, 220, 255);  // blue (sharp)
    textures_[static_cast<int>(PortType::VIDEO_RGBI)]        = create_icon_texture(ICON_MONITOR,   180, 200, 255);  // blue-grey
    textures_[static_cast<int>(PortType::VIDEO_COMPONENT)]   = create_icon_texture(ICON_MONITOR,   180, 255, 220);  // cyan
    textures_[static_cast<int>(PortType::VIDEO_HDMI)]        = create_icon_texture(ICON_MONITOR,   255, 255, 255);  // white (digital)

    // Audio output connectors — speaker icon
    textures_[static_cast<int>(PortType::AUDIO_MONO)]        = create_icon_texture(ICON_SPEAKER,   255, 200, 150);  // warm
    textures_[static_cast<int>(PortType::AUDIO_STEREO)]      = create_icon_texture(ICON_SPEAKER,   255, 220, 180);  // warm light
    textures_[static_cast<int>(PortType::AUDIO_SPDIF)]       = create_icon_texture(ICON_SPEAKER,   200, 200, 255);  // light blue
    textures_[static_cast<int>(PortType::AUDIO_HDMI)]        = create_icon_texture(ICON_SPEAKER,   255, 255, 255);  // white

    initialised_ = true;
    log_info("PortIcons: Initialised %d icon textures\n",
           static_cast<int>(PortType::COUNT));
}

void cleanup() {
    if (!initialised_) return;
    for (int i = 0; i < static_cast<int>(PortType::COUNT); i++) {
        if (textures_[i]) {
            glDeleteTextures(1, &textures_[i]);
            textures_[i] = 0;
        }
    }
    initialised_ = false;
}

GLuint get_icon(PortType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(PortType::COUNT)) return 0;
    return textures_[idx];
}

} // namespace PortIcons
