#include "MainWindow.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QDateTime>
#include "helpers/Constants.h"
#include "model/Delegates/WaveDisplayDecoderDelegate.h"
#include "model/Delegates/RootTHttpServerDecoderDelegate.h"
#include "model/SAMDecoder/SAMDecoder.h"


#define K_DEFAULT_OUT_DIR QDir::currentPath()

MainWindow::MainWindow(bool multithreaded, QWidget *parent)
    : QMainWindow(parent)
{

    isMultithreaded = multithreaded; // Set multithreading flag

    fBoard = new Board(K_DEFAULT_IP, K_DEFAULT_OUT_DIR);
    fBoard->continuousMonitoring = true; // Enable continuous monitoring by default

    setWindowTitle("🐳 SAMDAQ");
    resize(800, 600);

    tabWidget = new QTabWidget(this);

    basicTab = createBasicTab();
    advancedTab = createAdvancedTab();
    onlineTab = createOnlineTab();
    offlineTab = createOfflineTab();
    tabWidget->addTab(basicTab, "Basic");
    tabWidget->addTab(advancedTab, "Advanced");
    tabWidget->addTab(onlineTab, "Online");
    tabWidget->addTab(offlineTab, "Offline");

    resultTextArea = new QTextEdit(this);
    resultTextArea->setReadOnly(true);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(resultTextArea);

    setCentralWidget(centralWidget);
    resize(800, 600);
}

void MainWindow::appendResult(const QString& text) {
    if (text.trimmed().isEmpty()) return;
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString newEntry = QString("%1 - %2").arg(timestamp, text);
    QString currentText = resultTextArea->toPlainText();
    if (!currentText.isEmpty()) {
        resultTextArea->setPlainText(newEntry + "\n" + currentText);
    } else {
        resultTextArea->setPlainText(newEntry);
    }
}

QWidget* MainWindow::createBasicTab() {
    QWidget *tab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Row 1: Last Update
    QHBoxLayout *row1 = new QHBoxLayout;
    lastUpdateLabel = new QLabel("Last Update:");
    lastUpdateValue = new QLabel("unknown");
    refreshButton = new QPushButton("Refresh");
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshStatus);
    row1->addWidget(lastUpdateLabel);
    row1->addWidget(lastUpdateValue);
    row1->addWidget(refreshButton);
    layout->addLayout(row1);

    // Row 2: Power On/Off (toggle)
    QHBoxLayout *row2 = new QHBoxLayout;
    powerOnLabel = new QLabel("Power:");
    powerOnValue = new QLabel("unknown");
    powerOnButton = new QPushButton("Power On");
    powerOnButton->setCheckable(true);
    connect(powerOnButton, &QPushButton::clicked, this, &MainWindow::onTogglePower);
    row2->addWidget(powerOnLabel);
    row2->addWidget(powerOnValue);
    row2->addWidget(powerOnButton);
    layout->addLayout(row2);

    // Row 3: Trigger Type
    QHBoxLayout *row3 = new QHBoxLayout;
    triggerTypeLabel = new QLabel("Trigger Type:");
    triggerTypeValue = new QLabel("unknown");
    triggerTypeCombo = new QComboBox;
    triggerTypeCombo->addItem("External", 0);
    triggerTypeCombo->addItem("Self-trigger", 1);
    triggerTypeCombo->addItem("1 kHz", 2);
    triggerTypeCombo->addItem("1 MHz", 3);
    triggerTypeCombo->setCurrentIndex(0);
    connect(triggerTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTriggerTypeChanged);
    row3->addWidget(triggerTypeLabel);
    row3->addWidget(triggerTypeValue);
    row3->addWidget(triggerTypeCombo);
    layout->addLayout(row3);

    // Row: Trigger Threshold
    QHBoxLayout *rowThreshold = new QHBoxLayout;
    triggerThresholdLabel = new QLabel("Trigger Threshold:");
    triggerThresholdSpin = new QSpinBox;
    triggerThresholdSpin->setRange(0, 1023);
    triggerThresholdSpin->setValue(0);
    connect(triggerThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTriggerThresholdChanged);
    rowThreshold->addWidget(triggerThresholdLabel);
    rowThreshold->addWidget(triggerThresholdSpin);
    layout->addLayout(rowThreshold);

    // Row 4: Connection (toggle)
    QHBoxLayout *row4 = new QHBoxLayout;
    connectionLabel = new QLabel("Board IP:");
    connectionValue = new QLabel("unknown");
    connectionButton = new QPushButton("Connect");
    connectionButton->setCheckable(true);
    connect(connectionButton, &QPushButton::clicked, this, &MainWindow::onConnect);
    row4->addWidget(connectionLabel);
    row4->addWidget(connectionValue);
    row4->addWidget(connectionButton);
    layout->addLayout(row4);

    // Row: Output Directory (with browse button, label only)
    QHBoxLayout *rowOutDir = new QHBoxLayout;
    outputDirLabel = new QLabel("Output Directory:");
    outputDirValue = new QLabel(QDir::currentPath());
    outputDirBrowseButton = new QPushButton("Browse...");
    connect(outputDirBrowseButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    rowOutDir->addWidget(outputDirLabel);
    rowOutDir->addWidget(outputDirValue);
    rowOutDir->addWidget(outputDirBrowseButton);
    layout->addLayout(rowOutDir);

    // Row: Output File Name (editable)
    QHBoxLayout *rowOutFName = new QHBoxLayout;
    outputFNameLabel = new QLabel("Output File Name:");
    outputFNameEntry = new QLineEdit;
    outputFNameEntry->setText(K_DEFAULT_OUT_FNAME);
    rowOutFName->addWidget(outputFNameLabel);
    rowOutFName->addWidget(outputFNameEntry);
    layout->addLayout(rowOutFName);

    // Row 5: Start Acquisition (toggle)
    QHBoxLayout *row5 = new QHBoxLayout;
    toggleAcquisitionButton = new QPushButton("Start Acquisition");
    toggleAcquisitionButton->setCheckable(true);
    connect(toggleAcquisitionButton, &QPushButton::clicked, this, &MainWindow::onStartAcquisition);
    row5->addWidget(toggleAcquisitionButton);
    layout->addLayout(row5);

    // Row: Polarity radio buttons
    QHBoxLayout *rowPolarity = new QHBoxLayout;
    polarityLabel = new QLabel("Polarity:");
    polarityPositiveRadio = new QRadioButton("Positive");
    polarityNegativeRadio = new QRadioButton("Negative");
    polarityGroup = new QButtonGroup(tab);
    polarityGroup->addButton(polarityPositiveRadio, 0);
    polarityGroup->addButton(polarityNegativeRadio, 1);
    polarityPositiveRadio->setChecked(true); // Default
    rowPolarity->addWidget(polarityLabel);
    rowPolarity->addWidget(polarityPositiveRadio);
    rowPolarity->addWidget(polarityNegativeRadio);
    layout->addLayout(rowPolarity);

    connect(polarityGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &MainWindow::onPolarityChanged);

    // Row: Gain/Shaping radio buttons
    QHBoxLayout *rowGainShaping = new QHBoxLayout;
    gainShapingLabel = new QLabel("Gain/Shaping:");
    gainShaping1Radio = new QRadioButton("30 mV/fC, 160 ns");
    gainShaping2Radio = new QRadioButton("20 mV/fC, 160 ns");
    gainShaping3Radio = new QRadioButton("4 mV/fC, 300 ns");
    gainShapingGroup = new QButtonGroup(tab);
    gainShapingGroup->addButton(gainShaping1Radio, 0);
    gainShapingGroup->addButton(gainShaping2Radio, 1);
    gainShapingGroup->addButton(gainShaping3Radio, 2);
    gainShaping1Radio->setChecked(true); // Default
    rowGainShaping->addWidget(gainShapingLabel);
    rowGainShaping->addWidget(gainShaping1Radio);
    rowGainShaping->addWidget(gainShaping2Radio);
    rowGainShaping->addWidget(gainShaping3Radio);
    layout->addLayout(rowGainShaping);

    connect(gainShapingGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &MainWindow::onGainShapingChanged);

    // Row: Number of Samples radio buttons
    QHBoxLayout *rowNumSamples = new QHBoxLayout;
    numSamplesLabel = new QLabel("Number of Samples:");
    numSamples16Radio = new QRadioButton("16");
    numSamples32Radio = new QRadioButton("32");
    numSamples64Radio = new QRadioButton("64");
    numSamples128Radio = new QRadioButton("128");
    numSamplesGroup = new QButtonGroup(tab);
    numSamplesGroup->addButton(numSamples16Radio, 0);
    numSamplesGroup->addButton(numSamples32Radio, 1);
    numSamplesGroup->addButton(numSamples64Radio, 2);
    numSamplesGroup->addButton(numSamples128Radio, 3);
    numSamples16Radio->setChecked(true); // Default
    rowNumSamples->addWidget(numSamplesLabel);
    rowNumSamples->addWidget(numSamples16Radio);
    rowNumSamples->addWidget(numSamples32Radio);
    rowNumSamples->addWidget(numSamples64Radio);
    rowNumSamples->addWidget(numSamples128Radio);
    layout->addLayout(rowNumSamples);

    connect(numSamplesGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &MainWindow::onNumSamplesChanged);

    // Row: Pretrigger Selection
    QHBoxLayout *rowPretrigger = new QHBoxLayout;
    pretriggerLabel = new QLabel("Pretrigger:");
    pretriggerCombo = new QComboBox;
    pretriggerCombo->addItem("0 samples", 0);
    pretriggerCombo->addItem("4 samples", 1);
    pretriggerCombo->addItem("8 samples", 2);
    pretriggerCombo->addItem("16 samples", 3);
    pretriggerCombo->setCurrentIndex(0);
    connect(pretriggerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPretriggerChanged);
    rowPretrigger->addWidget(pretriggerLabel);
    rowPretrigger->addWidget(pretriggerCombo);
    layout->addLayout(rowPretrigger);

    // Row: External Clock Enable
    QHBoxLayout *rowExternalClk = new QHBoxLayout;
    externalClkCheckBox = new QCheckBox("External Clock Enable");
    externalClkCheckBox->setChecked(false);
    connect(externalClkCheckBox, &QCheckBox::toggled, this, &MainWindow::onExternalClkChanged);
    rowExternalClk->addWidget(externalClkCheckBox);
    rowExternalClk->addStretch();
    layout->addLayout(rowExternalClk);

    layout->addStretch();
    return tab;
}

QWidget* MainWindow::createAdvancedTab() {
    QWidget *tab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Row 1: Ping
    QHBoxLayout *row1 = new QHBoxLayout;
    pingLabel = new QLabel("Ping:");
    ipEntry = new QLineEdit;
    ipEntry->setText(K_DEFAULT_IP);
    pingButton = new QPushButton("Ping");
    connect(pingButton, &QPushButton::clicked, this, &MainWindow::onPing);
    // Handle value change
    connect(ipEntry, &QLineEdit::textChanged, this, &MainWindow::onIpEntryChanged);
    row1->addWidget(pingLabel);
    row1->addWidget(ipEntry);
    row1->addWidget(pingButton);
    layout->addLayout(row1);

    // Row 3: Read Operation
    QHBoxLayout *row3 = new QHBoxLayout;
    readAddressLabel = new QLabel("Read Address: 0x");
    readAddressEntry = new QLineEdit;
    readButton = new QPushButton("Read");
    connect(readButton, &QPushButton::clicked, this, &MainWindow::onRead);
    row3->addWidget(readAddressLabel);
    row3->addWidget(readAddressEntry);
    row3->addWidget(readButton);
    layout->addLayout(row3);

    // Row 4: Write Operation
    QHBoxLayout *row4 = new QHBoxLayout;
    writeAddressLabel = new QLabel("Write Address: 0x");
    writeAddressEntry = new QLineEdit;
    writeValueEntry = new QLineEdit;
    writeButton = new QPushButton("Write");
    connect(writeButton, &QPushButton::clicked, this, &MainWindow::onWrite);
    row4->addWidget(writeAddressLabel);
    row4->addWidget(writeAddressEntry);
    row4->addWidget(writeValueEntry);
    row4->addWidget(writeButton);
    layout->addLayout(row4);

    layout->addStretch();
    return tab;
}

QWidget* MainWindow::createOnlineTab() {
    QWidget *tab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Delegate status
    delegateStatusLabel = new QLabel("Delegate status: Disabled");
    layout->addWidget(delegateStatusLabel);

    // Enable/Disable buttons
    delegateEnableButton = new QPushButton("Enable Online Waveform");
    delegateDisableButton = new QPushButton("Disable Online Waveform");
    layout->addWidget(delegateEnableButton);
    layout->addWidget(delegateDisableButton);

    // Add Chip Selection Dropdown
    QHBoxLayout *chipLayout = new QHBoxLayout;
    chipSelectionLabel = new QLabel("Select Chip:");
    chipSelectionCombo = new QComboBox;
    
    // Populate chip dropdown (assuming chips 0-3, adjust as needed)
    for (int i = 0; i < 4; ++i) {
        chipSelectionCombo->addItem(QString("Chip %1").arg(i), i);
    }
    chipSelectionCombo->setCurrentIndex(0); // Default to chip 0
    
    chipLayout->addWidget(chipSelectionLabel);
    chipLayout->addWidget(chipSelectionCombo);
    chipLayout->addStretch(); // Push items to the left
    layout->addLayout(chipLayout);

    // Add Channel Selection Dropdown
    QHBoxLayout *channelLayout = new QHBoxLayout;
    channelSelectionLabel = new QLabel("Select Channel:");
    channelSelectionCombo = new QComboBox;
    
    // Populate channel dropdown (32 channels: 0-31)
    for (int i = 0; i < 32; ++i) {
        channelSelectionCombo->addItem(QString("Channel %1").arg(i), i);
    }
    channelSelectionCombo->setCurrentIndex(0); // Default to channel 0
    
    channelLayout->addWidget(channelSelectionLabel);
    channelLayout->addWidget(channelSelectionCombo);
    channelLayout->addStretch(); // Push items to the left
    layout->addLayout(channelLayout);

    // Add Update Interval Control
    QHBoxLayout *intervalLayout = new QHBoxLayout;
    updateIntervalLabel = new QLabel("Update Interval (ms):");
    updateIntervalSpinBox = new QSpinBox;
    updateIntervalSpinBox->setRange(50, 2000); // 50ms to 2000ms range
    updateIntervalSpinBox->setValue(100); // Default to 100ms
    updateIntervalSpinBox->setSingleStep(50); // Step by 50ms
    
    intervalLayout->addWidget(updateIntervalLabel);
    intervalLayout->addWidget(updateIntervalSpinBox);
    intervalLayout->addStretch(); // Push items to the left
    layout->addLayout(intervalLayout);

    // Connect spin box signal
    connect(updateIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onUpdateIntervalChanged);

    // Connect dropdown change signals
    connect(chipSelectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onChipSelectionChanged);
    connect(channelSelectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onChannelSelectionChanged);

    // Connect enable/disable buttons
    connect(delegateEnableButton, &QPushButton::clicked, this, [this]() {
        if (fBoard && fBoard->decoder) {
            SAMDecoder *samDecoder = fBoard->decoder;
            samDecoder->removeDelegateOfType(typeid(WaveDisplayDecoderDelegate));
            onlineWaveDelegate = new WaveDisplayDecoderDelegate();
            
            // Set the update interval from the spin box
            int intervalMs = updateIntervalSpinBox->value();
            onlineWaveDelegate->setUpdateInterval(std::chrono::milliseconds(intervalMs));
            
            fBoard->decoder->addDelegate(onlineWaveDelegate);
            
            // Use selected chip and channel from dropdowns
            int selectedChip = chipSelectionCombo->currentData().toInt();
            int selectedChannel = channelSelectionCombo->currentData().toInt();
            onlineWaveDelegate->createWaveformCanvasAndGraph(selectedChip, selectedChannel);
            
            // Enable dropdown controls
            chipSelectionCombo->setEnabled(true);
            channelSelectionCombo->setEnabled(true);
            
            delegateStatusLabel->setText("Delegate status: Enabled");
            appendResult(QString("Online waveform enabled for Chip %1, Channel %2 with %3ms update interval")
                        .arg(selectedChip).arg(selectedChannel).arg(intervalMs));
        }
    });
    
    connect(delegateDisableButton, &QPushButton::clicked, this, [this]() {
        if (fBoard && fBoard->decoder && onlineWaveDelegate) {
            fBoard->decoder->removeDelegate(onlineWaveDelegate);
            onlineWaveDelegate = nullptr;
            
            // Disable dropdown controls
            chipSelectionCombo->setEnabled(false);
            channelSelectionCombo->setEnabled(false);
            
            delegateStatusLabel->setText("Delegate status: Disabled");
            appendResult("Online waveform disabled.");
        }
    });

    // Initially disable dropdowns until delegate is enabled
    chipSelectionCombo->setEnabled(false);
    channelSelectionCombo->setEnabled(false);

    layout->addStretch();
    return tab;
}

QWidget* MainWindow::createOfflineTab() {
    QWidget *tab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Row: Input bin file selection
    QHBoxLayout *rowInputBin = new QHBoxLayout;
    inputBinFileLabel = new QLabel("Input Bin File:");
    inputBinFileValue = new QLabel("No file selected");
    inputBinFileBrowseButton = new QPushButton("Browse...");
    connect(inputBinFileBrowseButton, &QPushButton::clicked, this, &MainWindow::onBrowseInputBinFile);
    rowInputBin->addWidget(inputBinFileLabel);
    rowInputBin->addWidget(inputBinFileValue);
    rowInputBin->addWidget(inputBinFileBrowseButton);
    layout->addLayout(rowInputBin);

    // Row: Output ROOT file name
    QHBoxLayout *rowOutputRoot = new QHBoxLayout;
    outputRootFileLabel = new QLabel("Output ROOT File:");
    outputRootFileEntry = new QLineEdit;
    outputRootFileEntry->setText("output/offline_output.root");
    rowOutputRoot->addWidget(outputRootFileLabel);
    rowOutputRoot->addWidget(outputRootFileEntry);
    layout->addLayout(rowOutputRoot);

    // Row: Process button
    QHBoxLayout *rowProcess = new QHBoxLayout;
    processOfflineButton = new QPushButton("Process Bin File to ROOT");
    processOfflineButton->setEnabled(false); // Disabled until file is selected
    connect(processOfflineButton, &QPushButton::clicked, this, &MainWindow::onProcessOffline);
    rowProcess->addWidget(processOfflineButton);
    layout->addLayout(rowProcess);

    layout->addStretch();
    return tab;
}

void MainWindow::displayValuesFromBoard(Board *board) {
    if (!board) {
        appendResult("Error: Board object is null.");
        return;
    }
    lastUpdateValue->setText(board->lastUpdate.toString("yyyy-MM-dd HH:mm:ss"));
    powerOnValue->setText(board->powerOn ? "On" : "Off");
    powerOnButton->setChecked(board->powerOn);
    powerOnButton->setText(board->powerOn ? "Power Off" : "Power On");
    connectionValue->setText(board->isConnected ? "Connected" : "Disconnected");
    triggerTypeValue->setText(TriggerTypeStrings::toString(board->triggerType));
    triggerTypeCombo->setCurrentIndex(static_cast<int>(board->triggerType));

    // Update polarity radio buttons
    polarityPositiveRadio->setChecked(board->polarity);
    polarityNegativeRadio->setChecked(!board->polarity);

    // Update gain/shaping radio buttons
    if (board->gain == Gain::G_30_mV_fC && board->shaping == Shaping::SH_160_ns)
        gainShaping1Radio->setChecked(true);
    else if (board->gain == Gain::G_20_mV_fC && board->shaping == Shaping::SH_160_ns)
        gainShaping2Radio->setChecked(true);
    else if (board->gain == Gain::G_4_mV_fC && board->shaping == Shaping::SH_300_ns)
        gainShaping3Radio->setChecked(true);

    // Update number of samples radio buttons
    switch (board->numSamples) {
        case NumSamples::N_16: numSamples16Radio->setChecked(true); break;
        case NumSamples::N_32: numSamples32Radio->setChecked(true); break;
        case NumSamples::N_64: numSamples64Radio->setChecked(true); break;
        case NumSamples::N_128: numSamples128Radio->setChecked(true); break;
    }

    // Update pretrigger combo box
    pretriggerCombo->setCurrentIndex(board->pretrigger);

    // Update external clock checkbox
    externalClkCheckBox->setChecked(board->externalClkEnable);

    appendResult("Values updated from board.");
}

// Implement your slots here (show message boxes, update resultTextArea, etc.)
void MainWindow::onRefreshStatus() { 
    ENSURE_BOARD
    QString error = fBoard->refreshPONValues();
    if (error.isEmpty()) {
        displayValuesFromBoard(fBoard);
        appendResult("Status refreshed successfully.");
    } else {
        appendResult("Refresh Error: " + error);
    }
}

void MainWindow::onTogglePower() { 
    if (!fBoard) {
        appendResult("Error: Board object is not initialized.");
        powerOnButton->setChecked(false);
        return;
    }
    bool turnOn = powerOnButton->isChecked();
    QString error = fBoard->powerOnOff(turnOn);
    if (error.isEmpty()) {
        powerOnButton->setText(turnOn ? "Power Off" : "Power On");
        powerOnValue->setText(turnOn ? "On" : "Off");
        appendResult(QString("Power %1 successfully.").arg(turnOn ? "on" : "off"));
    } else {
        appendResult("Power Error: " + error);
        powerOnButton->setChecked(!turnOn); // revert state on error
    }
}

void MainWindow::onTriggerTypeChanged(int index) { 
    ENSURE_BOARD
    // Map combo index to trigger bits
    // Self-trigger/External: original PON bit (assume bit 0)
    // 1 kHz: bit 8
    // 1 MHz: bit 9
    TriggerType newType;
    switch (index) {
        case 0: // Self-trigger
            newType = TriggerType::External;
            break;
        case 1: // External
            newType = TriggerType::SelfTrigger;
            break;
        case 2: // 1 kHz
            newType = TriggerType::KHz1;
            break;
        case 3: // 1 MHz
            newType = TriggerType::MHz1;
            break;
        default:
            newType = TriggerType::SelfTrigger;
    }
    QString error = fBoard->setTriggerType(newType);
    if (error.isEmpty()) {
        fBoard->triggerType = newType;
        triggerTypeCombo->setCurrentIndex(index);
        triggerTypeValue->setText(triggerTypeCombo->currentText());
        appendResult(QString("Trigger type changed to %1.").arg(triggerTypeCombo->currentText()));
    } else {
        appendResult("Trigger Type Change Error: " + error);
    }
}

void MainWindow::onConnect() { 
    if (!fBoard) {
        appendResult("Error: Board object is not initialized.");
        connectionButton->setChecked(false);
        return;
    }
    bool doConnect = connectionButton->isChecked();
    QString error;
    if (doConnect) {
        error = fBoard->connect();
        if (error.isEmpty()) {
            connectionButton->setText("Disconnect");
            connectionValue->setText("Connected");
            appendResult("Connected successfully.");
            // send current configuration to the board
            fBoard->setCurrentPONValues();
            onRefreshStatus(); // Refresh status after connection
        } else {
            appendResult("Connection Error: " + error);
            connectionButton->setChecked(false);
        }
    } else {
        // Implement disconnect logic if available
        // For now, just update UI
        connectionButton->setText("Connect");
        connectionValue->setText("Disconnected");
        appendResult("Disconnected.");
        // Optionally: fBoard->disconnect();
    }
}

void MainWindow::onPing() { 
    ENSURE_BOARD
    QString error = fBoard->ping();
    appendResult(error.isEmpty() ? "Ping successful." : "Ping Error: " + error);
}

void MainWindow::onRead() {
    ENSURE_BOARD
    QString addressStr = readAddressEntry->text().trimmed();
    if (addressStr.isEmpty()) {
        appendResult("Input Error: Please enter an address to read.");
        return;
    }
    if (!addressStr.startsWith("0x", Qt::CaseInsensitive) && !addressStr.startsWith("0X", Qt::CaseInsensitive)) {
        addressStr = "0x" + addressStr;
    }
    bool ok = false;
    uint32_t address = addressStr.toUInt(&ok, 0); // auto-detect base (hex/dec)
    if (!ok) {
        appendResult("Input Error: Invalid address format.");
        return;
    }

    uint32_t result = 0;
    QString error = fBoard->readRegister(address, &result);
    if (!error.isEmpty()) {
        appendResult("Read Error: " + error);
        return;
    }
    QString resultStr = QString("Data read from address %1: 0x%2").arg(addressStr).arg(result, 0, 16);
    appendResult(resultStr);
}

void MainWindow::onWrite() { 
    ENSURE_BOARD
    QString addressStr = writeAddressEntry->text().trimmed();
    QString valueStr = writeValueEntry->text().trimmed();
    if (addressStr.isEmpty() || valueStr.isEmpty()) {
        appendResult("Input Error: Please enter both address and value to write.");
        return;
    }

    if (!addressStr.startsWith("0x", Qt::CaseInsensitive) && !addressStr.startsWith("0X", Qt::CaseInsensitive)) {
        addressStr = "0x" + addressStr;
    }
    bool okAddress = false;
    uint32_t address = addressStr.toUInt(&okAddress, 0); // auto-detect base (hex/dec)
    if (!okAddress) {
        appendResult("Input Error: Invalid address format.");
        return;
    }

    bool okValue = false;
    uint32_t value = valueStr.toUInt(&okValue, 0); // auto-detect base (hex/dec)
    if (!okValue) {
        appendResult("Input Error: Invalid value format.");
        return;
    }

    QString error = fBoard->writeRegister(address, value);
    if (!error.isEmpty()) {
        appendResult("Write Error: " + error);
        return;
    }
    QString resultStr = QString("Data written to address %1: 0x%2").arg(addressStr).arg(value, 0, 16);
    appendResult(resultStr);
}

void MainWindow::onIpEntryChanged(const QString& text) {
    ENSURE_BOARD
    if (!text.trimmed().isEmpty()) {
        fBoard->ipAddress = text.trimmed();
        appendResult("IP address changed to: " + fBoard->ipAddress);
    } else {
        appendResult("IP address cleared.");
    }
}

void MainWindow::onStartAcquisition() {
    if (!fBoard) {
        appendResult("Error: Board object is not initialized.");
        toggleAcquisitionButton->setChecked(false);
        return;
    }
    bool start = toggleAcquisitionButton->isChecked();
    QString error = "";
    if (start) {
        // get filename
        QString outFName = outputFNameEntry->text().trimmed();
        QString outDir = outputDirValue->text().trimmed();
        if (outFName.isEmpty()) outDir = K_DEFAULT_OUT_DIR; // use default if empty
        QString basename = outDir + "/" + outFName;
        SAMDecoder* decoder = new SAMDecoder(basename.toStdString(), isMultithreaded);
        decoder->addDelegate(new MakeTTreeDecoderDelegate(basename.toStdString()+".root"));
        decoder->addDelegate(new RootTHttpServerDecoderDelegate(TString("http:8081")));
        error = fBoard->startAcquisition(decoder);
    } else {
        error = fBoard->stopAcquisition();
    }
    if (error.isEmpty()) {
        toggleAcquisitionButton->setText(start ? "Stop Acquisition" : "Start Acquisition");
        appendResult(start ? "Acquisition started successfully." : "Acquisition stopped.");
    } else {
        appendResult("Acquisition Error: " + error);
        toggleAcquisitionButton->setChecked(!start); // revert state on error
    }
}

void MainWindow::onBrowseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", outputDirValue->text());
    if (!dir.isEmpty()) {
        outputDirValue->setText(dir);
    }
}

void MainWindow::onPolarityChanged() {
    ENSURE_BOARD
    bool polarity = polarityPositiveRadio->isChecked(); // true for positive, false for negative
    QString error = fBoard->setPolarity(polarity);
    if (error.isEmpty()) {
        displayValuesFromBoard(fBoard); // <-- update UI
        appendResult(QString("Polarity set to %1.").arg(polarity ? "Positive" : "Negative"));
    } else {
        appendResult("Polarity Change Error: " + error);
    }
}

void MainWindow::onGainShapingChanged() {
    ENSURE_BOARD
    Gain gain;
    Shaping shaping;
    switch (gainShapingGroup->checkedId()) {
        case 0:
            gain = Gain::G_30_mV_fC;
            shaping = Shaping::SH_160_ns;
            break;
        case 1:
            gain = Gain::G_20_mV_fC;
            shaping = Shaping::SH_160_ns;
            break;
        case 2:
            gain = Gain::G_4_mV_fC;
            shaping = Shaping::SH_300_ns;
            break;
        default:
            gain = Gain::G_30_mV_fC;
            shaping = Shaping::SH_160_ns;
    }
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, gain, shaping, fBoard->numSamples, fBoard->triggerThreshold, fBoard->pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        displayValuesFromBoard(fBoard); // <-- update UI
        appendResult(QString("Gain/Shaping set to %1, %2.")
            .arg(GainStrings::toString(gain))
            .arg(ShapingStrings::toString(shaping)));
    } else {
        appendResult("Gain/Shaping Change Error: " + error);
    }
}

void MainWindow::onNumSamplesChanged() {
    ENSURE_BOARD
    NumSamples numSamples;
    switch (numSamplesGroup->checkedId()) {
        case 0: numSamples = NumSamples::N_16; break;
        case 1: numSamples = NumSamples::N_32; break;
        case 2: numSamples = NumSamples::N_64; break;
        case 3: numSamples = NumSamples::N_128; break;
        default: numSamples = NumSamples::N_16;
    }
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, fBoard->gain, fBoard->shaping, numSamples, fBoard->triggerThreshold, fBoard->pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        displayValuesFromBoard(fBoard); // <-- update UI
        appendResult(QString("Number of samples set to %1.").arg(NumSamplesStrings::toString(numSamples)));
    } else {
        appendResult("NumSamples Change Error: " + error);
    }
}

void MainWindow::onChipSelectionChanged(int chipIndex) {
    if (onlineWaveDelegate && fBoard && fBoard->decoder) {
        int selectedChip = chipSelectionCombo->currentData().toInt();
        int selectedChannel = channelSelectionCombo->currentData().toInt();
        
        // Update the delegate with new chip/channel selection
        onlineWaveDelegate->createWaveformCanvasAndGraph(selectedChip, selectedChannel);
        
        appendResult(QString("Waveform display switched to Chip %1, Channel %2")
                    .arg(selectedChip).arg(selectedChannel));
    }
}

void MainWindow::onChannelSelectionChanged(int channelIndex) {
    if (onlineWaveDelegate && fBoard && fBoard->decoder) {
        int selectedChip = chipSelectionCombo->currentData().toInt();
        int selectedChannel = channelSelectionCombo->currentData().toInt();
        
        // Update the delegate with new chip/channel selection
        onlineWaveDelegate->createWaveformCanvasAndGraph(selectedChip, selectedChannel);
        
        appendResult(QString("Waveform display switched to Chip %1, Channel %2")
                    .arg(selectedChip).arg(selectedChannel));
    }
}

void MainWindow::onTriggerThresholdChanged(int value) {
    if (!fBoard) return;
    fBoard->triggerThreshold = static_cast<uint16_t>(value);
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, fBoard->gain, fBoard->shaping, fBoard->numSamples, fBoard->triggerThreshold, fBoard->pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        appendResult(QString("Trigger threshold set to %1.").arg(value));
    } else {
        appendResult("Trigger Threshold Change Error: " + error);
    }
}

void MainWindow::onPretriggerChanged(int index) {
    if (!fBoard) {
        appendResult("Error: Board object is not initialized.");
        return;
    }

    uint8_t pretrigger = static_cast<uint8_t>(index);
    QString error = fBoard->setPONValues(fBoard->powerOn, fBoard->polarity, fBoard->triggerType, fBoard->gain, fBoard->shaping, fBoard->numSamples, fBoard->triggerThreshold, pretrigger, fBoard->externalClkEnable);
    if (error.isEmpty()) {
        appendResult(QString("Pretrigger value set to %1 samples").arg(pretrigger == 0 ? 0 : (1 << (pretrigger + 1))));
    } else {
        appendResult("Failed to set pretrigger value: " + error);
    }
}

void MainWindow::onExternalClkChanged(bool checked) {
    if (!fBoard) {
        appendResult("Error: Board object is not initialized.");
        return;
    }

    QString error = fBoard->setExternalClkEnable(checked);
    if (error.isEmpty()) {
        appendResult(QString("External clock %1").arg(checked ? "enabled" : "disabled"));
    } else {
        appendResult("Failed to set external clock: " + error);
    }
}

void MainWindow::onBrowseInputBinFile() {
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Select Binary File", 
        QDir::currentPath(), 
        "Binary Files (*.bin);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        inputBinFileValue->setText(fileName);
        processOfflineButton->setEnabled(true);
        appendResult("Selected input file: " + fileName);
    }
}

void MainWindow::onProcessOffline() {
    QString inputFile = inputBinFileValue->text();
    QString outputFile = outputRootFileEntry->text();
    
    if (inputFile == "No file selected" || inputFile.isEmpty()) {
        appendResult("Error: No input file selected.");
        return;
    }
    
    if (outputFile.isEmpty()) {
        appendResult("Error: Output file name cannot be empty.");
        return;
    }
    
    // Disable button during processing
    processOfflineButton->setEnabled(false);
    processOfflineButton->setText("Processing...");
    
    try {
        // Create decoder for offline processing
        SAMDecoder* offlineDecoder = new SAMDecoder("output/offline_temp", isMultithreaded);
        
        // Create ROOT file delegate
        std::string outputFileStd = outputFile.toStdString();
        auto rootDelegate = new MakeTTreeDecoderDelegate(outputFileStd);
        offlineDecoder->addDelegate(rootDelegate);
        
        appendResult("Starting offline processing...");
        appendResult("Input: " + inputFile);
        appendResult("Output: " + outputFile);
        
        // Process the file
        offlineDecoder->decodeFromFile(inputFile.toStdString());
        
        // Stop the decoder properly
        offlineDecoder->stop();
        
        // Explicitly close the ROOT delegate to ensure data is written
        if (rootDelegate) {
            rootDelegate->close();
        }
        
        // Show malformed blocks statistics
        size_t malformed_count = offlineDecoder->getMalformedBlocksCount();
        if (malformed_count > 0) {
            appendResult("Warning: " + QString::number(malformed_count) + " malformed blocks were detected and skipped during processing.");
        }
        
        // Clean up
        offlineDecoder->close();
        delete offlineDecoder;
        
        appendResult("Offline processing completed successfully!");
        
    } catch (const std::exception& e) {
        appendResult("Error during offline processing: " + QString(e.what()));
    } catch (...) {
        appendResult("Unknown error during offline processing.");
    }
    
    // Re-enable button
    processOfflineButton->setEnabled(true);
    processOfflineButton->setText("Process Bin File to ROOT");
}

void MainWindow::onUpdateIntervalChanged(int value)
{
    if (onlineWaveDelegate) {
        onlineWaveDelegate->setUpdateInterval(std::chrono::milliseconds(value));
        appendResult(QString("Update interval changed to %1ms").arg(value));
    }
}

MainWindow::~MainWindow()
{
    delete fBoard; // Clean up the connection object
    delete fCanvas;     // Clean up the canvas if it was created
}