#include "MakeTTreeDecoderDelegate.h"
#include <filesystem>

MakeTTreeDecoderDelegate::MakeTTreeDecoderDelegate(const std::string& rootFileName)
    : outfile(nullptr),
      tree(nullptr),
      block_id(0), chip(0), sample(0), timestamp(0), flag(0)
{
    // Create output directory if it doesn't exist
    std::filesystem::path filePath(rootFileName);
    std::filesystem::path dirPath = filePath.parent_path();
    if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }
    
    outfile = new TFile(rootFileName.c_str(), "RECREATE");
    tree = new TTree("tree", "Tree with decoded block data");
    
    // Create branches as requested
    tree->Branch("block_id", &block_id, "block_id/l");
    tree->Branch("chip", &chip, "chip/b");
    tree->Branch("sample", &sample, "sample/b");
    tree->Branch("timestamp", &timestamp, "timestamp/l");
    tree->Branch("channels", &channels);  // std::vector<uint16_t>
    tree->Branch("flag", &flag, "flag/I");  // DecodingStatus as integer
}

void MakeTTreeDecoderDelegate::onBlockDecoded(const DecodedBlock& block) {
    // Copy data from DecodedBlock to TTree branch variables
    block_id = block.block_id;
    chip = block.chip_number;
    sample = block.sample_number;
    timestamp = block.timestamp;
    channels = block.channels;  // Direct copy of the vector
    flag = static_cast<int>(block.status);  // Convert enum to int
    // Fill the tree with this block's data
    tree->Fill();
}

MakeTTreeDecoderDelegate::~MakeTTreeDecoderDelegate() {
    close();
}

void MakeTTreeDecoderDelegate::acquisitionWillStop() {
    close();
}

void MakeTTreeDecoderDelegate::close() {
    if (outfile && tree) {
        tree->Write();
        outfile->Close();
        tree = nullptr;
        outfile = nullptr;
    }
}