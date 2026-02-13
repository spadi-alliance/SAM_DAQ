#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>  // Add this include
#include <QSpinBox>   // Include QSpinBox for threshold adjustment
#include <QCheckBox>  // Include QCheckBox for delegate selection
#include <TApplication.h>
#include <TCanvas.h>
#include "model/Board/Board.h"
#include <QRadioButton>
#include <QButtonGroup>
#include "model/Delegates/WaveDisplayDecoderDelegate.h"
#include "model/Delegates/MakeTTreeDecoderDelegate.h"

#define ENSURE_BOARD \
    if (!fBoard) { appendResult("Error: Board object is not initialized."); return; }

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(bool multithreaded, QWidget *parent = nullptr);
    void appendResult(const QString &text);
    ~MainWindow();

    void showHistogram();
    void updateUI();

private:
    // Board connection
    Board *fBoard;
    WaveDisplayDecoderDelegate *onlineWaveDelegate = nullptr;

    bool isMultithreaded = true; // Flag for multithreading support

    // ROOT integration
    TApplication *fApp;
    TCanvas *fCanvas;

    // Tabs
    QTabWidget *tabWidget;
    QWidget *basicTab;
    QWidget *advancedTab;
    QWidget *onlineTab;
    QWidget *offlineTab;

    // --- Basic Tab Widgets ---
    // Last Update
    QLabel *lastUpdateLabel;
    QLabel *lastUpdateValue;
    QPushButton *refreshButton;
    // Power
    QLabel *powerOnLabel;
    QLabel *powerOnValue;
    QPushButton *powerOnButton;
    // Trigger
    QLabel *triggerTypeLabel;
    QLabel *triggerTypeValue;
    QComboBox *triggerTypeCombo;
    QLabel *triggerThresholdLabel;
    QSpinBox *triggerThresholdSpin; // Spin box for trigger threshold
    // Connection
    QLabel *connectionLabel;
    QLabel *connectionValue;
    QPushButton *connectionButton;
    // Acquisition
    QPushButton *toggleAcquisitionButton;

    // --- Advanced Tab Widgets ---
    // Ping
    QLabel *pingLabel;
    QLineEdit *ipEntry;
    QPushButton *pingButton;
    // Read
    QLabel *readAddressLabel;
    QLineEdit *readAddressEntry;
    QPushButton *readButton;
    // Write
    QLabel *writeAddressLabel;
    QLineEdit *writeAddressEntry;
    QLineEdit *writeValueEntry;
    QPushButton *writeButton;

    // Output file controls
    QLabel *outputDirLabel;
    QLabel *outputDirValue;         // Changed from QLineEdit to QLabel
    QPushButton *outputDirBrowseButton;
    QLabel *outputFNameLabel;
    QLineEdit *outputFNameEntry;

    // Result display
    QTextEdit *resultTextArea;

    // Polarity controls
    QLabel *polarityLabel;
    QRadioButton *polarityPositiveRadio;
    QRadioButton *polarityNegativeRadio;
    QButtonGroup *polarityGroup;

    // Gain/Shaping controls
    QLabel *gainShapingLabel;
    QRadioButton *gainShaping1Radio; // 30 mV/fC, 160 ns
    QRadioButton *gainShaping2Radio; // 20 mV/fC, 160 ns
    QRadioButton *gainShaping3Radio; // 4 mV/fC, 300 ns
    QButtonGroup *gainShapingGroup;

    // Number of Samples controls
    QLabel *numSamplesLabel;
    QRadioButton *numSamples16Radio;
    QRadioButton *numSamples32Radio;
    QRadioButton *numSamples64Radio;
    QRadioButton *numSamples128Radio;
    QButtonGroup *numSamplesGroup;

    // Delegate controls
    QLabel *delegateStatusLabel;
    QPushButton *delegateEnableButton;
    QPushButton *delegateDisableButton;
    
    // Add dropdown controls
    QLabel *chipSelectionLabel;
    QComboBox *chipSelectionCombo;
    QLabel *channelSelectionLabel;
    QComboBox *channelSelectionCombo;

    // Update interval controls
    QLabel *updateIntervalLabel;
    QSpinBox *updateIntervalSpinBox;

    // Pretrigger controls
    QLabel *pretriggerLabel;
    QComboBox *pretriggerCombo;
    QCheckBox *externalClkCheckBox;

    // Offline tab controls
    QLabel *inputBinFileLabel;
    QLabel *inputBinFileValue;
    QPushButton *inputBinFileBrowseButton;
    QLabel *outputRootFileLabel;
    QLineEdit *outputRootFileEntry;
    QLabel *delegateSelectionLabel;
    QCheckBox *rootDelegateCheckBox;
    QCheckBox *waveDelegateCheckBox;
    QPushButton *processOfflineButton;

    // Layout helpers
    QWidget* createBasicTab();
    QWidget* createAdvancedTab();
    QWidget* createOnlineTab();
    QWidget* createOfflineTab();

private slots:
    void onRefreshStatus();
    void onTogglePower();
    void onTriggerTypeChanged(int index);
    void onTriggerThresholdChanged(int value);
    void onConnect();
    void onPing();
    void onRead();
    void onWrite();
    void onIpEntryChanged(const QString& text);
    void onStartAcquisition();
    void displayValuesFromBoard(Board *board);
    void onBrowseOutputDir();
    void onPolarityChanged();
    void onGainShapingChanged();
    void onNumSamplesChanged();
    // Add new slots for dropdown changes
    void onChipSelectionChanged(int chipIndex);
    void onChannelSelectionChanged(int channelIndex);
    void onPretriggerChanged(int index); // Slot for pretrigger combo box
    void onExternalClkChanged(bool checked); // Slot for external clock checkbox
    void onUpdateIntervalChanged(int value); // Slot for update interval changes
    // Offline tab slots
    void onBrowseInputBinFile();
    void onProcessOffline();
};