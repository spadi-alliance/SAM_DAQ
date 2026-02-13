#include "RootTHttpServerDecoderDelegate.h"
#include <QDebug>

RootTHttpServerDecoderDelegate::RootTHttpServerDecoderDelegate(TString server_ip)
    : server_ip(server_ip)
{
    // Initialize the THttpServer
    httpServer = new THttpServer(server_ip);
}

void RootTHttpServerDecoderDelegate::acquisitionWillStop() {
    if (httpServer) {
        qDebug() << "Stopping THttpServer...";
        delete httpServer;
        httpServer = nullptr;
    }
}