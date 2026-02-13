#include "Connection.h"
#include <QString>
#include <QDebug>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
extern "C" {
#include "../../../lib/fakernet/client/fnetctrl.h"
}

Connection::Connection() : client(nullptr) {}

Connection::~Connection() {
    disconnect();
}

bool Connection::connectToBoard(const QString& host, bool reliable, QString* error) {
    disconnect();
    const char* errStr = nullptr;
    client = fnet_ctrl_connect(host.toStdString().c_str(), reliable ? 1 : 0, &errStr, nullptr);
    if (!client) {
        if (error) *error = errStr ? QString::fromLocal8Bit(errStr) : QString("Unknown error");
        return false;
    }
    return true;
}

void Connection::disconnect() {
    if (client) {
        fnet_ctrl_close(client);
        client = nullptr;
    }
}

bool Connection::isConnected() const {
    return client != nullptr;
}

bool Connection::pingBoard(const QString& host, QString* error) {
    #ifdef _WIN32
        QString cmd = QString("ping -n 1 -w 1000 %1").arg(host);
    #else
        QString cmd = QString("ping -c 1 -W 1 %1").arg(host);
    #endif

        int ret = system(cmd.toStdString().c_str());
        if (ret == 0) {
            return true;
        } else {
            if (error) *error = QString("Ping failed with code %1").arg(ret);
            return false;
        }
}

bool Connection::readRegister(uint32_t address, uint32_t* value, QString* error) {
    if (!client) {
        if (error) *error = "Not connected";
        return false;
    }

    fakernet_reg_acc_item *send = nullptr;
    fakernet_reg_acc_item *recv = nullptr;
    fnet_ctrl_get_send_recv_bufs(client, &send, &recv);

    send[0].addr = htonl(FAKERNET_REG_ACCESS_ADDR_READ | address);
    send[0].data = htonl(0);

    int ret = fnet_ctrl_send_recv_regacc(client, 1);

    if (ret != 1) {
        if (error) *error = QString::fromLocal8Bit(fnet_ctrl_last_error(client));
        return false;
    }

    *value = ntohl(recv[0].data);
    return true;
}

bool Connection::writeRegister(uint32_t address, uint32_t value, QString* error) {
    if (!client) {
        if (error) *error = "Not connected";
        return false;
    }

    fakernet_reg_acc_item *send = nullptr;
    fakernet_reg_acc_item *recv = nullptr;
    fnet_ctrl_get_send_recv_bufs(client, &send, &recv);

    send[0].addr = htonl(FAKERNET_REG_ACCESS_ADDR_WRITE | address);
    send[0].data = htonl(value);

    int ret = fnet_ctrl_send_recv_regacc(client, 1);

    if (ret != 1) {
        if (error) *error = QString::fromLocal8Bit(fnet_ctrl_last_error(client));
        return false;
    }

    return true;
}