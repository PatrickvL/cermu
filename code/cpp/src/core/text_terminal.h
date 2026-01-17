#pragma once

#include <cstdint>
#include <cstring>

/**
 * Generic Text Terminal Renderer
 * 
 * Provides a reusable text-mode display system for vintage computers.
 * Useful for Apple 1, early terminals, and text-based systems.
 * 
 * Features:
 * - Configurable dimensions (rows x columns)
 * - Character cell rendering with built-in font
 * - Color support (foreground/background)
 * - Cursor support
 * - Hardware scrolling
 */
class TextTerminal {
public:
    /**
     * Create a text terminal
     * @param cols Number of columns (typically 40 or 80)
     * @param rows Number of rows (typically 24 or 25)
     * @param char_width Width of each character in pixels (typically 8)
     * @param char_height Height of each character in pixels (typically 8)
     */
    TextTerminal(int cols, int rows, int char_width = 8, int char_height = 8);
    ~TextTerminal();
    
    // Display properties
    int get_width_pixels() const { return cols_ * char_width_; }
    int get_height_pixels() const { return rows_ * char_height_; }
    int get_cols() const { return cols_; }
    int get_rows() const { return rows_; }
    
    // Character operations
    void put_char(int col, int row, char ch);
    void put_char(int col, int row, char ch, uint32_t fg_color, uint32_t bg_color);
    char get_char(int col, int row) const;
    
    // Screen operations
    void clear();
    void clear(uint32_t bg_color);
    void scroll_up(int lines = 1);
    void scroll_down(int lines = 1);
    
    // Cursor
    void set_cursor(int col, int row);
    void get_cursor(int* col, int* row) const;
    void show_cursor(bool show);
    bool is_cursor_visible() const { return cursor_visible_; }
    
    // Colors
    void set_foreground_color(uint32_t color);
    void set_background_color(uint32_t color);
    uint32_t get_foreground_color() const { return fg_color_; }
    uint32_t get_background_color() const { return bg_color_; }
    
    // Rendering
    void render(uint32_t* framebuffer, int fb_width, int fb_height);
    
    // Character ROM (8x8 font data)
    static const uint8_t* get_default_font();
    void set_font(const uint8_t* font_data);  // 8x8 font, 256 characters, 8 bytes each
    
private:
    int cols_;
    int rows_;
    int char_width_;
    int char_height_;
    
    // Text buffer (character data)
    char* text_buffer_;
    
    // Color buffers (optional - if NULL, use default colors)
    uint32_t* fg_color_buffer_;
    uint32_t* bg_color_buffer_;
    
    // Cursor state
    int cursor_col_;
    int cursor_row_;
    bool cursor_visible_;
    
    // Default colors
    uint32_t fg_color_;
    uint32_t bg_color_;
    
    // Font data
    const uint8_t* font_data_;
    
    // Helper methods
    int get_buffer_index(int col, int row) const;
    void render_char(uint32_t* framebuffer, int fb_width, int x, int y, 
                     char ch, uint32_t fg, uint32_t bg);
};