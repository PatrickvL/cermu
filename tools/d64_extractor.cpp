/**
 * Simple D64 disk image extractor for Commodore 64 test files
 * Extracts PRG files from D64 images for CPU testing
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

// D64 constants
const int SECTORS_PER_TRACK[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  // Tracks 1-17
    19, 19, 19, 19, 19, 19, 19,                                          // Tracks 18-24
    18, 18, 18, 18, 18, 18,                                              // Tracks 25-30
    17, 17, 17, 17, 17                                                   // Tracks 31-35
};

const int BYTES_PER_SECTOR = 256;
const int DIRECTORY_TRACK = 18;
const int DIRECTORY_SECTOR = 1;

struct DirectoryEntry {
    uint8_t file_type;
    uint8_t track;
    uint8_t sector;
    char filename[16];
    uint16_t blocks;
    bool valid;
};

class D64Extractor {
private:
    std::vector<uint8_t> d64_data;
    
    int trackSectorToOffset(int track, int sector) {
        int offset = 0;
        for (int t = 1; t < track; t++) {
            offset += SECTORS_PER_TRACK[t-1] * BYTES_PER_SECTOR;
        }
        offset += sector * BYTES_PER_SECTOR;
        return offset;
    }
    
    std::vector<DirectoryEntry> readDirectory() {
        std::vector<DirectoryEntry> entries;
        int dir_offset = trackSectorToOffset(DIRECTORY_TRACK, DIRECTORY_SECTOR);
        
        for (int entry_idx = 0; entry_idx < 8; entry_idx++) {
            int entry_offset = dir_offset + (entry_idx * 32);
            DirectoryEntry entry = {};
            
            if (entry_offset + 32 > d64_data.size()) break;
            
            entry.file_type = d64_data[entry_offset + 2];
            entry.track = d64_data[entry_offset + 3];
            entry.sector = d64_data[entry_offset + 4];
            
            // Copy filename (PETSCII to ASCII approximation)
            for (int i = 0; i < 16; i++) {
                uint8_t c = d64_data[entry_offset + 5 + i];
                if (c == 0xA0) break; // PETSCII shifted space (end of name)
                entry.filename[i] = (c >= 0x41 && c <= 0x5A) ? c : 
                                   (c >= 0xC1 && c <= 0xDA) ? (c - 0x80) : c;
            }
            
            entry.blocks = d64_data[entry_offset + 30] | (d64_data[entry_offset + 31] << 8);
            entry.valid = (entry.file_type & 0x07) == 2; // PRG file
            
            if (entry.valid && entry.track > 0) {
                entries.push_back(entry);
            }
        }
        
        return entries;
    }
    
    std::vector<uint8_t> extractFile(const DirectoryEntry& entry) {
        std::vector<uint8_t> file_data;
        int track = entry.track;
        int sector = entry.sector;
        
        while (track > 0) {
            int offset = trackSectorToOffset(track, sector);
            if (offset >= d64_data.size()) break;
            
            // Read sector data
            int data_start = (track == entry.track && sector == entry.sector) ? 2 : 0;
            for (int i = data_start; i < 256; i++) {
                if (offset + i >= d64_data.size()) break;
                file_data.push_back(d64_data[offset + i]);
            }
            
            // Get next track/sector
            track = d64_data[offset];
            sector = d64_data[offset + 1];
            
            if (track == 0) break; // End of file
        }
        
        return file_data;
    }
    
public:
    bool loadD64(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: Cannot open D64 file: " << filename << std::endl;
            return false;
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        d64_data.resize(size);
        file.read(reinterpret_cast<char*>(d64_data.data()), size);
        
        std::cout << "Loaded D64 image: " << filename << " (" << size << " bytes)" << std::endl;
        return true;
    }
    
    void extractAll(const std::string& output_dir) {
        auto entries = readDirectory();
        
        std::cout << "Found " << entries.size() << " PRG files:" << std::endl;
        
        for (const auto& entry : entries) {
            std::cout << "  " << entry.filename << " (Track " << (int)entry.track 
                      << ", Sector " << (int)entry.sector << ", " << entry.blocks << " blocks)" << std::endl;
            
            auto file_data = extractFile(entry);
            if (file_data.size() >= 2) {
                // Remove C64 load address (first 2 bytes)
                std::vector<uint8_t> prg_data(file_data.begin() + 2, file_data.end());
                
                std::string output_path = output_dir + "/" + entry.filename + ".prg";
                std::ofstream out(output_path, std::ios::binary);
                if (out) {
                    out.write(reinterpret_cast<const char*>(prg_data.data()), prg_data.size());
                    std::cout << "    -> Extracted to: " << output_path << " (" << prg_data.size() << " bytes)" << std::endl;
                } else {
                    std::cerr << "    -> Error writing: " << output_path << std::endl;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <d64_file> <output_directory>" << std::endl;
        std::cout << "Extracts PRG files from Commodore 64 D64 disk images" << std::endl;
        return 1;
    }
    
    D64Extractor extractor;
    if (!extractor.loadD64(argv[1])) {
        return 1;
    }
    
    extractor.extractAll(argv[2]);
    return 0;
}