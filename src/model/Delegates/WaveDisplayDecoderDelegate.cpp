#include "WaveDisplayDecoderDelegate.h"
#include <TCanvas.h>
#include <TAxis.h>
#include <chrono>

void WaveDisplayDecoderDelegate::onBlockDecoded(const DecodedBlock& block)
{
    // Only process blocks from the current chip that we're monitoring
    if (block.chip_number != current_wave_chip || block.status != DecodingStatus::SUCCESS) {
        return;
    }
    
    // Check if we have valid channel data and the channel index is within bounds
    
    if (current_wave_channel >= static_cast<int>(block.channels.size())) {
        return;
    }
    

    // Get the sample value for our current channel
    uint16_t channel_value = block.channels[current_wave_channel];
    
    // Detect sample number rollback (indicates start of new waveform)
    bool rollback_detected = (block.sample_number < last_sample_number);
    
    if (rollback_detected && !collected_samples.empty()) {
        // Check if enough time has passed since last update
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time);
        
        if (elapsed_time >= update_interval) {
            // Update the waveform graph with collected samples
            updateWaveformDisplay();
            last_update_time = current_time; // Reset timer
        }
        
        // Clear collected samples for new waveform
        collected_samples.clear();
    }
    
    // Collect the sample for the current waveform
    collected_samples.push_back(channel_value);
    last_sample_number = block.sample_number;
    
    // Mark samples as ready when we have a complete waveform (128 samples)
    if (collected_samples.size() >= 128) {
        // Keep only the latest 128 samples
        if (collected_samples.size() > 128) {
            collected_samples.erase(collected_samples.begin(), collected_samples.end() - 128);
        }
    }
    
    counter++; // Keep counter for debugging/statistics
}

void WaveDisplayDecoderDelegate::updateWaveformDisplay()
{
    if (collected_samples.empty() || !waveform_graph) {
        return;
    }
    
    // Update the graph with collected samples
    size_t num_samples = std::min(collected_samples.size(), size_t(128));
    for (size_t i = 0; i < num_samples; ++i) {
        waveform_graph->SetPoint(static_cast<int>(i), static_cast<double>(i), static_cast<double>(collected_samples[i]));
    }
    
    // Fill remaining points with 0 if we have fewer than 128 samples
    for (size_t i = num_samples; i < 128; ++i) {
        waveform_graph->SetPoint(static_cast<int>(i), static_cast<double>(i), 0.0);
    }
    
    // Update canvas
    if (waveform_canvas) {
        waveform_canvas->Modified(); // Mark canvas as modified
        waveform_canvas->Update();   // Update the canvas to reflect changes
    } else {
        qDebug() << "Waveform canvas is not initialized.";
    }
}

TCanvas* WaveDisplayDecoderDelegate::findCanvasByName(const std::string& name)
{
    for (auto& canvas : display_canvases) {
        if (canvas && canvas->GetName() == name.c_str()) {
            return canvas;
        }
    }
    return nullptr;
}

TGraph* WaveDisplayDecoderDelegate::findGraphByName(const std::string& name)
{
    for (auto& graph : graphs) {
        if (graph && std::string(graph->GetName()) == name) {
            return graph;
        }
    }
    return nullptr;
}

TGraph* WaveDisplayDecoderDelegate::findGraphInCanvas(TCanvas* canvas, const std::string& name)
{
    if (!canvas) return nullptr;
    for (auto& graph : graphs) {
        if (graph && std::string(graph->GetName()) == name) {
            return graph;
        }
    }
    return nullptr;
}

void WaveDisplayDecoderDelegate::createWaveformCanvasAndGraph(int chip, int channel)
{
    // if current_wave_chip and current_wave_channel are different, remove the old graph
    if (current_wave_chip != chip || current_wave_channel != channel) {
        if (waveform_graph) {
            auto it = std::find(graphs.begin(), graphs.end(), waveform_graph);
            if (it != graphs.end()) graphs.erase(it);
            waveform_graph->Delete();
            waveform_graph = nullptr;
        }
        if (waveform_canvas) {
            auto it = std::find(display_canvases.begin(), display_canvases.end(), waveform_canvas);
            if (it != display_canvases.end()) display_canvases.erase(it);
            waveform_canvas->Close();
            waveform_canvas = nullptr;
        }  
    }

    current_wave_chip = chip;
    current_wave_channel = channel;
    
    // Reset the timer when switching channels
    last_update_time = std::chrono::steady_clock::now();
    
    // Reset sample collection state
    collected_samples.clear();
    last_sample_number = 255;

    // Check if the canvas already exists
    TCanvas* existing_canvas = findCanvasByName(makeCanvasName(chip, channel));
    if (existing_canvas) {
        waveform_canvas = existing_canvas;
        TGraph* existing_graph = findGraphInCanvas(waveform_canvas, makeGraphName(chip, channel));
        if (existing_graph) {
            waveform_graph = existing_graph;
            return;
        } else {
            existing_graph = findGraphByName(makeGraphName(chip, channel));
            if (existing_graph) {
                waveform_graph = existing_graph;
                waveform_canvas->cd();
                waveform_graph->Draw("ALP");
                return;
            } else {
                qDebug() << "Graph not found in existing canvas, creating a new one.";
            }
        }
    } else {
        waveform_canvas = new TCanvas(makeCanvasName(chip, channel).c_str(),
                                      makeCanvasTitle(chip, channel).c_str(),
                                      800, 600);
        display_canvases.push_back(waveform_canvas);
    }

    waveform_graph = new TGraph(128);
    //initialize to 0
    for (int i = 0; i < 128; ++i) {
        waveform_graph->SetPoint(i, static_cast<double>(i), 0.0);
    }
    // initialize ranges
    waveform_graph->GetXaxis()->SetTitle("Sample");
    waveform_graph->GetYaxis()->SetTitle("ADC Value");
    waveform_graph->GetXaxis()->SetLimits(0, 127);
    waveform_graph->GetXaxis()->SetRangeUser(0, 127);
    waveform_graph->SetMinimum(0);
    waveform_graph->SetMaximum(1023);

    waveform_graph->SetName(makeGraphName(chip, channel).c_str());
    waveform_graph->SetTitle(makeGraphTitle(chip, channel).c_str());
    waveform_graph->SetMarkerStyle(20);
    waveform_graph->SetMarkerSize(1.0);
    waveform_graph->SetMarkerColor(kBlue);
    waveform_graph->SetLineColor(kBlue);

    graphs.push_back(waveform_graph);
    waveform_canvas->cd();
    waveform_graph->Draw("ALP");
}

void WaveDisplayDecoderDelegate::openAllCanvases()
{
    for (auto& canvas : display_canvases) {
        if (canvas) {
            canvas->Update();
            canvas->Draw();
        }
    }
}