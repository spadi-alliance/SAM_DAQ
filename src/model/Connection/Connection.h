#pragma once

#include <QString>

// Forward declaration for C struct from fakernet
struct fnet_ctrl_client;

class Connection {
public:
    Connection();
    ~Connection();

    // Connect to the board using Fakernet
    bool connectToBoard(const QString& host, bool reliable = true, QString* error = nullptr);

    // Disconnect from the board
    void disconnect();

    // Check if connected
    bool isConnected() const;

    bool pingBoard(const QString &host, QString *error);

    bool readRegister(uint32_t reg_addr, uint32_t* value, QString* error = nullptr);
    bool writeRegister(uint32_t reg_addr, uint32_t value, QString* error = nullptr);
    
    // Get the underlying Fakernet client pointer
    fnet_ctrl_client* getFnetClient() const {
        return client;
    }
private:
    fnet_ctrl_client* client;
};