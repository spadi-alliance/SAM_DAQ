#pragma once
#include "../SAMDecoder/SAMDecoder.h"
#include <THttpServer.h>

class RootTHttpServerDecoderDelegate : public SAMDecoderDelegate {
public:
    RootTHttpServerDecoderDelegate(TString server_ip);

    void acquisitionWillStop() override;

    THttpServer* httpServer = nullptr;
    TString server_ip;
};