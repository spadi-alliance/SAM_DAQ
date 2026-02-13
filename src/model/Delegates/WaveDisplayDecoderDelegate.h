#pragma once
#include "../SAMDecoder/SAMDecoder.h"
#include <TGraph.h>
#include <TCanvas.h>
#include <QDebug>
#include <chrono>  // Add for time-based updates

class WaveDisplayDecoderDelegate : public SAMDecoderDelegate
{
public:
    ~WaveDisplayDecoderDelegate() override
    {
        for (auto &canvas : display_canvases)
        {
            if (canvas)
                delete canvas;
        }
        for (auto &graph : graphs)
        {
            if (graph)
                delete graph;
        }
    }

    std::vector<TCanvas *> display_canvases;
    std::vector<TGraph *> graphs;

    int counter = 0;
    // Changed from event-based to time-based
    std::chrono::milliseconds update_interval{100}; // Update every 100ms (1 second)
    std::chrono::steady_clock::time_point last_update_time{std::chrono::steady_clock::now()};
    
    void onBlockDecoded(const DecodedBlock& block) override;
    void createWaveformCanvasAndGraph(int chip, int channel);
    void openAllCanvases();
    void setUpdateInterval(std::chrono::milliseconds interval) { update_interval = interval; }

private:
    TCanvas *waveform_canvas;
    TGraph *waveform_graph;
    int current_wave_channel = 0;
    int current_wave_chip = 0;
    
    // For collecting samples across blocks
    std::vector<uint16_t> collected_samples;  // Collects samples for the current waveform
    u_int8_t last_sample_number = 255;        // Track last sample number to detect rollback

    TCanvas *findCanvasByName(const std::string &name);
    TGraph *findGraphByName(const std::string &name);
    TGraph *findGraphInCanvas(TCanvas *canvas, const std::string &name);
    void updateWaveformDisplay();  // Helper method to update the waveform graph
    std::string makeCanvasName(int chip, int channel) {
        return QString("Waveform_%1_%2").arg(chip).arg(channel).toStdString();
    }
    std::string makeGraphName(int chip, int channel) {
        return QString("WaveformGraph_%1_%2").arg(chip).arg(channel).toStdString();
    }
    std::string makeCanvasTitle(int chip, int channel) {
        return QString("Waveform Display for Chip %1, Channel %2").arg(chip).arg(channel).toStdString();
    }
    std::string makeGraphTitle(int chip, int channel) {
        return QString("Waveform for Chip %1, Channel %2").arg(chip).arg(channel).toStdString();
    }
};