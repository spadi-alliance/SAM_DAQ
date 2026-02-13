#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <vector>
#include <fstream>
#include <memory>
#include "DelegateManager.h"
#include "../../helpers/Constants.h"
#include "../../helpers/Logger.h"

// Decoding status enum
enum class DecodingStatus {
    SUCCESS,
    HEADER_1_NOT_FOUND,
    HEADER_2_NOT_FOUND, 
    HEADER_3_NOT_FOUND,
    HEADER_4_NOT_FOUND,
    FOOTER_NOT_FOUND,
    INCOMPLETE_BLOCK,
    CORRUPTED_DATA
};

// Decoded block information
struct DecodedBlock {
    u_int64_t block_id;
    u_int8_t chip_number;
    u_int8_t sample_number;
    u_int64_t timestamp;
    std::vector<uint16_t> channels;  // 32 channels, each with a 10-bit value (stored as uint16_t)
    DecodingStatus status;
    std::string error_message;
};

// Delegate protocol
class SAMDecoderDelegate {
public:
    virtual ~SAMDecoderDelegate() = default;
    virtual void onBlockDecoded(const DecodedBlock& block) {};
    virtual void acquisitionWillStop() {};
};

class SAMDecoder {
public:
    SAMDecoder(const std::string& baseName = K_DEFAULT_OUT_FNAME, bool multithreaded = true);
    ~SAMDecoder();

    // Handler for TCP data, to be used in Board.cpp
    size_t decodeFromTcp(char* buffer, size_t fill, uint64_t handled, uint32_t* cur_mark);

    // Handler for file data - reuses the same decoding logic
    void decodeFromFile(const std::string& filename);

    // Call to stop decoding (thread-safe)
    void stop();

    // Get statistics about data integrity
    size_t getMalformedBlocksCount() const { return malformed_blocks.load(); }
    void resetMalformedBlocksCount() { malformed_blocks = 0; }
    size_t getTotalBlocksCount() const { return blocks; }

    // Call to close ROOT file and tree gracefully
    void close();

    // Delegate management
    void addDelegate(SAMDecoderDelegate* delegate);
    void removeDelegate(SAMDecoderDelegate* delegate);
    void removeDelegateOfType(const std::type_info& type);
    void clearDelegates();

private:
    void logWarning(const std::string& msg);
    
    // New method for decoding individual blocks
    bool decodeBlock(const char* buffer, size_t buffer_size, DecodedBlock& block, size_t& bytes_consumed);

private:
    std::vector<uint16_t> ch;

    u_int64_t blocks = 0;
    u_int8_t current_sample = 0;
    u_int8_t current_chip = 0;
    u_int64_t current_timestamp = 0;

    std::atomic<bool> running;

    // Persistent buffer for incomplete TCP data
    std::vector<char> tcp_partial_buffer;

    std::ofstream tcp_bin_dump;
    std::string bin_file_name;

    // Logging system
    Logger logger;

    // Data integrity tracking
    std::atomic<size_t> malformed_blocks = 0;

    // Gestione delegates con thread dedicati
    std::unique_ptr<DelegateManager> delegate_manager;
};