/*
 * nes_example.cpp - NES System Example
 *
 * This example demonstrates how to use the complete NES system with
 * hardware-accurate CPU, APU, and PPU components.
 */

#include "../nes_system.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>

using namespace nes_system;

class SimpleNESEmulator {
private:
    NESSystem* nes;
    bool running = false;
    
    // Simple frame timing
    std::chrono::high_resolution_clock::time_point last_frame_time;
    double frame_time_ms;
    
public:
    SimpleNESEmulator(bool is_pal = false) {
        nes = new NESSystem(is_pal);
        frame_time_ms = is_pal ? (1000.0 / 50.0) : (1000.0 / 60.0);
        last_frame_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "NES Emulator initialized (" << (is_pal ? "PAL" : "NTSC") << ")" << std::endl;
    }
    
    ~SimpleNESEmulator() {
        delete nes;
    }
    
    bool load_rom(const std::string& filename) {
        if (nes->load_cartridge(filename)) {
            std::cout << "ROM loaded successfully: " << filename << std::endl;
            return true;
        } else {
            std::cout << "Failed to load ROM: " << filename << std::endl;
            return false;
        }
    }
    
    void reset() {
        nes->reset();
        std::cout << "System reset" << std::endl;
    }
    
    void run_frame() {
        if (!nes->is_system_ready()) {
            return;
        }
        
        // Run one frame
        nes->run_frame();
        
        // Simple timing control
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>
                      (current_time - last_frame_time).count();
        
        if (elapsed < frame_time_ms) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<long>(frame_time_ms - elapsed))
            );
        }
        
        last_frame_time = std::chrono::high_resolution_clock::now();
    }
    
    void run(int max_frames = -1) {
        if (!nes->is_system_ready()) {
            std::cout << "No cartridge loaded. Cannot run emulator." << std::endl;
            return;
        }
        
        running = true;
        int frame_count = 0;
        
        std::cout << "Running emulator..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (running && (max_frames == -1 || frame_count < max_frames)) {
            run_frame();
            frame_count++;
            
            // Print status every 60 frames
            if (frame_count % 60 == 0) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>
                                      (current_time - start_time).count();
                
                double fps = (elapsed_seconds > 0) ? (frame_count / (double)elapsed_seconds) : 0.0;
                
                std::cout << "Frame: " << frame_count 
                         << ", FPS: " << fps 
                         << ", Total Cycles: " << nes->get_total_cycles() << std::endl;
            }
            
            // Simple input handling (for demonstration)
            handle_input();
        }
        
        std::cout << "Emulator stopped after " << frame_count << " frames." << std::endl;
    }
    
    void stop() {
        running = false;
    }
    
    void save_screen_to_ppm(const std::string& filename) {
        const auto& screen = nes->get_screen();
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cout << "Failed to create PPM file: " << filename << std::endl;
            return;
        }
        
        // PPM header
        file << "P3\n";
        file << nes_constants::SCREEN_WIDTH << " " << nes_constants::SCREEN_HEIGHT << "\n";
        file << "255\n";
        
        // Pixel data
        for (int y = 0; y < nes_constants::SCREEN_HEIGHT; y++) {
            for (int x = 0; x < nes_constants::SCREEN_WIDTH; x++) {
                uint32_t pixel = screen[y * nes_constants::SCREEN_WIDTH + x];
                uint8_t r = (pixel >> 16) & 0xFF;
                uint8_t g = (pixel >> 8) & 0xFF;
                uint8_t b = pixel & 0xFF;
                
                file << (int)r << " " << (int)g << " " << (int)b << " ";
            }
            file << "\n";
        }
        
        std::cout << "Screen saved to: " << filename << std::endl;
    }
    
    void save_audio_to_wav(const std::string& filename) {
        const auto& audio_buffer = nes->get_audio_buffer();
        
        if (audio_buffer.empty()) {
            std::cout << "No audio data to save." << std::endl;
            return;
        }
        
        // Simple WAV file creation (44.1kHz, 16-bit, mono)
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "Failed to create WAV file: " << filename << std::endl;
            return;
        }
        
        uint32_t sample_rate = 44100;
        uint16_t bits_per_sample = 16;
        uint16_t channels = 1;
        uint32_t data_size = static_cast<uint32_t>(audio_buffer.size() * sizeof(int16_t));
        uint32_t file_size = 36 + data_size;
        
        // WAV header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&file_size), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        
        uint32_t fmt_chunk_size = 16;
        uint16_t audio_format = 1; // PCM
        uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
        uint16_t block_align = channels * bits_per_sample / 8;
        
        file.write(reinterpret_cast<const char*>(&fmt_chunk_size), 4);
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        file.write(reinterpret_cast<const char*>(&channels), 2);
        file.write(reinterpret_cast<const char*>(&sample_rate), 4);
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
        
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&data_size), 4);
        
        // Audio data (convert float to 16-bit PCM)
        for (float sample : audio_buffer) {
            int16_t pcm_sample = static_cast<int16_t>(sample * 32767.0f);
            file.write(reinterpret_cast<const char*>(&pcm_sample), sizeof(int16_t));
        }
        
        std::cout << "Audio saved to: " << filename << " (" 
                  << audio_buffer.size() << " samples)" << std::endl;
    }
    
    void print_system_info() {
        std::cout << "\n=== NES System Information ===" << std::endl;
        std::cout << "Cartridge loaded: " << (nes->is_cartridge_loaded() ? "Yes" : "No") << std::endl;
        std::cout << "System ready: " << (nes->is_system_ready() ? "Yes" : "No") << std::endl;
        std::cout << "Total cycles: " << nes->get_total_cycles() << std::endl;
        std::cout << "Audio buffer size: " << nes->get_audio_buffer().size() << " samples" << std::endl;
        std::cout << "================================\n" << std::endl;
    }
    
private:
    void handle_input() {
        // Simple keyboard input simulation
        // In a real emulator, you'd handle actual keyboard/gamepad input
        
        // For demonstration, we'll simulate some button presses occasionally
        static int input_counter = 0;
        input_counter++;
        
        if (input_counter % 120 == 0) { // Every 2 seconds at 60 FPS
            // Simulate pressing A button briefly
            nes->press_button(0, Controller::A);
        } else if (input_counter % 120 == 5) {
            // Release A button
            nes->release_button(0, Controller::A);
        }
        
        if (input_counter % 300 == 0) { // Every 5 seconds
            // Simulate pressing START button briefly
            nes->press_button(0, Controller::START);
        } else if (input_counter % 300 == 5) {
            // Release START button
            nes->release_button(0, Controller::START);
        }
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <rom_file>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -pal          Use PAL timing (default: NTSC)" << std::endl;
    std::cout << "  -frames N     Run for N frames then exit (default: unlimited)" << std::endl;
    std::cout << "  -screen FILE  Save final screen to PPM file" << std::endl;
    std::cout << "  -audio FILE   Save audio to WAV file" << std::endl;
    std::cout << "  -info         Print system information" << std::endl;
    std::cout << "  -test         Run basic functionality test" << std::endl;
    std::cout << "  -help         Show this help message" << std::endl;
}

void run_basic_test() {
    std::cout << "\n=== Running Basic NES System Test ===" << std::endl;
    
    // Test NTSC system creation
    std::cout << "Creating NTSC NES system..." << std::endl;
    SimpleNESEmulator ntsc_nes(false);
    ntsc_nes.print_system_info();
    
    // Test PAL system creation
    std::cout << "Creating PAL NES system..." << std::endl;
    SimpleNESEmulator pal_nes(true);
    pal_nes.print_system_info();
    
    // Test reset functionality
    std::cout << "Testing reset functionality..." << std::endl;
    ntsc_nes.reset();
    
    std::cout << "Basic test completed successfully!" << std::endl;
    std::cout << "======================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "NES System Example" << std::endl;
    std::cout << "Hardware-Accurate NES Emulator with Integrated APU" << std::endl;
    std::cout << "===================================================" << std::endl;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse command line arguments
    bool is_pal = false;
    int max_frames = -1;
    std::string rom_file;
    std::string screen_file;
    std::string audio_file;
    bool show_info = false;
    bool run_test = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-pal") {
            is_pal = true;
        } else if (arg == "-frames" && i + 1 < argc) {
            max_frames = std::atoi(argv[++i]);
        } else if (arg == "-screen" && i + 1 < argc) {
            screen_file = argv[++i];
        } else if (arg == "-audio" && i + 1 < argc) {
            audio_file = argv[++i];
        } else if (arg == "-info") {
            show_info = true;
        } else if (arg == "-test") {
            run_test = true;
        } else if (arg == "-help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg[0] != '-' && rom_file.empty()) {
            rom_file = arg;
        }
    }
    
    // Run basic test if requested
    if (run_test) {
        run_basic_test();
        if (rom_file.empty()) {
            return 0;
        }
    }
    
    if (rom_file.empty()) {
        std::cout << "Error: No ROM file specified." << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    try {
        // Create NES emulator
        SimpleNESEmulator emulator(is_pal);
        
        // Load ROM
        if (!emulator.load_rom(rom_file)) {
            return 1;
        }
        
        // Show system info if requested
        if (show_info) {
            emulator.print_system_info();
        }
        
        // Run emulator
        emulator.run(max_frames);
        
        // Save screen if requested
        if (!screen_file.empty()) {
            emulator.save_screen_to_ppm(screen_file);
        }
        
        // Save audio if requested
        if (!audio_file.empty()) {
            emulator.save_audio_to_wav(audio_file);
        }
        
        std::cout << "Emulation completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}