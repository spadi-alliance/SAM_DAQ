#include "SAMDecoder.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <pthread.h>
#include <sched.h>
#include <filesystem>
#include "../../helpers/MemOps.h"

/* GENERAL STRUCTURE OF DATA
HEADER
[ 0:15 ] = 16'hafaf // Header
[16:21 ] = 6'b000000 // Reserved
[22:23 ] = 2'b00 // Bus select
[24:31 ] = 8'b00000000 // Sample number
HEADER_2
[ 0:15 ] = 16'haffa // Header
[16:31 ] = counter[47:32] // 16 bits of counter
HEADER_3
[ 0:15 ] = 16'hfaaf // Header
[16:31 ] = counter[31:16] // 16 bits of counter
HEADER_4
[ 0:15 ] = 16'hfffa // Header
[16:31 ] = counter[15:0] // 16 bits of counter
EVENT DATA
[ 0:319] = 320 bits of data (32 channels * 10 bits each)
FOOTER
[ 0:15 ] = 16'hfafa // Footer
[16:21 ] = 6'b000000 // Reserved
[22:23 ] = 2'b00 // Bus select
[24:31 ] = 8'b00000000 // Sample number
*/

constexpr int HEADER_SIZE = 16; // Now includes HEADER, HEADER_2, HEADER_3, HEADER_4
constexpr int FOOTER_SIZE = 4;
constexpr int DATA_SIZE = 40;
constexpr int NUM_CHANNELS = 32;
constexpr int MAX_NUM_SAMPLES = 128;
constexpr int CHIP_NUMBER = 4;
constexpr int LINE_WIDTH = 16;


SAMDecoder::SAMDecoder(const std::string& baseName, bool multithreaded)
    : ch(NUM_CHANNELS, 0),
      running(true),
      delegate_manager(std::make_unique<DelegateManager>()),
      logger(baseName + ".log", false)  // Initialize logger with log file and console output
{
    // Set high priority but leave room for system processes
    if (multithreaded) {
        struct sched_param main_param;
        main_param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;  // -1 for safety
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &main_param) != 0) {
            std::cerr << "Warning: Could not set real-time priority, do you have permissions for that?" << std::endl;
        }
    } 

    delegate_manager->isMultithreaded = multithreaded;
    bin_file_name = baseName + ".bin";
    
    // Create output directory if it doesn't exist
    std::filesystem::path filePath(bin_file_name);
    std::filesystem::path dirPath = filePath.parent_path();
    if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }
    
    tcp_bin_dump.open(bin_file_name, std::ios::out | std::ios::binary | std::ios::trunc);
    
    // Log initialization
    logger.info("SAMDecoder initialized with base name: " + baseName);
    logger.info("Binary output file: " + bin_file_name);
}

SAMDecoder::~SAMDecoder() {
    close();
    if (tcp_bin_dump.is_open()) {
        tcp_bin_dump.close();
    }
}

void SAMDecoder::stop() {
    running = false;
    std::fill(ch.begin(), ch.end(), 0);
    delegate_manager->willStopAcquisition();
}

void SAMDecoder::close() {
    if (tcp_bin_dump.is_open()) {
        tcp_bin_dump.close();
    }
    delegate_manager->clearDelegates();
}

void SAMDecoder::addDelegate(SAMDecoderDelegate* delegate) {
    delegate_manager->addDelegate(delegate);
}

void SAMDecoder::removeDelegate(SAMDecoderDelegate* delegate) {
    delegate_manager->removeDelegate(delegate);
}

void SAMDecoder::removeDelegateOfType(const std::type_info& type) {
    delegate_manager->removeDelegateOfType(type);
}

void SAMDecoder::logWarning(const std::string& msg) {
    //logger.warning(msg);
}


/* GENERAL STRUCTURE OF DATA
HEADER
[ 0:15 ] = 16'hafaf // Header
[16:23 ] = 8'bxxxxxxxx // Chip selected
[24:31 ] = 8'bxxxxxxxx // Sample number
HEADER_2
[ 0:15 ] = 16'haffa // Header
[16:31 ] = counter[47:32] // 16 bits of counter
HEADER_3
[ 0:15 ] = 16'hfaaf // Header
[16:31 ] = counter[31:16] // 16 bits of counter
HEADER_4
[ 0:15 ] = 16'hfffa // Header
[16:31 ] = counter[15:0] // 16 bits of counter
CHs DATA
[ 0:319] = 320 bits of data (32 channels * 10 bits each)
FOOTER
[ 0:15 ] = 16'hfafa // Footer
[16:23 ] = 8'bxxxxxxxx // Chip selected
[24:31 ] = 8'bxxxxxxxx // Sample number
*/

size_t SAMDecoder::decodeFromTcp(char* buffer, size_t fill, uint64_t /*handled*/, uint32_t* /*cur_mark*/) {
    // Fast binary dump - keep this the same as requested
    if (tcp_bin_dump.is_open()) {
        tcp_bin_dump.write(buffer, fill);
        tcp_bin_dump.flush();
    }
    
    // Combine any leftover data from previous TCP chunks with new data
    std::vector<char> combined_buffer;
    combined_buffer.reserve(tcp_partial_buffer.size() + fill);
    combined_buffer.insert(combined_buffer.end(), tcp_partial_buffer.begin(), tcp_partial_buffer.end());
    combined_buffer.insert(combined_buffer.end(), buffer, buffer + fill);
    
    size_t consumed = 0;
    size_t total_size = combined_buffer.size();
    
    while (consumed < total_size) {  // Process all available data regardless of running state
        size_t remaining = total_size - consumed;
        
        // Minimum block size check
        if (remaining < HEADER_SIZE + DATA_SIZE + FOOTER_SIZE) {
            break; // Not enough data for a complete block
        }
        
        // Try to decode a block starting at current position
        DecodedBlock block;
        size_t block_bytes_consumed = 0;
        bool should_continue = decodeBlock(combined_buffer.data() + consumed, remaining, block, block_bytes_consumed);
        
        // Always notify delegates about the block (even failed ones) - but only if still running or this is final cleanup
        if (running || tcp_partial_buffer.size() > 0) {  // Process if running OR if we have leftover data
            delegate_manager->queueTaskForAllDelegates(
                [block](SAMDecoderDelegate* delegate) {
                    try {
                        delegate->onBlockDecoded(block);
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in onBlockDecoded: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Unknown exception in onBlockDecoded" << std::endl;
                    }
                });
        }
        
        if (block.status == DecodingStatus::SUCCESS) {
            // Successfully decoded a complete block
            blocks++;
        }
        
        consumed += block_bytes_consumed;
        
        if (!should_continue) {
            // Decoder found unrecoverable error, stop processing this buffer
            break;
        }
        
        // If not running and we've processed some data, don't continue indefinitely
        if (!running && consumed > 0) {
            break;
        }
    }
    
    // Store any unprocessed data for the next TCP chunk
    tcp_partial_buffer.clear();
    if (consumed < total_size) {
        tcp_partial_buffer.assign(combined_buffer.begin() + consumed, combined_buffer.end());
    }
    
    // Return the number of bytes consumed from the original buffer
    // If we had leftover data, we need to calculate how much of the original buffer was consumed
    if (tcp_partial_buffer.size() > 0) {
        // We have leftover data, so we consumed less than the full original buffer
        size_t original_consumed = (consumed > tcp_partial_buffer.size()) ? 
            consumed - (total_size - fill) : 0;
        return original_consumed;
    } else {
        // All data was consumed, including any previous partial data
        return fill;
    }
}

void SAMDecoder::decodeFromFile(const std::string& filename) {
    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        logger.error("Cannot open file: " + filename);
        return;
    }
    
    // Get file size for debugging
    infile.seekg(0, std::ios::end);
    size_t fileSize = infile.tellg();
    infile.seekg(0, std::ios::beg);
    
    logger.info("Starting to decode file: " + filename + " (size: " + std::to_string(fileSize) + " bytes)");
    
    // ... existing code ...
    
    // Read file in chunks to avoid loading everything into memory at once
    // Use same chunk size as TCP buffer (0x20000 = 128KB) for consistency
    constexpr size_t CHUNK_SIZE = 0x20000; // 128KB chunks - same as TCP_BUF_SIZE
    std::vector<char> buffer(CHUNK_SIZE);
    std::vector<char> leftover_buffer;
    size_t totalProcessed = 0;
    
    while (infile && running) {
        infile.read(buffer.data(), CHUNK_SIZE);
        size_t bytes_read = infile.gcount();
        
        if (bytes_read == 0) break;
        
        // Combine leftover from previous chunk with new data
        std::vector<char> combined_buffer;
        combined_buffer.reserve(leftover_buffer.size() + bytes_read);
        combined_buffer.insert(combined_buffer.end(), leftover_buffer.begin(), leftover_buffer.end());
        combined_buffer.insert(combined_buffer.end(), buffer.begin(), buffer.begin() + bytes_read);
        
        // Process the combined buffer
        uint32_t dummy_mark = 0;
        size_t consumed = decodeFromTcp(combined_buffer.data(), combined_buffer.size(), 0, &dummy_mark);
        totalProcessed += consumed;
        
        std::cout << "Processed chunk: " << bytes_read << " bytes read, " << consumed << " bytes consumed, total processed: " << totalProcessed << std::endl;
        
        // Keep any unprocessed data for next iteration
        leftover_buffer.clear();
        if (consumed < combined_buffer.size()) {
            leftover_buffer.assign(combined_buffer.begin() + consumed, combined_buffer.end());
        }
    }
    
    // Process any remaining data in the leftover buffer
    if (!leftover_buffer.empty()) {
        uint32_t dummy_mark = 0;
        size_t consumed = decodeFromTcp(leftover_buffer.data(), leftover_buffer.size(), 0, &dummy_mark);
        totalProcessed += consumed;
        std::cout << "Processed leftover: " << leftover_buffer.size() << " bytes, " << consumed << " bytes consumed" << std::endl;
        
        // Note: If there's still unconsumed data after this, it means the file ended with incomplete packets
        if (consumed < leftover_buffer.size()) {
            logWarning("Warning: File ended with " + std::to_string(leftover_buffer.size() - consumed) + " bytes of incomplete data");
        }
    }
    
    logger.info("Finished decoding file: " + filename + " (total processed: " + std::to_string(totalProcessed) + " bytes)");
}

bool SAMDecoder::decodeBlock(const char* buffer, size_t buffer_size, DecodedBlock& block, size_t& bytes_consumed) {
    bytes_consumed = 0;
    
    // Initialize block with default values
    block.block_id = blocks;
    block.chip_number = 0;
    block.sample_number = 0;
    block.timestamp = 0;
    block.channels.assign(NUM_CHANNELS, 0);  // Initialize with correct size and zero values
    block.status = DecodingStatus::INCOMPLETE_BLOCK;
    block.error_message = "";
    
    // Minimum size check
    if (buffer_size < HEADER_SIZE + DATA_SIZE + FOOTER_SIZE) {
        block.status = DecodingStatus::INCOMPLETE_BLOCK;
        block.error_message = "Buffer too small for complete block";
        return false; // Not enough data, don't consume anything
    }
    
    size_t pos = 0;
    
    // Look for HEADER_1 (0xafaf)
    uint16_t header1 = readWord(buffer + pos);
    if (header1 != 0xafaf) {
        // Search for next header
        pos = 1;
        bool found = false;
        while (pos + 1 < buffer_size) {
            if (readWord(buffer + pos) == 0xafaf) {
                found = true;
                break;
            }
            pos++;
        }
        
        if (!found) {
            // No header found, consume all available data
            bytes_consumed = buffer_size;
            block.status = DecodingStatus::HEADER_1_NOT_FOUND;
            block.error_message = "Header 1 (0xafaf) not found in buffer";
            malformed_blocks.fetch_add(1);
            logWarning("Header 1 not found, consuming " + std::to_string(bytes_consumed) + " bytes");
            return true; // Continue processing
        }
        
        // Found header at new position, consume bytes up to header
        bytes_consumed = pos;
        block.status = DecodingStatus::HEADER_1_NOT_FOUND;
        block.error_message = "Skipped " + std::to_string(pos) + " bytes to find Header 1";
        malformed_blocks.fetch_add(1);
        logWarning(block.error_message);
        return true; // Continue processing
    }
    
    // Extract chip and sample from header 1
    uint16_t header_data = readWord(buffer + pos + 2);
    block.sample_number = header_data & 0xFF;
    block.chip_number = (header_data & 0xFF00) >> 8;
    pos += 4;
    
    // Check remaining space for rest of headers
    if (pos + 12 > buffer_size) {
        block.status = DecodingStatus::INCOMPLETE_BLOCK;
        block.error_message = "Not enough data for complete headers";
        return false; // Don't consume anything
    }
    
    // Check HEADER_2 (0xaffa)
    uint16_t header2 = readWord(buffer + pos);
    if (header2 != 0xaffa) {
        // Look for next header starting from current position
        size_t search_pos = pos - 4 + 1; // Start searching from byte after header1
        bool found = false;
        while (search_pos + 1 < buffer_size) {
            if (readWord(buffer + search_pos) == 0xafaf) {
                found = true;
                break;
            }
            search_pos++;
        }
        
        bytes_consumed = found ? search_pos : pos + 2;
        block.status = DecodingStatus::HEADER_2_NOT_FOUND;
        block.error_message = "Header 2 (0xaffa) not found, expected at position " + std::to_string(pos);
        malformed_blocks.fetch_add(1);
        logWarning(block.error_message);
        return true; // Continue processing
    }
    
    // Extract timestamp from header 2
    uint64_t timestamp_high = static_cast<uint64_t>(readWord(buffer + pos + 2)) << 32;
    pos += 4;
    
    // Check HEADER_3 (0xfaaf)
    uint16_t header3 = readWord(buffer + pos);
    if (header3 != 0xfaaf) {
        // Look for next header
        size_t search_pos = pos - 8 + 1; // Start searching from byte after header1
        bool found = false;
        while (search_pos + 1 < buffer_size) {
            if (readWord(buffer + search_pos) == 0xafaf) {
                found = true;
                break;
            }
            search_pos++;
        }
        
        bytes_consumed = found ? search_pos : pos + 2;
        block.status = DecodingStatus::HEADER_3_NOT_FOUND;
        block.error_message = "Header 3 (0xfaaf) not found, expected at position " + std::to_string(pos);
        malformed_blocks.fetch_add(1);
        logWarning(block.error_message);
        return true; // Continue processing
    }
    
    // Extract timestamp from header 3
    uint64_t timestamp_mid = static_cast<uint64_t>(readWord(buffer + pos + 2)) << 16;
    pos += 4;
    
    // Check HEADER_4 (0xfffa)
    uint16_t header4 = readWord(buffer + pos);
    if (header4 != 0xfffa) {
        // Look for next header
        size_t search_pos = pos - 12 + 1; // Start searching from byte after header1
        bool found = false;
        while (search_pos + 1 < buffer_size) {
            if (readWord(buffer + search_pos) == 0xafaf) {
                found = true;
                break;
            }
            search_pos++;
        }
        
        bytes_consumed = found ? search_pos : pos + 2;
        block.status = DecodingStatus::HEADER_4_NOT_FOUND;
        block.error_message = "Header 4 (0xfffa) not found, expected at position " + std::to_string(pos);
        malformed_blocks.fetch_add(1);
        logWarning(block.error_message);
        return true; // Continue processing
    }
    
    // Extract timestamp from header 4
    uint64_t timestamp_low = static_cast<uint64_t>(readWord(buffer + pos + 2));
    block.timestamp = timestamp_high | timestamp_mid | timestamp_low;
    pos += 4;
    
    // Check if we have enough space for data and footer
    if (pos + DATA_SIZE + FOOTER_SIZE > buffer_size) {
        block.status = DecodingStatus::INCOMPLETE_BLOCK;
        block.error_message = "Not enough data for data section and footer";
        return false; // Don't consume anything
    }
    
    // Extract channel data
    const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(buffer + pos);
    
    // Extract and assign channels directly to the 1D block.channels vector
    extractAndAssignChannels(data_ptr, block.channels);
    
    pos += DATA_SIZE;
    
    // Check footer (0xfafa)
    uint16_t footer = readWord(buffer + pos);
    if (footer != 0xfafa) {
        // Look for next header
        size_t search_pos = pos - (HEADER_SIZE + DATA_SIZE) + 1; // Start searching from byte after header1
        bool found = false;
        while (search_pos + 1 < buffer_size) {
            if (readWord(buffer + search_pos) == 0xafaf) {
                found = true;
                break;
            }
            search_pos++;
        }
        
        bytes_consumed = found ? search_pos : pos + 2;
        block.status = DecodingStatus::FOOTER_NOT_FOUND;
        block.error_message = "Footer (0xfafa) not found, expected at position " + std::to_string(pos);
        malformed_blocks.fetch_add(1);
        logWarning(block.error_message);
        return true; // Continue processing
    }
    
    pos += FOOTER_SIZE;
    
    // Block successfully decoded
    bytes_consumed = pos;
    block.status = DecodingStatus::SUCCESS;
    block.error_message = "";
    
    return true; // Continue processing
}