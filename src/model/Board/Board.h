#pragma once
#include <QString>
#include <QDateTime>
#include <thread>
#include <atomic>
#include <functional>
#include "../Options.h"
#include "../SAMDecoder/SAMDecoder.h"

class Board {
public:
    enum class Status {
        Disconnected,
        Connecting,
        Connected,
        Acquiring,
        Stopped,
        Error
    };

    // Progress callback function type
    using ProgressCallback = std::function<void(const std::string&)>;

    Board(QString ip = QString(), QString outDir = "./");

    // Set progress callback for monitoring acquisition progress
    void setProgressCallback(ProgressCallback callback) { progressCallback = callback; }

    // Board status fields
    QString ipAddress;
    bool isConnected;
    bool powerOn;
    bool continuousMonitoring = false; // Continuous monitoring flag
    
    QDateTime lastUpdate;
    Status connectionStatus;
    QString lastError;
    
    TriggerType triggerType; // Using enum class from Options.h
    Gain gain; // Using enum class from Options.h
    Shaping shaping; // Using enum class from Options.h
    bool polarity; // Polarity for the PON register
    NumSamples numSamples; // <--- Add this line for number of samples
    uint16_t triggerThreshold = 0; // Add trigger threshold member
    uint8_t pretrigger = 0; // Add pretrigger member
    bool externalClkEnable = false; // Add external clock enable member
    // Connection object (assumed to be defined elsewhere)
    class Connection* connection; // Pointer to a Connection object
    SAMDecoder* decoder; // Pointer to a SAMDecoder object

    // Utility methods
    QString connect();
    QString ping();
    QString readRegister(uint32_t address, uint32_t* value);
    QString writeRegister(uint32_t address, uint32_t value);
    QString getStatusString() const;
    QString getLastError() const { return lastError; }
    QString powerOnOff(bool on);
    QString startAcquisition(SAMDecoder* decoder);
    QString stopAcquisition();
    QString refreshPONValues();
    QString setPONValues(bool powerOn, bool polarity, TriggerType triggerType, Gain gain, Shaping shaping, NumSamples numSamples, uint16_t triggerThreshold, uint8_t pretrigger, bool externalClkEnable);
    QString setCurrentPONValues();
    QString setTriggerType(TriggerType type);
    QString setGain(Gain gain);
    QString setShaping(Shaping shaping);
    QString setPolarity(bool polarity);
    QString setNumSamples(NumSamples numSamples);
    QString setExternalClkEnable(bool enable);

    // Progress monitoring methods
    void setupProgressMonitoring();
    void cleanupProgressMonitoring();

private:
    std::thread acquisitionThread;
    std::thread progressThread;
    std::atomic<bool> acquisitionActive{false};
    int tcp_sockfd = -1; // Store the socket fd for closing
    ProgressCallback progressCallback; // Progress callback function
    
    // Progress monitoring variables
    int stderr_pipe[2] = {-1, -1}; // Pipe for stderr redirection
    int original_stderr = -1; // Original stderr file descriptor
    std::atomic<bool> progressMonitoringActive{false};
};