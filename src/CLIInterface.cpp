#include "CLIInterface.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include "helpers/Constants.h"
#include "model/SAMDecoder/SAMDecoder.h"

CLIInterface::CLIInterface() 
    : fBoard(nullptr),
      decoder(nullptr),
      ttreeDelegate(nullptr),
      onlineWaveDelegate(nullptr),
      ipAddress(K_DEFAULT_IP),
      outputDir(std::filesystem::current_path()),
      outputFileName(K_DEFAULT_OUT_FNAME),
      acquisitionRunning(false),
      lastProgressTime(std::chrono::steady_clock::now())
{
    fBoard = new Board(K_DEFAULT_IP, QString::fromStdString(outputDir));
    fBoard->continuousMonitoring = true; // Enable continuous monitoring by default
    // Set up progress callback for monitoring acquisition
    fBoard->setProgressCallback([this](const std::string& progress) {
        displayProgress(progress);
    });
}

CLIInterface::~CLIInterface() {
    if (acquisitionRunning) {
        stopAcquisition();
    }
    delete fBoard;
    delete decoder;
    delete ttreeDelegate;
    delete onlineWaveDelegate;
}

void CLIInterface::clearLastLines(int num_lines) {
    for (int i = 0; i < num_lines; ++i) {
        // Move cursor up one line and clear the line
        std::cout << "\x1b[1A\x1b[2K";
    }
    std::cout << std::flush; // Ensure the changes are immediately visible
}

int CLIInterface::run(int argc, char* argv[]) {
    std::cout << "SAM DAQ Command Line Interface\n";
    std::cout << "Type 'help' for available commands or 'quit' to exit.\n\n";
    
    parseArguments(argc, argv);
    
    // Check if script file is specified
    if (!scriptFile.empty()) {
        return runScript(scriptFile);
    }
    
    if (argc > 1) {
        // Process single command and exit
        std::string command;
        for (int i = 1; i < argc; ++i) {
            command += std::string(argv[i]);
            if (i < argc - 1) command += " ";
        }
        processCommand(command);
        return 0;
    } else {
        // Interactive mode
        interactiveMode();
        return 0;
    }
}

void CLIInterface::parseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            setIP(argv[++i]);
        } else if (arg == "--output-dir" && i + 1 < argc) {
            setOutputDir(argv[++i]);
        } else if (arg == "--output-file" && i + 1 < argc) {
            setOutputFile(argv[++i]);
        } else if (arg == "--script" && i + 1 < argc) {
            scriptFile = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            showHelp();
            exit(0);
        } else if (arg == "-m") {
            isMultithreaded = true; // Enable multithreading
        }
    }
}

void CLIInterface::interactiveMode() {
    std::string input;
    
    while (true) {
        std::cout << "SAM_DAQ> " << std::flush;
        std::getline(std::cin, input);
        
        if (input.empty()) {
            // Just continue to next iteration - progress is handled by displayProgress callback
            continue;
        }
        
        if (input == "quit" || input == "exit") break;
        
        processCommand(input);
        
        // Clear progress after command execution
        currentProgress.clear();
    }
}

int CLIInterface::runScript(const std::string& scriptPath) {
    std::ifstream scriptFile(scriptPath);
    if (!scriptFile.is_open()) {
        printError("Failed to open script file: " + scriptPath);
        return 1;
    }
    
    std::string line;
    int lineNumber = 0;
    bool exitScript = false;
    
    printMessage("Executing script: " + scriptPath);
    
    while (std::getline(scriptFile, line)) {
        lineNumber++;
        
        // Remove comments (everything after #)
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // Trim whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), line.end());
        
        // Skip empty lines
        if (line.empty()) continue;
        
        // Check for exit commands
        if (line == "quit" || line == "exit") {
            exitScript = true;
            break;
        }
        
        printMessage("[" + std::to_string(lineNumber) + "] " + line);
        
        try {
            processCommand(line);
        } catch (const std::exception& e) {
            printError("Script error at line " + std::to_string(lineNumber) + ": " + e.what());
            return 1;
        }
        
        // Add small delay between commands for stability
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    scriptFile.close();
    
    if (exitScript) {
        printMessage("Script completed successfully");
    } else {
        printMessage("Script finished (end of file)");
    }
    
    return 0;
}

void CLIInterface::processCommand(const std::string& command) {
    auto tokens = splitString(command, ' ');
    if (tokens.empty()) return;
    
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    try {
        if (cmd == "help") {
            showHelp();
        } else if (cmd == "status") {
            showStatus();
        } else if (cmd == "refresh") {
            refreshStatus();
        } else if (cmd == "power") {
            if (tokens.size() > 1) {
                togglePower(tokens[1] == "on" || tokens[1] == "1");
            } else {
                printError("Usage: power <on|off>");
            }
        } else if (cmd == "trigger") {
            if (tokens.size() > 1) {
                setTriggerType(tokens[1]);
            } else {
                printError("Usage: trigger <self|external|1khz|1mhz>");
            }
        } else if (cmd == "trigger-threshold") {
            if (tokens.size() > 1) {
                setTriggerThreshold(tokens[1]);
            } else {
                printError("Usage: trigger-threshold <0-1023>");
            }
        } else if (cmd == "connect") {
            connectBoard();
        } else if (cmd == "disconnect") {
            disconnectBoard();
        } else if (cmd == "ping") {
            pingBoard();
        } else if (cmd == "read") {
            if (tokens.size() > 1) {
                readRegister(tokens[1]);
            } else {
                printError("Usage: read <address>");
            }
        } else if (cmd == "write") {
            if (tokens.size() > 2) {
                writeRegister(tokens[1], tokens[2]);
            } else {
                printError("Usage: write <address> <value>");
            }
        } else if (cmd == "polarity") {
            if (tokens.size() > 1) {
                setPolarity(tokens[1]);
            } else {
                printError("Usage: polarity <positive|negative>");
            }
        } else if (cmd == "gain") {
            if (tokens.size() > 1) {
                setGainShaping(tokens[1]);
            } else {
                printError("Usage: gain <1|2|3> (1=30mV/160ns, 2=20mV/160ns, 3=4mV/300ns)");
            }
        } else if (cmd == "samples") {
            if (tokens.size() > 1) {
                setNumSamples(tokens[1]);
            } else {
                printError("Usage: samples <16|32|64|128>");
            }
        } else if (cmd == "output-dir") {
            if (tokens.size() > 1) {
                setOutputDir(tokens[1]);
            } else {
                printError("Usage: output-dir <directory>");
            }
        } else if (cmd == "output-file") {
            if (tokens.size() > 1) {
                setOutputFile(tokens[1]);
            } else {
                printError("Usage: output-file <filename>");
            }
        } else if (cmd == "ip") {
            if (tokens.size() > 1) {
                setIP(tokens[1]);
            } else {
                printError("Usage: ip <ip_address>");
            }
        } else if (cmd == "start") {
            startAcquisition();
        } else if (cmd == "stop") {
            stopAcquisition();
        } else if (cmd == "sleep") {
            if (tokens.size() > 1) {
                sleepSeconds(tokens[1]);
            } else {
                printError("Usage: sleep <seconds>");
            }
        } else if (cmd == "online") {
            if (tokens.size() > 1) {
                toggleOnlineWaveforms(tokens[1] == "on" || tokens[1] == "enable");
            } else {
                printError("Usage: online <on|off>");
            }
        } else if (cmd == "pretrigger") {
            if (tokens.size() > 1) {
                setPretrigger(tokens[1]);
            } else {
                printError("Usage: pretrigger <0|4|8|16>");
            }
        } else if (cmd == "external-clk") {
            if (tokens.size() > 1) {
                setExternalClkEnable(tokens[1]);
            } else {
                printError("Usage: external-clk <on|off>");
            }
        } else if (cmd == "process-offline") {
            if (tokens.size() > 2) {
                processOffline(tokens[1], tokens[2]);
            } else {
                printError("Usage: process-offline <input.bin> <output.root>");
            }
        } else if (cmd == "malformed-blocks") {
            showMalformedBlocksStats();
        } else if (cmd == "reset-malformed") {
            resetMalformedBlocksStats();
            printMessage("Malformed blocks counter has been reset to 0.");
        } else if (cmd == "stat") {
            showProgress = !showProgress;
            printMessage(std::string("Progress display ") + (showProgress ? "enabled" : "disabled"));
            if (acquisitionRunning && !currentProgress.empty()) {
                printMessage("Current progress: " + currentProgress);
            }
        } else if (cmd == "quit" || cmd == "exit") {
            // Handle quit command - this will be caught by the calling context
            printMessage("Exiting...");
            exit(0);
        } else {
            printError("Unknown command: " + cmd + ". Type 'help' for available commands.");
        }
    } catch (const std::exception& e) {
        printError("Exception: " + std::string(e.what()));
    }
}

void CLIInterface::showHelp() {
    std::cout << "SAM DAQ Command Line Interface\n\n";
    
    std::cout << "Command-line options:\n";
    std::cout << "  --script <file>          - Execute commands from script file\n";
    std::cout << "  --ip <address>           - Set board IP address\n";
    std::cout << "  --output-dir <dir>       - Set output directory\n";
    std::cout << "  --output-file <file>     - Set output filename\n";
    std::cout << "  --help, -h               - Show this help\n";
    std::cout << "  -m                       - Enable multithreading\n\n";
    
    std::cout << "Available commands:\n";
    std::cout << "  help                     - Show this help\n";
    std::cout << "  status                   - Show current board status\n";
    std::cout << "  refresh                  - Refresh board status\n";
    std::cout << "  power <on|off>           - Turn power on or off\n";
    std::cout << "  trigger <self|external|1khz|1mhz> - Set trigger type\n";
    std::cout << "  trigger-threshold <0-1023> - Set trigger threshold\n";
    std::cout << "  connect                  - Connect to board\n";
    std::cout << "  disconnect               - Disconnect from board\n";
    std::cout << "  ping                     - Ping the board\n";
    std::cout << "  read <address>           - Read from register address\n";
    std::cout << "  write <address> <value>  - Write value to register address\n";
    std::cout << "  polarity <positive|negative> - Set signal polarity\n";
    std::cout << "  gain <1|2|3>             - Set gain/shaping (1=30mV/160ns, 2=20mV/160ns, 3=4mV/300ns)\n";
    std::cout << "  samples <16|32|64|128>   - Set number of samples\n";
    std::cout << "  output-dir <directory>   - Set output directory\n";
    std::cout << "  output-file <filename>   - Set output filename\n";
    std::cout << "  ip <ip_address>          - Set board IP address\n";
    std::cout << "  start                    - Start data acquisition\n";
    std::cout << "  stop                     - Stop data acquisition\n";
    std::cout << "  sleep <seconds>          - Sleep for specified number of seconds\n";
    std::cout << "  online <on|off>          - Enable/disable online waveform display\n";
    std::cout << "  pretrigger <0|4|8|16>    - Set pretrigger samples\n";
    std::cout << "  external-clk <on|off>     - Enable/disable external clock\n";
    std::cout << "  process-offline <input.bin> <output.root> - Process bin file to ROOT offline\n";
    std::cout << "  malformed-blocks            - Show malformed blocks statistics\n";
    std::cout << "  reset-malformed            - Reset malformed blocks counter\n";
    std::cout << "  stat                       - Toggle progress display on/off\n";
    std::cout << "  quit                     - Exit program\n\n";
    
    std::cout << "Scripting:\n";
    std::cout << "  Use --script option to execute commands from a file\n";
    std::cout << "  Script files support comments with # and empty lines\n";
    std::cout << "  Example: samdaq --script my_script.txt\n";
}

void CLIInterface::showStatus() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    std::cout << "\n--- Board Status ---\n";
    std::cout << "IP Address: " << fBoard->ipAddress.toStdString() << "\n";
    std::cout << "Connected: " << (fBoard->isConnected ? "Yes" : "No") << "\n";
    std::cout << "Power: " << (fBoard->powerOn ? "On" : "Off") << "\n";
    std::cout << "Trigger Type: " << TriggerTypeStrings::toString(fBoard->triggerType) << "\n";
    std::cout << "Trigger Threshold: " << fBoard->triggerThreshold << "\n";
    std::cout << "Polarity: " << (fBoard->polarity ? "Positive" : "Negative") << "\n";
    std::cout << "Gain: " << GainStrings::toString(fBoard->gain) << "\n";
    std::cout << "Shaping: " << ShapingStrings::toString(fBoard->shaping) << "\n";
    std::cout << "Samples: " << NumSamplesStrings::toString(fBoard->numSamples) << "\n";
    std::cout << "Last Update: " << fBoard->lastUpdate.toString("yyyy-MM-dd HH:mm:ss").toStdString() << "\n";
    std::cout << "Output Directory: " << outputDir << "\n";
    std::cout << "Output Filename: " << outputFileName << "\n";
    std::cout << "Acquisition: " << (acquisitionRunning ? "Running" : "Stopped") << "\n";
    std::cout << "-------------------\n\n";
}

void CLIInterface::refreshStatus() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    QString error = fBoard->refreshPONValues();
    if (error.isEmpty()) {
        printMessage("Status refreshed successfully");
        showStatus();
    } else {
        printError("Refresh failed: " + error.toStdString());
    }
}

void CLIInterface::togglePower(bool turnOn) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    QString error = fBoard->powerOnOff(turnOn);
    if (error.isEmpty()) {
        printMessage(std::string("Power ") + (turnOn ? "on" : "off") + " successful");
    } else {
        printError("Power operation failed: " + error.toStdString());
    }
}

void CLIInterface::setTriggerType(const std::string& type) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    TriggerType triggerType;
    if (type == "self" || type == "internal") {
        triggerType = TriggerType::SelfTrigger;
    } else if (type == "external") {
        triggerType = TriggerType::External;
    } else if (type == "1khz") {
        triggerType = TriggerType::KHz1;
    } else if (type == "1mhz") {
        triggerType = TriggerType::MHz1;
    } else {
        printError("Invalid trigger type. Use 'self', 'external', '1khz', or '1mhz'");
        return;
    }
    QString error = fBoard->setTriggerType(triggerType);
    if (error.isEmpty()) {
        printMessage("Trigger type set to " + type);
    } else {
        printError("Failed to set trigger type: " + error.toStdString());
    }
}

void CLIInterface::setTriggerThreshold(const std::string& value) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    int threshold = 0;
    try {
        threshold = std::stoi(value);
    } catch (...) {
        printError("Invalid threshold. Must be 0-1023");
        return;
    }
    if (threshold < 0 || threshold > 1023) {
        printError("Invalid threshold. Must be 0-1023");
        return;
    }
    fBoard->triggerThreshold = static_cast<uint16_t>(threshold);
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, fBoard->gain, fBoard->shaping, fBoard->numSamples, fBoard->triggerThreshold, fBoard->pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        printMessage("Trigger threshold set to " + std::to_string(threshold));
    } else {
        printError("Failed to set trigger threshold: " + error.toStdString());
    }
}

void CLIInterface::connectBoard() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    QString error = fBoard->connect();
    if (error.isEmpty()) {
        printMessage("Connected successfully");
        fBoard->setCurrentPONValues();
        refreshStatus();
    } else {
        printError("Connection failed: " + error.toStdString());
    }
}

void CLIInterface::disconnectBoard() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    // Implement disconnect logic if available in Board class
    printMessage("Disconnected");
}

void CLIInterface::pingBoard() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    QString error = fBoard->ping();
    if (error.isEmpty()) {
        printMessage("Ping successful");
    } else {
        printError("Ping failed: " + error.toStdString());
    }
}

void CLIInterface::readRegister(const std::string& address) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    bool ok;
    uint32_t addr = parseHexOrDec(address, ok);
    if (!ok) {
        printError("Invalid address format");
        return;
    }
    
    uint32_t result = 0;
    QString error = fBoard->readRegister(addr, &result);
    if (error.isEmpty()) {
        std::cout << "Read from 0x" << std::hex << addr << ": 0x" << result << std::dec << std::endl;
    } else {
        printError("Read failed: " + error.toStdString());
    }
}

void CLIInterface::writeRegister(const std::string& address, const std::string& value) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    bool okAddr, okVal;
    uint32_t addr = parseHexOrDec(address, okAddr);
    uint32_t val = parseHexOrDec(value, okVal);
    
    if (!okAddr || !okVal) {
        printError("Invalid address or value format");
        return;
    }
    
    QString error = fBoard->writeRegister(addr, val);
    if (error.isEmpty()) {
        std::cout << "Wrote 0x" << std::hex << val << " to 0x" << addr << std::dec << std::endl;
    } else {
        printError("Write failed: " + error.toStdString());
    }
}

void CLIInterface::setPolarity(const std::string& polarity) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    bool pol;
    if (polarity == "positive") {
        pol = true;
    } else if (polarity == "negative") {
        pol = false;
    } else {
        printError("Invalid polarity. Use 'positive' or 'negative'");
        return;
    }
    
    QString error = fBoard->setPolarity(pol);
    if (error.isEmpty()) {
        printMessage("Polarity set to " + polarity);
    } else {
        printError("Failed to set polarity: " + error.toStdString());
    }
}

void CLIInterface::setGainShaping(const std::string& gainShaping) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    Gain gain;
    Shaping shaping;
    if (gainShaping == "1") {
        gain = Gain::G_30_mV_fC;
        shaping = Shaping::SH_160_ns;
    } else if (gainShaping == "2") {
        gain = Gain::G_20_mV_fC;
        shaping = Shaping::SH_160_ns;
    } else if (gainShaping == "3") {
        gain = Gain::G_4_mV_fC;
        shaping = Shaping::SH_300_ns;
    } else {
        printError("Invalid gain/shaping. Use 1, 2, or 3");
        return;
    }
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, gain, shaping, fBoard->numSamples, fBoard->triggerThreshold, fBoard->pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        printMessage("Gain/Shaping set");
    } else {
        printError("Failed to set gain/shaping: " + error.toStdString());
    }
}

void CLIInterface::setNumSamples(const std::string& numSamples) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    NumSamples samples;
    if (numSamples == "16") {
        samples = NumSamples::N_16;
    } else if (numSamples == "32") {
        samples = NumSamples::N_32;
    } else if (numSamples == "64") {
        samples = NumSamples::N_64;
    } else if (numSamples == "128") {
        samples = NumSamples::N_128;
    } else {
        printError("Invalid number of samples. Use 16, 32, 64, or 128");
        return;
    }
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, fBoard->gain, fBoard->shaping, samples, fBoard->triggerThreshold, fBoard->pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        printMessage("Number of samples set to " + numSamples);
    } else {
        printError("Failed to set number of samples: " + error.toStdString());
    }
}

void CLIInterface::setOutputDir(const std::string& dir) {
    outputDir = dir;
    printMessage("Output directory set to: " + dir);
}

void CLIInterface::setOutputFile(const std::string& filename) {
    outputFileName = filename;
    printMessage("Output filename set to: " + filename);
}

void CLIInterface::setIP(const std::string& ip) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    ipAddress = ip;
    fBoard->ipAddress = QString::fromStdString(ip);
    printMessage("IP address set to: " + ip);
}

void CLIInterface::startAcquisition() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    if (acquisitionRunning) {
        printError("Acquisition already running");
        return;
    }
    
    std::string basename = outputDir + "/" + outputFileName;
    decoder = new SAMDecoder(basename, isMultithreaded);
    ttreeDelegate = new MakeTTreeDecoderDelegate(basename + ".root");
    decoder->addDelegate(ttreeDelegate);
    
    QString error = fBoard->startAcquisition(decoder);
    if (error.isEmpty()) {
        acquisitionRunning = true;
        printMessage("Acquisition started. Files: " + basename + ".root, " + basename + ".bin");
    } else {
        printError("Failed to start acquisition: " + error.toStdString());
        delete decoder;
        delete ttreeDelegate;
        decoder = nullptr;
        ttreeDelegate = nullptr;
    }
}

void CLIInterface::stopAcquisition() {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    
    if (!acquisitionRunning) {
        printError("Acquisition not running");
        return;
    }
    
    QString error = fBoard->stopAcquisition();
    if (error.isEmpty()) {
        acquisitionRunning = false;
        if (ttreeDelegate) {
            ttreeDelegate->close();
        }
        printMessage("Acquisition stopped");
    } else {
        printError("Failed to stop acquisition: " + error.toStdString());
    }
}

void CLIInterface::toggleOnlineWaveforms(bool enable) {
    if (!fBoard || !decoder) {
        printError("Board or decoder not initialized");
        return;
    }
    
    if (enable) {
        if (!onlineWaveDelegate) {
            onlineWaveDelegate = new WaveDisplayDecoderDelegate();
            decoder->addDelegate(onlineWaveDelegate);
            onlineWaveDelegate->createWaveformCanvasAndGraph(0, 0);
            printMessage("Online waveform display enabled");
        } else {
            printMessage("Online waveform display already enabled");
        }
    } else {
        if (onlineWaveDelegate) {
            decoder->removeDelegate(onlineWaveDelegate);
            delete onlineWaveDelegate;
            onlineWaveDelegate = nullptr;
            printMessage("Online waveform display disabled");
        } else {
            printMessage("Online waveform display already disabled");
        }
    }
}

void CLIInterface::printMessage(const std::string& message) {
    std::cout << "[" << getCurrentTimestamp() << "] " << message << std::endl;
}

void CLIInterface::printError(const std::string& error) {
    std::cerr << "[" << getCurrentTimestamp() << "] ERROR: " << error << std::endl;
}

std::vector<std::string> CLIInterface::splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

uint32_t CLIInterface::parseHexOrDec(const std::string& str, bool& ok) {
    try {
        if (str.find("0x") == 0 || str.find("0X") == 0) {
            ok = true;
            return std::stoul(str, nullptr, 16);
        } else {
            ok = true;
            return std::stoul(str, nullptr, 10);
        }
    } catch (...) {
        ok = false;
        return 0;
    }
}

std::string CLIInterface::getCurrentTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void CLIInterface::setPretrigger(const std::string& value) {
    if (!fBoard) {
        printError("Board not initialized");
        return;
    }
    uint8_t pretrigger = 0;
    if (value == "0") {
        pretrigger = 0;
    } else if (value == "4") {
        pretrigger = 1;
    } else if (value == "8") {
        pretrigger = 2;
    } else if (value == "16") {
        pretrigger = 3;
    } else {
        printError("Invalid pretrigger value. Must be 0, 4, 8, or 16");
        return;
    }
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, fBoard->gain, fBoard->shaping, fBoard->numSamples, fBoard->triggerThreshold, pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        printMessage("Pretrigger value set to " + value);
    } else {
        printError("Failed to set pretrigger value: " + error.toStdString());
    }
}

void CLIInterface::setExternalClkEnable(const std::string& value) {
    bool enable = false;
    if (value == "on" || value == "enable" || value == "1") {
        enable = true;
    } else if (value == "off" || value == "disable" || value == "0") {
        enable = false;
    } else {
        printError("Invalid external clock value. Must be on/off or enable/disable");
        return;
    }
    QString error = fBoard->setExternalClkEnable(enable);
    if (error.isEmpty()) {
        printMessage("External clock " + std::string(enable ? "enabled" : "disabled"));
    } else {
        printError("Failed to set external clock: " + error.toStdString());
    }
}

void CLIInterface::processOffline(const std::string& inputFile, const std::string& outputFile) {
    printMessage("Starting offline processing...");
    printMessage("Input file: " + inputFile);
    printMessage("Output file: " + outputFile);
    
    // Check if input file exists
    if (!std::filesystem::exists(inputFile)) {
        printError("Input file does not exist: " + inputFile);
        return;
    }
    
    try {
        // Create decoder for offline processing
        SAMDecoder* offlineDecoder = new SAMDecoder("output/offline_temp", isMultithreaded);
        
        // Create ROOT file delegate
        auto rootDelegate = new MakeTTreeDecoderDelegate(outputFile);
        offlineDecoder->addDelegate(rootDelegate);
        
        printMessage("Processing file: " + inputFile + " -> " + outputFile);
        
        // Process the file
        offlineDecoder->decodeFromFile(inputFile);
        
        // Stop the decoder properly
        offlineDecoder->stop();
        
        // Explicitly close the ROOT delegate to ensure data is written
        rootDelegate->close();
        
        // Show malformed blocks statistics
        size_t malformed_count = offlineDecoder->getMalformedBlocksCount();
        size_t total_blocks = offlineDecoder->getTotalBlocksCount();
        size_t good_blocks = total_blocks - malformed_count;
        
        printMessage("=== Offline Processing Statistics ===");
        printMessage("Total blocks processed: " + std::to_string(total_blocks));
        printMessage("Good blocks: " + std::to_string(good_blocks));
        printMessage("Malformed blocks: " + std::to_string(malformed_count));
        if (malformed_count > 0) {
            printMessage("Warning: " + std::to_string(malformed_count) + " malformed blocks were detected and skipped.");
        }
        printMessage("=====================================");
        
        // Clean up
        offlineDecoder->close();
        delete offlineDecoder;
        
        printMessage("Offline processing completed successfully!");
        printMessage("Output ROOT file created: " + outputFile);
        
    } catch (const std::exception& e) {
        printError("Error during offline processing: " + std::string(e.what()));
    } catch (...) {
        printError("Unknown error during offline processing.");
    }
}

void CLIInterface::showMalformedBlocksStats() {
    if (!decoder) {
        printError("No active decoder. Start acquisition first to see live statistics.");
        return;
    }
    
    size_t count = decoder->getMalformedBlocksCount();
    printMessage("Malformed blocks detected: " + std::to_string(count));
    
    if (count > 0) {
        printMessage("These blocks were corrupted/incomplete and were skipped during processing.");
        printMessage("This may indicate network issues or data transmission problems.");
    } else {
        printMessage("No malformed blocks detected - data integrity looks good!");
    }
}

void CLIInterface::resetMalformedBlocksStats() {
    if (decoder) {
        decoder->resetMalformedBlocksCount();
    }
}

void CLIInterface::sleepSeconds(const std::string& secondsStr) {
    try {
        int seconds = std::stoi(secondsStr);
        if (seconds < 0) {
            printError("Sleep time cannot be negative");
            return;
        }
        
        if (seconds > 3600) { // Max 1 hour to prevent accidental long sleeps
            printError("Sleep time cannot exceed 3600 seconds (1 hour)");
            return;
        }
        
        printMessage("Sleeping for " + std::to_string(seconds) + " seconds...");
        
        // Use std::this_thread::sleep_for for cross-platform compatibility
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        
        printMessage("Sleep completed");
        
    } catch (const std::invalid_argument& e) {
        printError("Invalid number format for sleep time: " + secondsStr);
    } catch (const std::out_of_range& e) {
        printError("Sleep time value out of range: " + secondsStr);
    }
}

void CLIInterface::displayProgress(const std::string& progress) {
    // Store the latest progress information
    currentProgress = progress;
    
    // Only display progress if enabled and acquisition is running
    if (showProgress && acquisitionRunning) {
        // Only display if it's been a while since last display to avoid spam
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProgressTime).count() > 500) {
            // Save current cursor position
            std::cout << "\x1b[s";
            // Move cursor to start of line above prompt
            std::cout << "\x1b[1A\x1b[2K";
            // Print progress
            std::cout << "[PROGRESS] " << progress;
            // Restore cursor position
            std::cout << "\x1b[u" << std::flush;
            lastProgressTime = now;
        }
    }
}