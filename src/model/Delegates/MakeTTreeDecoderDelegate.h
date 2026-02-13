#pragma once
#include "../SAMDecoder/SAMDecoder.h"
#include <TFile.h>
#include <TTree.h>
#include <vector>
#include <string>

// In ROOT analysis:
// flag == 0  → Successful blocks
// flag == 1  → Missing first header
// flag == 2  → Missing second header
// flag == 3  → Missing third header
// flag == 4  → Missing fourth header
// flag == 5  → Missing footer
// flag == 6  → Incomplete data
// flag == 7  → Corrupted data

class MakeTTreeDecoderDelegate : public SAMDecoderDelegate {
public:
    MakeTTreeDecoderDelegate(const std::string& rootFileName);
    ~MakeTTreeDecoderDelegate() override;

    void onBlockDecoded(const DecodedBlock& block) override;
    void acquisitionWillStop() override;
    void close();

private:
    TFile* outfile;
    TTree* tree;
    
    // TTree branch variables
    uint64_t block_id;
    uint8_t chip;
    uint8_t sample;
    uint64_t timestamp;
    std::vector<uint16_t> channels;
    int flag;  // DecodingStatus as integer for ROOT compatibility
};