#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "model/Board/Board.h"
#include "model/SAMDecoder/SAMDecoder.h"
#include "model/Delegates/MakeTTreeDecoderDelegate.h"
#include "model/Delegates/WaveDisplayDecoderDelegate.h"

class CLIInterface {
public:
    CLIInterface();
    ~CLIInterface();

    // Main entry point
    int run(int argc, char* argv[]);

private:
    // Core objects
    Board* fBoard;
    SAMDecoder* decoder;
    MakeTTreeDecoderDelegate* ttreeDelegate;
    WaveDisplayDecoderDelegate* onlineWaveDelegate;

    bool isMultithreaded = false; // Flag for multithreading support
    bool showProgress = false; // Flag to control progress display

    // Configuration
    std::string ipAddress;
    std::string outputDir;
    std::string outputFileName;
    std::string scriptFile;  // Script file path
    bool acquisitionRunning;
    
    // Progress monitoring
    std::string currentProgress;
    std::chrono::steady_clock::time_point lastProgressTime;

    // Command processing
    void showHelp();
    void showStatus();
    void processCommand(const std::string& command);
    void parseArguments(int argc, char* argv[]);
    void interactiveMode();
    int runScript(const std::string& scriptPath);
    void clearLastLines(int num_lines);
    
    // Board operations
    void refreshStatus();
    void togglePower(bool turnOn);
    void setTriggerType(const std::string& type);
    void connectBoard();
    void disconnectBoard();
    void pingBoard();
    void readRegister(const std::string& address);
    void writeRegister(const std::string& address, const std::string& value);
    
    // Configuration operations
    void setPolarity(const std::string& polarity);
    void setGainShaping(const std::string& gainShaping);
    void setNumSamples(const std::string& numSamples);
    void setOutputDir(const std::string& dir);
    void setOutputFile(const std::string& filename);
    void setIP(const std::string& ip);
    void setTriggerThreshold(const std::string& value);
    void setPretrigger(const std::string& value);
    void setExternalClkEnable(const std::string& value);
    
    // Acquisition operations
    void startAcquisition();
    void stopAcquisition();
    void toggleOnlineWaveforms(bool enable);
    
    // Offline processing
    void processOffline(const std::string& inputFile, const std::string& outputFile);
    void showMalformedBlocksStats();
    void resetMalformedBlocksStats();
    
    // Utility functions
    void printMessage(const std::string& message);
    void printError(const std::string& error);
    void sleepSeconds(const std::string& secondsStr);
    void displayProgress(const std::string& progress);
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    uint32_t parseHexOrDec(const std::string& str, bool& ok);
    std::string getCurrentTimestamp();
};