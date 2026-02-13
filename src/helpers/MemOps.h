#include <immintrin.h>  // For SIMD
#include <cstring>
#include <vector>
#include <cstdint>


static uint16_t readWord(const char* buffer) {
    uint16_t raw_data;
    std::memcpy(&raw_data, buffer, sizeof(raw_data));
    return ((raw_data >> 8) & 0xFF) | ((raw_data << 8) & 0xFF00);
}

static int16_t read10BitData(const std::vector<uint8_t>& data, size_t index) { // SLOW IMPLEMENTATION
    size_t byte_index = (index * 10) / 8;
    uint16_t result = 0;
    switch (index % 4) {
        case 0:
            result = ((data[byte_index]) << 2) | ((data[byte_index + 1] & 0xC0) >> 6);
            break;
        case 1:
            result = ((data[byte_index] & 0x3F) << 4) | ((data[byte_index + 1] & 0xF0) >> 4);
            break;
        case 2:
            result = ((data[byte_index] & 0x0F) << 6) | ((data[byte_index + 1] & 0xFC) >> 2);
            break;
        case 3:
            result = ((data[byte_index] & 0x03) << 8) | (data[byte_index + 1]);
            break;
    }
    return result;
}

// Fast optimized big-endian channel extraction - maintains Python compatibility
// while being much more efficient than the bit-by-bit approach
static void extractAndAssignChannels(const uint8_t* data, 
                                    std::vector<uint16_t>& channels) {
    // Process 4 channels at a time (5 bytes = 40 bits)
    // This maintains the same big-endian bit order as Python while being ~12x faster
    for (int group = 0; group < 32; group += 4) {
        size_t byte_offset = (group * 5) / 4;  // Each group of 4 channels = 5 bytes
        
        // Load 5 bytes as big-endian 40-bit number
        uint64_t packed_data = 0;
        for (int i = 0; i < 5; i++) {
            packed_data = (packed_data << 8) | data[byte_offset + i];
        }
        
        // Extract 4 channels from the 40-bit big-endian sequence
        channels[group + 0] = (packed_data >> 30) & 0x3FF;  // Bits 0-9
        channels[group + 1] = (packed_data >> 20) & 0x3FF;  // Bits 10-19
        channels[group + 2] = (packed_data >> 10) & 0x3FF;  // Bits 20-29
        channels[group + 3] = (packed_data >> 0) & 0x3FF;   // Bits 30-39
    }
}

// Reference implementation using BitReader (for debugging/verification)
// This is the slow but clearly correct implementation that matches Python exactly
#ifdef DEBUG_CHANNEL_EXTRACTION
class CppBitReader {
private:
    const uint8_t* data;
    size_t data_size;
    size_t byte_pos;
    size_t bit_pos;
    
public:
    CppBitReader(const uint8_t* data, size_t size) 
        : data(data), data_size(size), byte_pos(0), bit_pos(0) {}
    
    uint16_t read_bits(int num_bits) {
        if (num_bits == 0) return 0;
        
        uint16_t result = 0;
        int bits_read = 0;
        
        while (bits_read < num_bits) {
            if (byte_pos >= data_size) {
                throw std::runtime_error("Reached end of data");
            }
            
            // Get current byte
            uint8_t current_byte = data[byte_pos];
            
            // Calculate how many bits to read from current byte
            int bits_available = 8 - bit_pos;
            int bits_needed = num_bits - bits_read;
            int bits_to_read = std::min(bits_available, bits_needed);
            
            // Extract bits from current byte (BIG-ENDIAN style - MSB first)
            uint8_t mask = ((1 << bits_to_read) - 1) << (bits_available - bits_to_read);
            uint8_t extracted = (current_byte & mask) >> (bits_available - bits_to_read);
            
            // Add to result
            result = (result << bits_to_read) | extracted;
            
            // Update positions
            bit_pos += bits_to_read;
            bits_read += bits_to_read;
            
            // Move to next byte if current byte is exhausted
            if (bit_pos >= 8) {
                byte_pos++;
                bit_pos = 0;
            }
        }
        
        return result;
    }
};

static void extractAndAssignChannels_reference(const uint8_t* data, 
                                              std::vector<uint16_t>& channels) {
    // Use BitReader that matches Python's logic exactly (for verification)
    CppBitReader reader(data, 40);  // 40 bytes = 320 bits for 32 channels
    
    // Extract 32 channels, 10 bits each, in the same order as Python
    for (int i = 0; i < 32; i++) {
        channels[i] = reader.read_bits(10);
    }
}
#endif