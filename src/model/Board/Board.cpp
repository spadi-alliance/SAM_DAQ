#include "Board.h"
#include "../Connection/Connection.h"
#include "../SAMDecoder/SAMDecoder.h"
#include <unistd.h> // for close(), pipe(), dup2()
#include <fcntl.h>  // for fcntl()
#include <atomic>
#include <thread>
#include <QApplication> // Add this line for QApplication
#include "../../helpers/Constants.h"

extern "C"
{
#include "../../../lib/fakernet/client/fnetctrl.h"
}

// Handler that checks for stop flag (optional, for more control)
static size_t tcp_handler_with_stop(char *buffer, size_t fill, uint64_t handled, void *pinfo, uint32_t *cur_mark)
{
    SAMDecoder *decoder = static_cast<SAMDecoder *>(pinfo);
    if (decoder)
    {
        return decoder->decodeFromTcp(buffer, fill, handled, cur_mark);
    }
    return fill;
}

Board::Board(QString ip, QString outDir)
    : ipAddress(ip),
      isConnected(false),
      powerOn(false),
      triggerType(TriggerType::External),
      gain(Gain::G_30_mV_fC),
      shaping(Shaping::SH_160_ns),
      polarity(false),
      numSamples(NumSamples::N_16), // <--- Add this initialization
      lastUpdate(QDateTime::currentDateTime()),
      connectionStatus(Status::Disconnected),
      lastError(""),
      connection(new Connection()),
      tcp_sockfd(-1),
      acquisitionActive(false),
      decoder(nullptr),
      externalClkEnable(false) // Initialize external clock enable
{
}

QString Board::connect()
{
    connectionStatus = Status::Connecting;
    QString error;
    if (connection && connection->connectToBoard(ipAddress, true, &error))
    {
        isConnected = true;
        connectionStatus = Status::Connected;
        lastUpdate = QDateTime::currentDateTime();
        lastError = "";
        return QString(); // No error
    }
    else
    {
        isConnected = false;
        connectionStatus = Status::Error;
        lastError = error;
        return error;
    }
}

QString Board::ping()
{
    if (connection)
    {
        QString error;
        bool result = connection->pingBoard(ipAddress, &error);
        lastUpdate = QDateTime::currentDateTime();
        if (!result)
        {
            lastError = error;
            connectionStatus = Status::Error;
            return error;
        }
        return QString();
    }
    lastError = "No connection object";
    connectionStatus = Status::Error;
    return lastError;
}

QString Board::readRegister(uint32_t address, uint32_t *value)
{
    if (connection)
    {
        QString error;
        bool result = connection->readRegister(address, value, &error);
        lastUpdate = QDateTime::currentDateTime();
        if (!result)
        {
            lastError = error;
            connectionStatus = Status::Error;
            return error;
        }
        return QString();
    }
    lastError = "No connection object";
    connectionStatus = Status::Error;
    return lastError;
}

QString Board::writeRegister(uint32_t address, uint32_t value)
{
    if (connection)
    {
        QString error;
        bool result = connection->writeRegister(address, value, &error);
        lastUpdate = QDateTime::currentDateTime();
        if (!result)
        {
            lastError = error;
            connectionStatus = Status::Error;
            return error;
        }
        return QString();
    }
    lastError = "No connection object";
    connectionStatus = Status::Error;
    return lastError;
}

QString Board::powerOnOff(bool on)
{
    return setPONValues(on, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setTriggerType(TriggerType type)
{
    return setPONValues(powerOn, polarity, type, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setGain(Gain gain)
{
    return setPONValues(powerOn, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setShaping(Shaping shaping)
{
    return setPONValues(powerOn, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setPolarity(bool polarity)
{
    return setPONValues(powerOn, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setNumSamples(NumSamples numSamples)
{
    return setPONValues(powerOn, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setExternalClkEnable(bool enable)
{
    return setPONValues(powerOn, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, enable);
}

QString Board::startAcquisition(SAMDecoder *newDecoder)
{
    if (!connection)
    {
        lastError = "No connection object";
        connectionStatus = Status::Error;
        return lastError;
    }

    if (acquisitionActive)
    {
        lastError = "Acquisition already active";
        connectionStatus = Status::Error;
        return lastError;
    }

    if (!newDecoder)
    {
        lastError = "No decoder provided";
        connectionStatus = Status::Error;
        return lastError;
    }


    QString error;

    // Write 0x1 (start) to 0x1000
    if (!connection->writeRegister(0x1000, 0x1, &error))
    {
        lastError = error;
        connectionStatus = Status::Error;
        return "Failed to write to 0x1000: " + error;
    }

    // Write 0x1 (start) to 0x500
    if (!connection->writeRegister(0x500, 0x1, &error))
    {
        lastError = error;
        connectionStatus = Status::Error;
        return "Failed to write to 0x500: " + error;
    }

    if (!connection->writeRegister(0x05, 0x03, &error))
    {
        lastError = error;
        connectionStatus = Status::Error;
        return "Failed to write to 0x05: " + error;
    }

    struct fnet_ctrl_client *client = connection->getFnetClient();
    if (!client)
    {
        lastError = "No fnet_ctrl_client available";
        connectionStatus = Status::Error;
        return lastError;
    }

    // Open TCP socket
    tcp_sockfd = fnet_ctrl_open_tcp(client);
    if (tcp_sockfd < 0)
    {
        lastError = QString("Failed to open TCP socket: %1").arg(fnet_ctrl_last_error(client));
        connectionStatus = Status::Error;
        return lastError;
    }

    if (decoder) {
        delete decoder;
        decoder = nullptr;
    }
    decoder = newDecoder;

    acquisitionActive = true;
    connectionStatus = Status::Acquiring;

    // Set up progress monitoring if callback is provided and continuous monitoring is enabled
    if (progressCallback && continuousMonitoring) {
        setupProgressMonitoring();
    }

    // Launch acquisition in a separate thread
    acquisitionThread = std::thread([this, client]()
                                    {
        // Use a handler that checks acquisitionActive
        fnet_tcp_read(client, tcp_sockfd, tcp_handler_with_stop, decoder, continuousMonitoring);
        // When done, close the socket
        close(tcp_sockfd);
        tcp_sockfd = -1;
        acquisitionActive = false;
        connectionStatus = Status::Connected;
        
        // Clean up progress monitoring
        if (progressMonitoringActive) {
            cleanupProgressMonitoring();
        }
    });
    acquisitionThread.detach();

    lastUpdate = QDateTime::currentDateTime();
    lastError = "";
    return QString(); // No error
}

void Board::setupProgressMonitoring() {
    // Create a pipe for stderr redirection
    if (pipe(stderr_pipe) == -1) {
        // If pipe creation fails, continue without progress monitoring
        return;
    }
    
    // Make the read end non-blocking
    int flags = fcntl(stderr_pipe[0], F_GETFL, 0);
    fcntl(stderr_pipe[0], F_SETFL, flags | O_NONBLOCK);
    
    // Save original stderr
    original_stderr = dup(STDERR_FILENO);
    
    // Redirect stderr to the pipe
    dup2(stderr_pipe[1], STDERR_FILENO);
    close(stderr_pipe[1]); // Close write end in parent process
    
    progressMonitoringActive = true;
    
    // Start progress monitoring thread
    progressThread = std::thread([this]() {
        char buffer[1024];
        std::string progressLine;
        
        while (progressMonitoringActive) {
            ssize_t bytesRead = read(stderr_pipe[0], buffer, sizeof(buffer) - 1);
            
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                progressLine += buffer;
                
                // Process complete lines (look for \r or \n)
                size_t pos;
                while ((pos = progressLine.find('\r')) != std::string::npos ||
                       (pos = progressLine.find('\n')) != std::string::npos) {
                    std::string line = progressLine.substr(0, pos);
                    progressLine = progressLine.substr(pos + 1);
                    
                    // Call the progress callback with the line
                    if (progressCallback && !line.empty()) {
                        progressCallback(line);
                    }
                }
            } else if (bytesRead == 0) {
                // EOF - pipe was closed
                break;
            }
            
            // Small delay to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Process any remaining data
        if (progressCallback && !progressLine.empty()) {
            progressCallback(progressLine);
        }
    });
}

void Board::cleanupProgressMonitoring() {
    if (!progressMonitoringActive) {
        return;
    }
    
    progressMonitoringActive = false;
    
    // Wait for progress thread to finish
    if (progressThread.joinable()) {
        progressThread.join();
    }
    
    // Restore original stderr
    if (original_stderr != -1) {
        dup2(original_stderr, STDERR_FILENO);
        close(original_stderr);
        original_stderr = -1;
    }
    
    // Close pipe
    if (stderr_pipe[0] != -1) {
        close(stderr_pipe[0]);
        stderr_pipe[0] = -1;
    }
}

QString Board::stopAcquisition()
{
    if (!connection)
    {
        lastError = "No connection object";
        connectionStatus = Status::Error;
        return lastError;
    }

    QString error;

    // Write 0x0 (stop) to 0x1000
    if (!connection->writeRegister(0x1000, 0x0, &error))
    {
        lastError = error;
        connectionStatus = Status::Error;
        return "Failed to write to 0x1000: " + error;
    }

    // Write 0x0 (stop) to 0x500
    if (!connection->writeRegister(0x500, 0x0, &error))
    {
        lastError = error;
        connectionStatus = Status::Error;
        return "Failed to write to 0x500: " + error;
    }

    // Stop acquisition: signal the thread and close the socket
    acquisitionActive = false;
    if (tcp_sockfd != -1)
    {
        close(tcp_sockfd); // This will cause read() to return and the thread to exit
        tcp_sockfd = -1;
    }
    
    // Clean up progress monitoring
    if (progressMonitoringActive) {
        cleanupProgressMonitoring();
    }
    
    if (decoder)
    {
        decoder->stop();
        decoder->close();
        delete decoder;
        decoder = nullptr;
    }
    connectionStatus = Status::Connected;

    lastUpdate = QDateTime::currentDateTime();
    lastError = "";
    return QString(); // No error
}

QString Board::setCurrentPONValues()
{
    // Refresh the PON values from the board
    return setPONValues(powerOn, polarity, triggerType, gain, shaping, numSamples, triggerThreshold, pretrigger, externalClkEnable);
}

QString Board::setPONValues(bool powerOn, bool polarity, TriggerType triggerType, Gain gain, Shaping shaping, NumSamples numSamples, uint16_t triggerThreshold, uint8_t pretrigger, bool externalClkEnable)
{
    if (!connection)
    {
        lastError = "No connection object";
        connectionStatus = Status::Error;
        return lastError;
    }
    uint32_t ponValue = PONConverter::toPON_reg(powerOn, polarity, gain, shaping, triggerType, numSamples, triggerThreshold, pretrigger, externalClkEnable);
    printf("Setting PON register to 0x%08X (binary: 0b", ponValue);
    for (int i = 31; i >= 0; --i) {
        printf("%d", (ponValue >> i) & 1);
    }
    printf(")\n");
    QString error;
    if (connection->writeRegister(0x800, ponValue, &error))
    {
        this->powerOn = powerOn;
        this->polarity = polarity;
        this->triggerType = triggerType;
        this->gain = gain;
        this->shaping = shaping;
        this->numSamples = numSamples;
        this->triggerThreshold = triggerThreshold;
        this->externalClkEnable = externalClkEnable;
        lastUpdate = QDateTime::currentDateTime();
        lastError = "";
        return QString(); // No error
    }
    else
    {
        lastError = error;
        connectionStatus = Status::Error;
        return error;
    }
}

QString Board::refreshPONValues()
{
    if (connection)
    {
        lastUpdate = QDateTime::currentDateTime();

        // Read the PON register (address 0x800) to update powerOn
        uint32_t ponValue = 0;
        QString error;
        if (connection->readRegister(0x800, &ponValue, &error))
        {
            printf("PON register value: 0x%08X\n", ponValue);
            powerOn = PONConverter::fromPON_regToPowerOn(ponValue);
            printf("powerOn: %d\n", powerOn);
            polarity = PONConverter::fromPON_regToPolarity(ponValue);
            printf("polarity: %d\n", polarity);
            triggerType = PONConverter::fromPON_regToTriggerType(ponValue);
            printf("triggerType: %d\n", static_cast<int>(triggerType));
            gain = PONConverter::fromPON_regToGain(ponValue);
            printf("gain: %d\n", static_cast<int>(gain));
            shaping = PONConverter::fromPON_regToShaping(ponValue);
            printf("shaping: %d\n", static_cast<int>(shaping));
            numSamples = PONConverter::fromPON_regToNumSamples(ponValue);
            printf("numSamples: %d\n", static_cast<int>(numSamples));
            triggerThreshold = PONConverter::fromPON_regToTriggerThreshold(ponValue);
            printf("triggerThreshold: %d\n", triggerThreshold);
            pretrigger = PONConverter::fromPON_regToPretrigger(ponValue);
            printf("pretrigger: %d\n", pretrigger);
            externalClkEnable = PONConverter::fromPON_regToExternalClkEnable(ponValue);
            printf("externalClkEnable: %d\n", externalClkEnable);
        }
        else
        {
            lastError = "Failed to read PON register: " + error;
            connectionStatus = Status::Error;
            return lastError;
        }

        // Optionally, refresh other board model values here

        lastError = "";
        return QString(); // No error
    }
    lastError = "No connection object";
    connectionStatus = Status::Error;
    return lastError;
}

QString Board::getStatusString() const
{
    switch (connectionStatus)
    {
    case Status::Disconnected:
        return "disconnected";
    case Status::Connecting:
        return "connecting";
    case Status::Connected:
        return "connected";
    case Status::Error:
        return "error";
    default:
        return "unknown";
    }
}