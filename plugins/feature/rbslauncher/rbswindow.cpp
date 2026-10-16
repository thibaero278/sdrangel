///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Thibaud (RBS)                                              //
// Some code by Claude                                                           //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include <QtMath>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "channel/channelwebapiutils.h"
#include "device/deviceapi.h"
#include "device/deviceenumerator.h"
#include "device/deviceset.h"
#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "util/callsign.h"
#include "util/maidenhead.h"
#include "maincore.h"
#include "mainwindow.h"

#include "mcpdatafeed.h"
#include "rbsbandpopup.h"
#include "rbsdelegates.h"
#include "rbsfrequencydial.h"
#include "rbssection.h"
#include "rbsspectrumtee.h"
#include "rbswindow.h"

// RBS settings live apart from SDRangel's: MainSettings::save() removes every group but its own
static const char* const m_settingsApplication = "RBS";
static const char* const m_settingsGroup = "RBSLauncher";
static constexpr int m_sidebarWidth = 310;      //!< Fixed, as the SDR++ menu
static constexpr int m_decodesHeight = 250;     //!< At first in the all in one layout, then as the user drags it
// The waterfall is a texture one pixel per FFT bin wide: above 8192 it passes the 16384 limit of
// many graphics chips (Intel) and stays black
static constexpr int m_log2FFTSizeMax = 13;

// Sample rates offered for the SDRplay: device rate and decimation
struct SampleRate { int m_devSampleRate; int m_log2Decim; };
static const SampleRate m_sampleRates[] = {
    {2000000, 5}, {2000000, 4}, {2000000, 3}, {2000000, 2}, {2000000, 1}, {2000000, 0},
    {4000000, 0}, {6000000, 0}, {8000000, 0}, {10000000, 0}
};

static const char* const m_styleSheet = R"(
RBSWindow { background: #0b0d10; }
QToolBar#rbsTopBarToolbar { background: #111418; border: none; border-bottom: 1px solid #252a31; padding: 0; spacing: 0; }
QWidget#rbsTopBar { background: #111418; }
QToolButton#rbsSectionHeader { background: #181c22; border: none; border-bottom: 1px solid #252a31; padding: 5px 6px; color: #dfe4ea; text-align: left; }
QWidget#rbsSectionBody { background: #111418; border-bottom: 1px solid #252a31; }
QWidget#rbsPanel, QScrollArea, QScrollArea > QWidget > QWidget { background: #111418; border: none; }
QScrollArea#rbsSidebar { border-right: 1px solid #252a31; }
QWidget#rbsPanelHeader { background: #181c22; border-top: 1px solid #252a31; border-bottom: 1px solid #252a31; }
QTabWidget::pane { border: none; background: #0b0d10; }
QTabBar { background: #111418; }
QTabBar::tab { background: #111418; color: #7f8a98; padding: 7px 28px; border: none; border-right: 1px solid #252a31; border-bottom: 1px solid #252a31; min-width: 140px; }
QTabBar::tab:selected { color: #ffffff; background: #0b0d10; border-bottom: 2px solid #3d8bff; }
QTabBar::tab:hover { color: #dfe4ea; }
QTableView, QTreeWidget { background: #08090b; alternate-background-color: #0c0e11; gridline-color: #15191e; border: none; selection-background-color: #1b3a66; selection-color: #ffffff; }
QHeaderView::section { background: #111418; color: #7f8a98; border: none; border-bottom: 1px solid #252a31; padding: 3px 8px; }
QLineEdit, QSpinBox { background: #08090b; border: 1px solid #252a31; border-radius: 0; padding: 2px 6px; color: #dfe4ea; min-height: 18px; }
QComboBox { combobox-popup: 0; background: #22272e; border: 1px solid #2f353d; border-radius: 3px; padding: 3px 8px; color: #dfe4ea; min-height: 18px; }
QComboBox:hover, QComboBox:on { border-color: #3d8bff; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; border: none; width: 22px; }
QComboBox::down-arrow { image: url(:/rbslauncher/images/combo_arrow.png); width: 10px; height: 6px; }
QComboBox QAbstractItemView { background: #2a2f37; border: 1px solid #3a414b; outline: 0; padding: 2px 0; color: #dfe4ea; }
QComboBox QAbstractItemView::item { min-height: 26px; padding: 0 10px; }
QComboBox QAbstractItemView::item:hover { background: #343a44; }
QComboBox QAbstractItemView::item:selected { background: #3d8bff; color: #ffffff; }
QToolButton#rbsBandButton { border: none; background: transparent; padding: 0; text-align: left; }
QToolButton#rbsBandButton:hover { color: #3d8bff; }
QLabel#rbsNewCountry { color: #ef4444; }
QToolButton { border: 1px solid #252a31; background: #181c22; padding: 3px 10px; border-radius: 0; color: #dfe4ea; }
QToolButton:hover { border-color: #3d8bff; }
QToolButton:checked { background: #3d8bff; border-color: #3d8bff; color: #ffffff; }
QToolButton#rbsPlay { min-width: 52px; min-height: 42px; font-size: 24px; padding: 0; }
QToolButton#rbsMenu { min-width: 36px; min-height: 42px; font-size: 20px; padding: 0; border: none; background: transparent; }
QToolButton::menu-indicator { image: none; }
QMenu { background: #111418; border: 1px solid #252a31; color: #dfe4ea; }
QMenu::item { padding: 6px 18px; }
QMenu::item:selected { background: #1b3a66; }
QProgressBar { background: #1c2128; border: none; max-height: 6px; }
QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3ecf7a, stop:0.8 #f2c94c, stop:1 #ef4444); }
QCheckBox, QRadioButton { color: #dfe4ea; spacing: 6px; }
QCheckBox::indicator { width: 11px; height: 11px; border: 1px solid #4a525d; background: #08090b; }
QCheckBox::indicator:checked { background: #3d8bff; border-color: #3d8bff; }
QRadioButton::indicator { width: 11px; height: 11px; border: 1px solid #4a525d; border-radius: 6px; background: #08090b; }
QRadioButton::indicator:checked { background: #3d8bff; border-color: #3d8bff; }
QRadioButton:disabled, QCheckBox:disabled, QLabel:disabled { color: #4a525d; }
QListWidget { background: #111418; border: none; outline: 0; }
QListWidget::item { padding: 3px 8px; border-left: 2px solid transparent; color: #dfe4ea; }
QListWidget::item:selected { background: #13213a; border-left: 2px solid #3d8bff; color: #dfe4ea; }
QListWidget::item:disabled { color: #7f8a98; }
QStatusBar { background: #050607; color: #7f8a98; border-top: 1px solid #252a31; }
QStatusBar QLabel { color: #7f8a98; padding: 0 8px; }
QStatusBar::item { border: none; }
QSlider::groove:horizontal { height: 4px; background: #2d333b; }
QSlider::sub-page:horizontal { background: #3d8bff; }
QSlider::handle:horizontal { width: 10px; margin: -6px 0; background: #dfe4ea; }
QSlider::handle:horizontal:disabled, QSlider::sub-page:horizontal:disabled { background: #4a525d; }
QSlider#rbsScaleSlider::groove:vertical { width: 6px; background: #2d333b; }
QSlider#rbsScaleSlider::add-page:vertical { background: #3d8bff; }
QSlider#rbsScaleSlider::sub-page:vertical { background: #2d333b; }
QSlider#rbsScaleSlider::handle:vertical { height: 12px; margin: 0 -9px; background: #dfe4ea; }
QLabel { color: #dfe4ea; }
QLabel[rbsDim="true"] { color: #7f8a98; }
QLabel#rbsLogo { font-weight: bold; font-style: italic; font-size: 17px; letter-spacing: 2px; padding: 0 6px; }
)";

static QLabel *dimLabel(const QString& text, QWidget *parent = nullptr)
{
    QLabel *label = new QLabel(text, parent);
    label->setProperty("rbsDim", true);
    return label;
}

static QVBoxLayout *noMargins(QVBoxLayout *layout)
{
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    return layout;
}

// Slider with its value shown on the right, as in SDR++
static QWidget *sliderRow(QSlider *slider, QLabel *value)
{
    QWidget *row = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(slider, 1);
    value->setMinimumWidth(56);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(value);
    return row;
}

RBSWindow::RBSWindow(WebAPIAdapterInterface *webAPIAdapterInterface, QWidget *parent) :
    QMainWindow(parent),
    m_layout(AllInOne),
    m_updatingControls(false),
    m_spectrumCenter(-1),
    m_spectrumRate(-1),
    m_displayFFTSize(4096),
    m_displayLinesPerSecond(12),
    m_lastPacketSequence(0),
    m_countryTreeDirty(false),
    m_messageCount(0),
    m_farthestKm(0.0)
{
    setObjectName("RBSWindow");
    setWindowTitle("RBS");
    setWindowIcon(qApp->windowIcon());   // RBS icon, set by the main window
    setStyleSheet(m_styleSheet);

    m_controller = new RBSController(webAPIAdapterInterface, this);
    connect(m_controller, &RBSController::stateChanged, this, &RBSWindow::onControllerState);
    connect(m_controller, &RBSController::modeStarted, this, &RBSWindow::onModeStarted);

    m_modes = RBSMode::loadAll();

    // Top bar
    QToolBar *toolBar = new QToolBar("RBS", this);
    toolBar->setObjectName("rbsTopBarToolbar");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->toggleViewAction()->setVisible(false);
    toolBar->addWidget(createTopBar());
    addToolBar(Qt::TopToolBarArea, toolBar);

    // Panels, then the two layouts they live in
    m_controlsPanel = createControls();
    m_spectrumPanel = createSpectrum();
    m_decodesPanel = createDecodes();
    m_infoPanel = createInfo();
    createPages();

    // Status bar
    m_statusLocation = new QLabel(this);
    m_statusRate = new QLabel(this);
    m_statusState = new QLabel("Prêt", this);
    m_statusClock = new QLabel(this);
    statusBar()->addWidget(m_statusLocation);
    statusBar()->addWidget(m_statusRate);
    statusBar()->addWidget(m_statusState, 1);
    statusBar()->addPermanentWidget(dimLabel("Moteur SDRangel", this));
    statusBar()->addPermanentWidget(m_statusClock);
    updateStationDisplay();

    // Decode sources
    m_ft8Log = new RBSFT8Log(this);
    connect(m_ft8Log, &RBSFT8Log::decoded, this, &RBSWindow::onFT8Decode);
    m_dataFeed = new MCPDataFeed(this);
    connect(m_dataFeed, &MCPDataFeed::packetsChanged, this, &RBSWindow::onPackets, Qt::QueuedConnection);
    connect(&m_adsbTimer, &QTimer::timeout, this, &RBSWindow::refreshAircraft);
    m_countryTreeTimer.setInterval(1000);
    connect(&m_countryTreeTimer, &QTimer::timeout, this, [this]() {
        // Every 10 s, also let the "new" marks expire
        static int ticks = 0;

        if (++ticks % 10 == 0)
        {
            m_countryTreeDirty = true;
            updateCountryBadge();
        }

        rebuildCountryTree();
    });
    m_countryTreeTimer.start();

    m_listenUpdateTimer.setSingleShot(true);
    m_listenUpdateTimer.setInterval(150);
    connect(&m_listenUpdateTimer, &QTimer::timeout, this, [this]() {
        m_controller->updateListen(m_listen);
        saveListen();
    });

    // Sources and modes
    populateDevices();
    populateModeList();

    // Lists of the combo boxes drawn by the style sheet: grey, opening downwards
    for (QComboBox *combo : findChildren<QComboBox*>()) {
        combo->setItemDelegate(new QStyledItemDelegate(combo));
    }

    // Layout and size used last; full screen the first time, as SDR++
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    setLayoutMode(settings.value("layout", "allinone").toString() == "tabs" ? Tabs : AllInOne);
    QByteArray geometry = settings.value("geometry").toByteArray();
    settings.endGroup();

    if (geometry.isEmpty()) {
        setWindowState(Qt::WindowMaximized);
    } else {
        restoreGeometry(geometry);
    }

    connect(&m_pollTimer, &QTimer::timeout, this, &RBSWindow::poll);
    m_pollTimer.start(250);
}

RBSWindow::~RBSWindow()
{
    m_pollTimer.stop();
    m_adsbTimer.stop();
    m_spectrumTee->detach();
    delete m_spectrumTee;
}

// ---------------------------------------------------------------------------
// Top bar

QWidget *RBSWindow::createTopBar()
{
    QWidget *bar = new QWidget(this);
    bar->setObjectName("rbsTopBar");
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(6, 4, 8, 4);
    layout->setSpacing(12);

    // SDR++ style menu: what is not used every minute
    QToolButton *menuButton = new QToolButton(bar);
    menuButton->setObjectName("rbsMenu");
    menuButton->setText("≡");
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(menuButton);
    menu->addAction("Réglages experts (fenêtre SDRangel)", this, []() {
        if (MainWindow *mainWindow = MainWindow::getInstance())
        {
            mainWindow->showNormal();
            mainWindow->raise();
            mainWindow->activateWindow();
        }
    });
    menu->addAction("Dossier des fiches de modes", this, []() {
        QString path = RBSMode::userModesPath();
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    menu->addSeparator();
    menu->addAction("Quitter RBS", this, [this]() { close(); });
    menuButton->setMenu(menu);
    layout->addWidget(menuButton);

    m_play = new QToolButton(bar);
    m_play->setObjectName("rbsPlay");
    m_play->setCheckable(true);
    m_play->setText("▶");
    m_play->setToolTip("Démarrer / arrêter la réception");
    connect(m_play, &QToolButton::clicked, this, [this](bool checked) {
        if (checked) {
            m_controller->startDevice();
        } else {
            m_controller->stopDevice();
        }
    });
    layout->addWidget(m_play);

    m_frequency = new RBSFrequencyDial(bar);
    m_frequency->setRange(0, 2000000000LL);   // RSP1B: up to 2 GHz
    connect(m_frequency, &RBSFrequencyDial::changed, this, [this](qint64 value) {
        if (!m_updatingControls) {
            tuneTo(value);
        }
    });
    layout->addWidget(m_frequency);
    layout->addWidget(dimLabel("Hz", bar));

    // Band: opens the known frequencies of the mode
    QVBoxLayout *bandBox = new QVBoxLayout();
    bandBox->setSpacing(0);
    bandBox->addWidget(dimLabel("Bande", bar));
    m_bandButton = new QToolButton(bar);
    m_bandButton->setObjectName("rbsBandButton");
    m_bandButton->setText("—  ▾");
    m_bandButton->setCursor(Qt::PointingHandCursor);
    m_bandButton->setToolTip("Choisir une fréquence connue du mode");
    bandBox->addWidget(m_bandButton);
    layout->addLayout(bandBox);
    m_bandPopup = new RBSBandPopup(this);
    connect(m_bandButton, &QToolButton::clicked, this, [this]() {
        m_bandPopup->showFor(m_bandButton, m_controller->getMode(), m_frequency->value());
    });
    connect(m_bandPopup, &RBSBandPopup::frequencyChosen, this, &RBSWindow::tuneTo);

    QVBoxLayout *levelBox = new QVBoxLayout();
    levelBox->setSpacing(2);
    levelBox->addWidget(dimLabel("Niveau du canal", bar));
    m_level = new QProgressBar(bar);
    m_level->setRange(-130, 0);
    m_level->setValue(-130);
    m_level->setTextVisible(false);
    m_level->setFixedWidth(130);
    levelBox->addWidget(m_level);
    m_levelText = new QLabel("—", bar);
    levelBox->addWidget(m_levelText);
    layout->addLayout(levelBox);

    m_deviceStatus = new QLabel("■ RSP1B", bar);
    layout->addWidget(m_deviceStatus);

    layout->addStretch(1);

    m_layoutAllInOne = new QToolButton(bar);
    m_layoutAllInOne->setText("Tout-en-un");
    m_layoutAllInOne->setCheckable(true);
    m_layoutTabs = new QToolButton(bar);
    m_layoutTabs->setText("Onglets");
    m_layoutTabs->setCheckable(true);
    QButtonGroup *layoutGroup = new QButtonGroup(bar);
    layoutGroup->setExclusive(true);
    layoutGroup->addButton(m_layoutAllInOne);
    layoutGroup->addButton(m_layoutTabs);
    QHBoxLayout *layoutSwitch = new QHBoxLayout();
    layoutSwitch->setSpacing(0);
    layoutSwitch->addWidget(m_layoutAllInOne);
    layoutSwitch->addWidget(m_layoutTabs);
    layout->addLayout(layoutSwitch);
    connect(m_layoutAllInOne, &QToolButton::clicked, this, [this]() { setLayoutMode(AllInOne); });
    connect(m_layoutTabs, &QToolButton::clicked, this, [this]() { setLayoutMode(Tabs); });

    // Logo: the image is twice the displayed height, for high density screens
    QLabel *logo = new QLabel(bar);
    logo->setObjectName("rbsLogo");
    QPixmap logoPixmap(":/rbslauncher/images/rbs_logo_white.png");
    logoPixmap.setDevicePixelRatio(2.0);
    logo->setPixmap(logoPixmap);
    layout->addWidget(logo);

    return bar;
}

// ---------------------------------------------------------------------------
// Controls: Source, Radio, Modes, My station

QWidget *RBSWindow::createControls()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = noMargins(new QVBoxLayout(panel));

    // --- Source
    RBSSection *source = new RBSSection("Source", panel);
    m_device = new QComboBox();
    m_device->setToolTip("D'où viennent les échantillons : le RSP1B, la carte son (IQ), un fichier IQ, le réseau…");
    source->addWidget(m_device);

    m_sdrplayControls = new QWidget();
    QFormLayout *sdrplay = new QFormLayout(m_sdrplayControls);
    sdrplay->setContentsMargins(0, 4, 0, 0);
    sdrplay->setHorizontalSpacing(8);
    sdrplay->setVerticalSpacing(5);
    sdrplay->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_sampleRate = new QComboBox();

    for (const auto& rate : m_sampleRates)
    {
        int baseband = rate.m_devSampleRate >> rate.m_log2Decim;
        QString text = baseband >= 1000000 ? QString("%1 MHz").arg(baseband / 1e6, 0, 'g', 3) : QString("%1 kHz").arg(baseband / 1e3, 0, 'g', 4);
        m_sampleRate->addItem(text, QVariant::fromValue(QPoint(rate.m_devSampleRate, rate.m_log2Decim)));
    }

    m_sampleRate->setToolTip("Largeur de spectre reçue. Plus large = plus de signaux visibles, plus de CPU");
    sdrplay->addRow(dimLabel("Échantillonnage"), m_sampleRate);

    m_lna = new QSlider(Qt::Horizontal);
    m_lna->setRange(0, 9);
    m_lnaText = new QLabel();
    m_lna->setToolTip("Gain LNA du RSP1B. Trop fort en VHF près de Lyon : saturation");
    sdrplay->addRow(dimLabel("Gain LNA"), sliderRow(m_lna, m_lnaText));

    m_ifAGC = new QCheckBox("AGC IF");
    sdrplay->addRow(m_ifAGC);
    m_ifGain = new QSlider(Qt::Horizontal);
    m_ifGain->setRange(0, 39);
    m_ifGainText = new QLabel();
    m_ifGain->setToolTip("Gain IF, réglable quand l'AGC IF est coupée");
    sdrplay->addRow(dimLabel("Gain IF"), sliderRow(m_ifGain, m_ifGainText));

    m_amNotch = new QCheckBox("Notch AM (PO)");
    m_fmNotch = new QCheckBox("Notch FM broadcast");
    m_dabNotch = new QCheckBox("Notch DAB");
    m_biasTee = new QCheckBox("Bias-T (alimente l'antenne)");
    m_biasTee->setToolTip("Envoie une tension sur le câble : seulement pour une antenne active");
    sdrplay->addRow(m_amNotch);
    sdrplay->addRow(m_fmNotch);
    sdrplay->addRow(m_dabNotch);
    sdrplay->addRow(m_biasTee);
    source->addWidget(m_sdrplayControls);
    layout->addWidget(source);

    connect(m_device, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        QString hwType = m_device->itemData(index, Qt::UserRole).toString();
        int sequence = m_device->itemData(index, Qt::UserRole + 1).toInt();

        // Same source chosen again: nothing to do (and nothing to restart)
        if ((hwType == m_controller->getHwType()) && m_controller->hasDevice()) {
            return;
        }

        m_controller->setDevice(hwType, sequence);
        QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
        settings.beginGroup(m_settingsGroup);
        settings.setValue("deviceHwType", hwType);
        settings.setValue("deviceSequence", sequence);
        settings.endGroup();
        m_sdrplayControls->setVisible(m_controller->isSDRplay());
        startMode(m_controller->getMode().m_id);
    });
    connect(m_sampleRate, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        QPoint rate = m_sampleRate->itemData(index).toPoint();
        m_controller->setDeviceSetting("devSampleRate", rate.x());
        m_controller->setDeviceSetting("log2Decim", rate.y());
    });
    connect(m_lna, &QSlider::valueChanged, this, [this](int value) {
        m_lnaText->setText(QString("%1 / 9").arg(value));
        if (!m_updatingControls) {
            m_controller->setDeviceSetting("lnaIndex", 9 - value); // index 0 = most gain
        }
    });
    connect(m_ifGain, &QSlider::valueChanged, this, [this](int value) {
        m_ifGainText->setText(QString("%1 dB").arg(value));
        if (!m_updatingControls) {
            m_controller->setDeviceSetting("ifGain", value - 59); // gain reduction 59..20 dB
        }
    });
    connect(m_ifAGC, &QCheckBox::toggled, this, [this](bool checked) {
        m_ifGain->setEnabled(!checked);
    });

    auto connectCheck = [this](QCheckBox *check, const QString& key) {
        connect(check, &QCheckBox::toggled, this, [this, key](bool checked) {
            if (!m_updatingControls) {
                m_controller->setDeviceSetting(key, checked ? 1 : 0);
            }
        });
    };
    connectCheck(m_ifAGC, "ifAGC");
    connectCheck(m_amNotch, "amNotch");
    connectCheck(m_fmNotch, "fmNotch");
    connectCheck(m_dabNotch, "dabNotch");
    connectCheck(m_biasTee, "biasTee");

    // --- Radio: listening channel, SDR++ style
    RBSSection *radio = new RBSSection("Radio", panel);
    QWidget *demods = new QWidget();
    QGridLayout *demodGrid = new QGridLayout(demods);
    demodGrid->setContentsMargins(0, 0, 0, 4);
    demodGrid->setHorizontalSpacing(6);
    demodGrid->setVerticalSpacing(4);
    m_demodGroup = new QButtonGroup(this);
    m_demodGroup->setExclusive(true);
    const QStringList demodNames = {"Coupé", "NFM", "AM", "WFM", "USB", "LSB", "CW"};

    for (int i = 0; i < demodNames.size(); i++)
    {
        QRadioButton *button = new QRadioButton(demodNames[i]);
        button->setProperty("demod", i == 0 ? QString() : demodNames[i]);
        m_demodGroup->addButton(button, i);
        demodGrid->addWidget(button, i / 4, i % 4);
    }

    radio->addWidget(demods);

    m_bandwidth = new QSpinBox();
    m_bandwidth->setRange(100, 250000);
    m_bandwidth->setSingleStep(100);
    m_bandwidth->setSuffix(" Hz");
    m_bandwidth->setGroupSeparatorShown(true);
    radio->form()->addRow(dimLabel("Bande passante"), m_bandwidth);

    m_squelch = new QSlider(Qt::Horizontal);
    m_squelch->setRange(-100, 0);
    m_squelchText = new QLabel();
    radio->form()->addRow(dimLabel("SQL"), sliderRow(m_squelch, m_squelchText));

    m_volume = new QSlider(Qt::Horizontal);
    m_volume->setRange(0, 100);
    m_volumeText = new QLabel();
    radio->form()->addRow(dimLabel("Volume"), sliderRow(m_volume, m_volumeText));

    m_decoder = dimLabel("—");
    m_decoder->setWordWrap(true);
    radio->addWidget(m_decoder);
    layout->addWidget(radio);

    connect(m_demodGroup, &QButtonGroup::idClicked, this, [this](int id) {
        onDemodChanged(m_demodGroup->button(id)->property("demod").toString());
    });
    connect(m_bandwidth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_updatingControls)
        {
            m_listen.m_bandwidth = value;
            m_listenUpdateTimer.start();
        }
    });
    connect(m_squelch, &QSlider::valueChanged, this, [this](int value) {
        m_squelchText->setText(QString("%1 dB").arg(value));
        if (!m_updatingControls)
        {
            m_listen.m_squelch = value;
            m_listenUpdateTimer.start();
        }
    });
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        m_volumeText->setText(QString("%1 %").arg(value));
        if (!m_updatingControls)
        {
            m_listen.m_volume = value;
            m_listenUpdateTimer.start();
        }
    });

    // --- Display: spectrum resolution and waterfall speed, remembered per mode
    RBSSection *display = new RBSSection("Affichage", panel);
    m_fftSize = new QComboBox();

    for (int log2 = 10; log2 <= m_log2FFTSizeMax; log2++) {
        m_fftSize->addItem(QString::number(1 << log2), 1 << log2);
    }

    m_fftSize->setToolTip("Finesse du spectre : plus fin = signaux étroits (FT8) visibles, mais plus lent à réagir");
    display->form()->addRow(dimLabel("Résolution"), m_fftSize);
    m_waterfallSpeed = new QSlider(Qt::Horizontal);
    m_waterfallSpeed->setRange(1, 30);
    m_waterfallSpeed->setToolTip("Lignes de cascade par seconde. Plus lent = signaux faibles plus visibles");
    m_waterfallSpeedText = new QLabel();
    display->form()->addRow(dimLabel("Vitesse cascade"), sliderRow(m_waterfallSpeed, m_waterfallSpeedText));
    layout->addWidget(display);

    connect(m_fftSize, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        m_displayFFTSize = m_fftSize->itemData(index).toInt();
        saveDisplay();
        applySpectrumSettings();
    });
    connect(m_waterfallSpeed, &QSlider::valueChanged, this, [this](int value) {
        m_waterfallSpeedText->setText(QString("%1 /s").arg(value));

        if (!m_updatingControls)
        {
            m_displayLinesPerSecond = value;
            saveDisplay();
            applySpectrumSettings();
        }
    });

    // --- Modes: search and category
    RBSSection *modes = new RBSSection("Modes", panel);
    m_modeSearch = new QLineEdit();
    m_modeSearch->setPlaceholderText("Rechercher un mode…");
    m_modeSearch->setClearButtonEnabled(true);
    m_modeCategory = new QComboBox();
    m_modeList = new QListWidget();
    m_modeList->setMinimumHeight(120);
    modes->addWidget(m_modeSearch);
    modes->addWidget(m_modeCategory);
    modes->addWidget(m_modeList);
    layout->addWidget(modes);

    connect(m_modeSearch, &QLineEdit::textChanged, this, [this]() { populateModeList(); });
    connect(m_modeCategory, QOverload<int>::of(&QComboBox::activated), this, [this]() { populateModeList(); });
    connect(m_modeList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QString id = item->data(Qt::UserRole).toString();

        if (!id.isEmpty()) {
            startMode(id);
        }
    });

    // --- My station: reference for the distances
    RBSSection *station = new RBSSection("Ma station", panel);
    m_callsign = new QLineEdit();
    m_callsign->setPlaceholderText("ex. F4ABC");
    m_callsign->setToolTip("Les messages FT8 adressés à cet indicatif sont surlignés");
    m_locator = new QLineEdit();
    m_locator->setPlaceholderText("ex. JN25ox");
    m_locator->setMaxLength(6);
    m_locator->setToolTip("Point de référence des distances");
    station->form()->addRow(dimLabel("Indicatif"), m_callsign);
    station->form()->addRow(dimLabel("Locator"), m_locator);
    layout->addWidget(station);

    m_callsign->setText(MainCore::instance()->getSettings().getStationName());
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();

    if ((latitude != 0.0f) || (longitude != 0.0f)) {
        m_locator->setText(Maidenhead::toMaidenhead(latitude, longitude));
    }

    connect(m_callsign, &QLineEdit::editingFinished, this, [this]() {
        MainSettings& settings = MainCore::instance()->getMutableSettings();
        settings.setStationName(m_callsign->text().trimmed().toUpper());
        settings.save();
    });
    connect(m_locator, &QLineEdit::editingFinished, this, [this]() {
        float lat, lon;

        if (Maidenhead::fromMaidenhead(m_locator->text().trimmed(), lat, lon))
        {
            MainSettings& settings = MainCore::instance()->getMutableSettings();
            settings.setLatitude(lat);
            settings.setLongitude(lon);
            settings.save();
            updateStationDisplay();
            addEvent(QString("Position : %1").arg(m_locator->text().trimmed()));
        }
        else
        {
            addEvent(QString("Locator invalide : %1").arg(m_locator->text()));
        }
    });

    return panel;
}

void RBSWindow::populateDevices()
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    QString savedHwType = settings.value("deviceHwType", RBSController::m_sdrplayHwType).toString();
    int savedSequence = settings.value("deviceSequence", -1).toInt();
    settings.endGroup();

    // Friendly names for the sources that make sense here
    const QHash<QString, QString> names = {
        {"SDRplayV3", "SDRplay RSP1B"},
        {"AudioInput", "Carte son (IQ ou audio)"},
        {"FileInput", "Fichier IQ"},
        {"SigMFFileInput", "Fichier IQ SigMF"},
        {"RemoteTCPInput", "Réseau (rtl_tcp, RemoteTCP)"},
        {"KiwiSDR", "KiwiSDR (web)"}
    };

    DeviceEnumerator *enumerator = DeviceEnumerator::instance();
    int selected = -1;

    for (int i = 0; i < enumerator->getNbRxSamplingDevices(); i++)
    {
        const PluginInterface::SamplingDevice *device = enumerator->getRxSamplingDevice(i);

        // Only the sources that make sense on their own: those named above, and real receivers
        // found plugged in (they have a serial number). Not LocalInput, RemoteInput, TestSource...
        if (!names.contains(device->hardwareId) && device->serial.isEmpty()) {
            continue;
        }

        QString name = names.value(device->hardwareId, device->displayedName);

        if (!device->serial.isEmpty() && (device->hardwareId == "SDRplayV3")) {
            name += QString("  ·  %1").arg(device->serial);
        }

        m_device->addItem(name);
        int row = m_device->count() - 1;
        m_device->setItemData(row, device->hardwareId, Qt::UserRole);
        m_device->setItemData(row, device->sequence, Qt::UserRole + 1);

        if ((device->hardwareId == savedHwType) && ((selected < 0) || (device->sequence == savedSequence))) {
            selected = row;
        }
    }

    // A saved source that is no longer offered (LocalInput...): back to the RSP1B
    if ((selected < 0) && !names.contains(savedHwType) && (savedHwType != RBSController::m_sdrplayHwType))
    {
        savedHwType = RBSController::m_sdrplayHwType;
        savedSequence = -1;

        for (int row = 0; row < m_device->count(); row++)
        {
            if (m_device->itemData(row, Qt::UserRole).toString() == savedHwType) {
                selected = row;
            }
        }
    }

    if (selected < 0)
    {
        // Saved source absent: keep it selected anyway so that the error says it is missing
        m_device->addItem(QString("%1 (absent)").arg(names.value(savedHwType, savedHwType)));
        selected = m_device->count() - 1;
        m_device->setItemData(selected, savedHwType, Qt::UserRole);
        m_device->setItemData(selected, -1, Qt::UserRole + 1);
    }

    m_device->setCurrentIndex(selected);
    m_controller->setDevice(savedHwType, savedSequence);
    m_sdrplayControls->setVisible(m_controller->isSDRplay());
}

void RBSWindow::populateModeList()
{
    // Categories, kept in the order of the modes
    if (m_modeCategory->count() == 0)
    {
        m_modeCategory->addItem("Toutes les catégories", QString());

        for (const auto& mode : m_modes)
        {
            if (m_modeCategory->findData(mode.m_group) < 0) {
                m_modeCategory->addItem(mode.m_group, mode.m_group);
            }
        }
    }

    QString search = m_modeSearch->text().trimmed();
    QString category = m_modeCategory->currentData().toString();
    QString currentId = m_controller->getMode().m_id;
    m_modeList->clear();

    for (const auto& mode : m_modes)
    {
        if (!category.isEmpty() && (mode.m_group != category)) {
            continue;
        }

        if (!search.isEmpty()
            && !mode.m_name.contains(search, Qt::CaseInsensitive)
            && !mode.m_group.contains(search, Qt::CaseInsensitive)
            && !mode.m_band.contains(search, Qt::CaseInsensitive)
            && !QString::number(mode.tunedFrequency() / 1e6, 'f', 3).contains(search)) {
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(mode.m_name, m_modeList);
        item->setData(Qt::UserRole, mode.m_id);
        item->setToolTip(QString("%1 · %2").arg(mode.m_group).arg(mode.m_band));

        if (mode.m_id == currentId) {
            m_modeList->setCurrentItem(item);
        }
    }
}

// ---------------------------------------------------------------------------
// Spectrum

QWidget *RBSWindow::createSpectrum()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QHBoxLayout *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_spectrum = new GLSpectrum();
    m_spectrum->setFont(font());
    m_spectrum->setDisplayWaterfall(true);
    m_spectrum->setDisplayCurrent(true);
    m_spectrum->setDisplayHistogram(false);
    m_spectrum->setDisplayMaxHold(false);
    m_spectrum->setDisplayGrid(true);
    m_spectrum->setDisplayGridIntensity(12);
    m_spectrum->setDisplayTraceIntensity(80);
    m_spectrum->setSpectrumStyle(SpectrumSettings::Gradient);
    m_spectrum->setSpectrumColor(qRgb(61, 139, 255));
    m_spectrum->setColorMapName("Jet");
    m_spectrum->setWaterfallShare(0.66f);   // SDR++: spectrum a third, waterfall two thirds
    m_spectrum->setReferenceLevel(-30.0f);
    m_spectrum->setPowerRange(80.0f);
    m_spectrum->setDecay(1);
    m_spectrum->setFPSPeriodMs(40);
    m_spectrum->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // No measurements panel: its splitter handle only took height from the waterfall
    if (QSplitter *measurements = m_spectrum->findChild<QSplitter*>())
    {
        if (measurements->count() > 1) {
            measurements->widget(1)->hide();
        }

        measurements->setHandleWidth(0);
    }

    layout->addWidget(m_spectrum, 1);
    m_spectrumTee = new RBSSpectrumTee(m_spectrum);

    // SDR++ vertical sliders: zoom, top and bottom of the scale. Wide enough to grab easily
    QWidget *sliders = new QWidget(panel);
    sliders->setObjectName("rbsPanel");
    sliders->setFixedWidth(60);
    QVBoxLayout *slidersLayout = new QVBoxLayout(sliders);
    slidersLayout->setContentsMargins(4, 8, 4, 8);
    slidersLayout->setSpacing(6);

    auto scaleSlider = [sliders](int min, int max, int value, const QString& tip) {
        QSlider *slider = new QSlider(Qt::Vertical, sliders);
        slider->setObjectName("rbsScaleSlider");
        slider->setRange(min, max);
        slider->setValue(value);
        slider->setToolTip(tip);
        slider->setMinimumWidth(30);
        slider->setCursor(Qt::SizeVerCursor);
        return slider;
    };
    m_zoom = scaleSlider(0, 60, 0, "Zoom autour de la fréquence du décodeur (jusqu'à x64)");
    m_refLevel = scaleSlider(-120, 20, -30, "Haut de l'échelle (dB)");
    m_range = scaleSlider(-160, -20, -110, "Bas de l'échelle (dB)");
    slidersLayout->addWidget(dimLabel("Zoom", sliders), 0, Qt::AlignHCenter);
    slidersLayout->addWidget(m_zoom, 1, Qt::AlignHCenter);
    slidersLayout->addWidget(dimLabel("Max", sliders), 0, Qt::AlignHCenter);
    slidersLayout->addWidget(m_refLevel, 1, Qt::AlignHCenter);
    slidersLayout->addWidget(dimLabel("Min", sliders), 0, Qt::AlignHCenter);
    slidersLayout->addWidget(m_range, 1, Qt::AlignHCenter);
    layout->addWidget(sliders);

    // Scale remembered per mode
    auto scaleChanged = [this]() {
        if (!m_updatingControls)
        {
            applyScale();
            saveDisplay();
        }
    };
    connect(m_refLevel, &QSlider::valueChanged, this, scaleChanged);
    connect(m_range, &QSlider::valueChanged, this, scaleChanged);
    connect(m_zoom, &QSlider::valueChanged, this, [this]() {
        applyZoom();

        if (!m_updatingControls) {
            saveDisplay();
        }
    });

    return panel;
}

// ---------------------------------------------------------------------------
// Decodes

QWidget *RBSWindow::createDecodes()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = noMargins(new QVBoxLayout(panel));

    QWidget *tools = new QWidget(panel);
    tools->setObjectName("rbsPanelHeader");
    QHBoxLayout *toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(8, 4, 6, 4);
    toolsLayout->setSpacing(8);
    m_decodesTitle = new QLabel("Décodages", tools);
    m_search = new QLineEdit(tools);
    m_search->setPlaceholderText("Chercher : indicatif, pays, locator…");
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(300);
    m_groupByCountry = new QCheckBox("Regrouper par pays", tools);
    m_newCountryBadge = new QLabel(tools);
    m_newCountryBadge->setObjectName("rbsNewCountry");
    m_newCountryBadge->setToolTip("Pays entendus pour la première fois : ouvre-les dans la vue par pays");
    m_newCountryBadge->hide();
    m_distanceReference = dimLabel(QString(), tools);
    QToolButton *copy = new QToolButton(tools);
    copy->setText("Copier");
    QToolButton *exportButton = new QToolButton(tools);
    exportButton->setText("Exporter CSV");
    QToolButton *clear = new QToolButton(tools);
    clear->setText("Effacer");
    toolsLayout->addWidget(m_decodesTitle);
    toolsLayout->addSpacing(10);
    toolsLayout->addWidget(m_search, 1);
    toolsLayout->addWidget(m_groupByCountry);
    toolsLayout->addWidget(m_newCountryBadge);
    toolsLayout->addStretch(1);
    toolsLayout->addWidget(m_distanceReference);
    toolsLayout->addWidget(copy);
    toolsLayout->addWidget(exportButton);
    toolsLayout->addWidget(clear);
    layout->addWidget(tools);

    m_decodeModel = new RBSDecodeModel(this);
    m_decodeFilter = new RBSDecodeFilter(this);
    m_decodeFilter->setSourceModel(m_decodeModel);
    m_decodeTable = new QTableView();
    m_decodeTable->setModel(m_decodeFilter);
    m_decodeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_decodeTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_decodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_decodeTable->setShowGrid(false);
    m_decodeTable->setWordWrap(false);
    m_decodeTable->verticalHeader()->setVisible(false);
    m_decodeTable->verticalHeader()->setDefaultSectionSize(22);
    m_decodeTable->horizontalHeader()->setStretchLastSection(true);
    m_decodeTable->horizontalHeader()->setHighlightSections(false);
    m_decodeTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_decodeTable->horizontalHeader()->setDefaultSectionSize(95);

    // Call 1, call 2 and message as three tags of their own colour
    m_call1Chip = new RBSChipDelegate(QColor("#1d2a3d"), QColor("#9ec3f5"), false, this);
    m_call2Chip = new RBSChipDelegate(QColor("#2b3d5c"), QColor("#ffffff"), true, this);
    m_messageChip = new RBSChipDelegate(QColor("#33291a"), QColor("#f2c94c"), false, this);

    // Countries: one box per country, its stations under it
    m_countryTree = new QTreeWidget();
    m_countryTree->setColumnCount(7);
    m_countryTree->setHeaderLabels({"Pays / indicatif", "Messages", "Heure", "Call 1", "Message", "Meilleur dB", "Distance"});
    m_countryTree->setRootIsDecorated(false);
    m_countryTree->setIndentation(22);
    m_countryTree->setExpandsOnDoubleClick(false);
    m_countryTree->setMouseTracking(true);
    m_countryTree->setItemDelegate(new RBSCountryDelegate(m_countryTree));
    m_countryTree->setItemDelegateForColumn(3, m_call1Chip);
    m_countryTree->setItemDelegateForColumn(4, m_messageChip);
    m_countryTree->header()->setDefaultSectionSize(110);
    m_countryTree->setColumnWidth(0, 200);
    m_countryTree->setColumnWidth(2, 70);
    m_countryTree->setColumnWidth(4, 130);
    m_countryTree->setSortingEnabled(true);
    m_countryTree->sortByColumn(1, Qt::DescendingOrder);
    connect(m_countryTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (item->parent()) {
            return;
        }

        item->setExpanded(!item->isExpanded());
        QString country = item->data(0, RBSCountryDelegate::KeyRole).toString();

        if (item->data(0, RBSCountryDelegate::NewRole).toBool())
        {
            m_seenCountries.insert(country);
            item->setData(0, RBSCountryDelegate::NewRole, false);
            updateCountryBadge();
        }
    });

    m_decodeViews = new QStackedWidget(panel);
    m_decodeViews->addWidget(m_decodeTable);
    m_decodeViews->addWidget(m_countryTree);
    layout->addWidget(m_decodeViews, 1);

    connect(m_search, &QLineEdit::textChanged, m_decodeFilter, &RBSDecodeFilter::setSearch);
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        m_countryTreeDirty = true;
    });
    connect(m_groupByCountry, &QCheckBox::toggled, this, [this](bool checked) {
        m_decodeViews->setCurrentWidget(checked ? (QWidget*) m_countryTree : (QWidget*) m_decodeTable);
        m_countryTreeDirty = true;
        rebuildCountryTree();
        updateCountryBadge();
    });
    connect(copy, &QToolButton::clicked, this, &RBSWindow::copySelection);
    connect(exportButton, &QToolButton::clicked, this, &RBSWindow::exportCSV);
    connect(clear, &QToolButton::clicked, this, [this]() {
        m_decodeModel->clear();
        m_stationsByCountry.clear();
        m_countryFirstHeard.clear();
        m_seenCountries.clear();
        m_countryTreeDirty = true;
        rebuildCountryTree();
        updateCountryBadge();
    });

    return panel;
}

QWidget *RBSWindow::createInfo()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = noMargins(new QVBoxLayout(panel));

    RBSSection *today = new RBSSection("Aujourd'hui", panel);
    m_statsMessages = new QLabel("0");
    m_statsCalls = new QLabel("0");
    m_statsCountries = new QLabel("0");
    m_statsFarthest = new QLabel("—");
    m_statsFarthest->setWordWrap(true);
    today->form()->addRow(dimLabel("Messages"), m_statsMessages);
    today->form()->addRow(dimLabel("Indicatifs"), m_statsCalls);
    today->form()->addRow(dimLabel("Pays"), m_statsCountries);
    today->form()->addRow(dimLabel("Plus loin"), m_statsFarthest);
    layout->addWidget(today);

    RBSSection *events = new RBSSection("Événements", panel);
    m_events = new QListWidget();
    m_events->setMinimumHeight(120);
    m_events->setSelectionMode(QAbstractItemView::NoSelection);
    events->addWidget(m_events);
    layout->addWidget(events);

    return panel;
}

// A scroll area whose content is a column the panels can be added to
static QScrollArea *scrollColumn(QWidget *parent, QVBoxLayout *&column)
{
    QScrollArea *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget(scroll);
    content->setObjectName("rbsPanel");
    column = noMargins(new QVBoxLayout(content));
    column->addStretch(1);
    scroll->setWidget(content);
    return scroll;
}

void RBSWindow::createPages()
{
    m_pages = new QStackedWidget(this);

    // All in one: fixed side menu | spectrum and waterfall over the decodes
    m_allInOnePage = new QWidget(m_pages);
    QHBoxLayout *allInOne = new QHBoxLayout(m_allInOnePage);
    allInOne->setContentsMargins(0, 0, 0, 0);
    allInOne->setSpacing(0);
    QScrollArea *sidebar = scrollColumn(m_allInOnePage, m_sidebarBox);
    sidebar->setObjectName("rbsSidebar");
    sidebar->setFixedWidth(m_sidebarWidth);
    allInOne->addWidget(sidebar);

    // Spectrum over decodes: drag the line between them, what the waterfall gives the decodes get
    m_centerSplitter = new QSplitter(Qt::Vertical, m_allInOnePage);
    m_centerSplitter->setChildrenCollapsible(false);
    m_centerSplitter->setHandleWidth(5);
    m_centerSplitter->setStyleSheet("QSplitter::handle { background: #252a31; } QSplitter::handle:hover { background: #3d8bff; }");
    allInOne->addWidget(m_centerSplitter, 1);
    m_pages->addWidget(m_allInOnePage);
    connect(m_centerSplitter, &QSplitter::splitterMoved, this, [this]() {
        QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
        settings.beginGroup(m_settingsGroup);
        settings.setValue("centerSplitter", m_centerSplitter->saveState());
        settings.endGroup();
    });

    // Tabs: each panel full window
    m_tabs = new QTabWidget(m_pages);
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    QWidget *spectrumTab = new QWidget(m_tabs);
    m_tabSpectrumBox = noMargins(new QVBoxLayout(spectrumTab));
    QWidget *decodesTab = new QWidget(m_tabs);
    m_tabDecodesBox = noMargins(new QVBoxLayout(decodesTab));
    m_tabs->addTab(spectrumTab, "Spectre et cascade");
    m_tabs->addTab(decodesTab, "Décodages");
    m_tabs->addTab(scrollColumn(m_tabs, m_tabControlsBox), "Contrôles");
    m_tabs->addTab(scrollColumn(m_tabs, m_tabInfoBox), "Infos");
    m_pages->addWidget(m_tabs);

    setCentralWidget(m_pages);
}

void RBSWindow::setLayoutMode(Layout layout)
{
    m_layout = layout;
    m_layoutAllInOne->setChecked(layout == AllInOne);
    m_layoutTabs->setChecked(layout == Tabs);

    if (layout == AllInOne)
    {
        // Side menu: controls then info, above the stretch
        m_sidebarBox->insertWidget(0, m_controlsPanel);
        m_sidebarBox->insertWidget(1, m_infoPanel);
        m_centerSplitter->insertWidget(0, m_spectrumPanel);
        m_centerSplitter->insertWidget(1, m_decodesPanel);
        m_centerSplitter->setStretchFactor(0, 1);
        m_centerSplitter->setStretchFactor(1, 0);
        m_spectrumPanel->setMinimumHeight(120);
        m_decodesPanel->setMinimumHeight(90);

        QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
        settings.beginGroup(m_settingsGroup);
        QByteArray state = settings.value("centerSplitter").toByteArray();
        settings.endGroup();

        if (state.isEmpty() || !m_centerSplitter->restoreState(state)) {
            m_centerSplitter->setSizes({std::max(200, height() - m_decodesHeight), m_decodesHeight});
        }

        m_pages->setCurrentWidget(m_allInOnePage);
    }
    else
    {
        m_tabSpectrumBox->addWidget(m_spectrumPanel, 1);
        m_tabDecodesBox->addWidget(m_decodesPanel, 1);
        m_tabControlsBox->insertWidget(0, m_controlsPanel);
        m_tabInfoBox->insertWidget(0, m_infoPanel);
        m_pages->setCurrentWidget(m_tabs);
    }

    m_spectrumPanel->show();
    m_decodesPanel->show();
    m_controlsPanel->show();
    m_infoPanel->show();

    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.setValue("layout", layout == Tabs ? "tabs" : "allinone");
    settings.endGroup();
}

void RBSWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
    event->accept();

    // RBS is the application: closing it closes SDRangel, which saves its configuration
    QTimer::singleShot(0, []() {
        if (MainWindow *mainWindow = MainWindow::getInstance()) {
            mainWindow->close();
        }
    });
}

void RBSWindow::updateStationDisplay()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();
    bool known = (latitude != 0.0f) || (longitude != 0.0f);
    QString locator = known ? Maidenhead::toMaidenhead(latitude, longitude) : QString();

    m_statusLocation->setText(known ? locator : QString("Position non définie (Ma station)"));
    m_distanceReference->setText(known ? QString("Distances depuis %1").arg(locator) : QString("Distances : renseigne ton locator dans Ma station"));
}

// ---------------------------------------------------------------------------
// Modes

const RBSMode *RBSWindow::findMode(const QString& modeId) const
{
    for (const auto& mode : m_modes)
    {
        if (mode.m_id == modeId) {
            return &mode;
        }
    }

    return nullptr;
}

void RBSWindow::startMode(const QString& modeId)
{
    const RBSMode *mode = findMode(modeId);

    if (!mode && !m_modes.isEmpty()) {
        mode = &m_modes.first();
    }
    if (!mode) {
        return;
    }

    m_ft8Log->stop();
    m_adsbTimer.stop();

    // The frequency chosen last for this mode (band selector, dial) instead of the sheet's one
    RBSMode started = *mode;
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    qint64 tuned = settings.value("tuned_" + mode->m_id, 0).toLongLong();
    settings.endGroup();

    if (tuned > 0) {
        started.m_device.insert("centerFrequency", (double) (tuned - started.m_channelOffset));
    }

    // Same mode again (source changed, mode clicked again): the decodes stay
    if (started.m_id != m_viewModeId)
    {
        m_lastPacketSequence = 0;
        setupDecodeView(started);
    }

    showModeSettings(started);
    loadListen(started);
    loadDisplay(started);
    setWindowTitle(QString("RBS — %1").arg(started.m_name));
    m_controller->startMode(started);
    populateModeList();
    emit modeChanged(started.m_id);
}

void RBSWindow::showModeSettings(const RBSMode& mode)
{
    qint64 tuned = mode.tunedFrequency();
    m_bandButton->setText(mode.bandOf(tuned) + "  ▾");
    m_decoder->setText(QString("Décodeur %1 sur %2").arg(mode.m_name).arg(RBSBandPopup::formatMHz(tuned)));

    m_updatingControls = true;
    m_frequency->setValue(tuned);
    m_updatingControls = false;
}

// Dial, band selector: the decoder goes there, and the mode will start there next time
void RBSWindow::tuneTo(qint64 frequency)
{
    m_controller->setTunedFrequency(frequency);
    const RBSMode& mode = m_controller->getMode();
    m_bandButton->setText(mode.bandOf(frequency) + "  ▾");
    m_decoder->setText(QString("Décodeur %1 sur %2").arg(mode.m_name).arg(RBSBandPopup::formatMHz(frequency)));

    if (m_frequency->value() != frequency)
    {
        m_updatingControls = true;
        m_frequency->setValue(frequency);
        m_updatingControls = false;
    }

    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.setValue("tuned_" + mode.m_id, frequency);
    settings.endGroup();
}

// ---------------------------------------------------------------------------
// Spectrum display: resolution, waterfall speed and zoom

void RBSWindow::loadDisplay(const RBSMode& mode)
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.beginGroup("display_" + mode.m_id);
    m_displayFFTSize = std::min(settings.value("fftSize", mode.m_fftSize).toInt(), 1 << m_log2FFTSizeMax);
    m_displayLinesPerSecond = settings.value("linesPerSecond", mode.m_waterfallLinesPerSecond).toInt();
    int maxLevel = settings.value("maxLevel", mode.m_maxLevel).toInt();
    int minLevel = settings.value("minLevel", mode.m_minLevel).toInt();
    int zoom = settings.value("zoom", 0).toInt();
    settings.endGroup();
    settings.endGroup();

    m_updatingControls = true;
    int index = m_fftSize->findData(m_displayFFTSize);
    m_fftSize->setCurrentIndex(index < 0 ? m_fftSize->findData(4096) : index);
    m_displayFFTSize = m_fftSize->currentData().toInt();
    m_waterfallSpeed->setValue(m_displayLinesPerSecond);
    m_waterfallSpeedText->setText(QString("%1 /s").arg(m_displayLinesPerSecond));
    m_refLevel->setValue(maxLevel);
    m_range->setValue(minLevel);
    m_zoom->setValue(zoom);
    m_updatingControls = false;
    applyScale();
}

void RBSWindow::applyScale()
{
    int top = m_refLevel->value();
    int bottom = std::min(m_range->value(), top - 10);
    m_spectrum->setReferenceLevel((float) top);
    m_spectrum->setPowerRange((float) (top - bottom));
}

void RBSWindow::saveDisplay()
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.beginGroup("display_" + m_controller->getMode().m_id);
    settings.setValue("fftSize", m_displayFFTSize);
    settings.setValue("linesPerSecond", m_displayLinesPerSecond);
    settings.setValue("maxLevel", m_refLevel->value());
    settings.setValue("minLevel", m_range->value());
    settings.setValue("zoom", m_zoom->value());
    settings.endGroup();
    settings.endGroup();
}

// The waterfall gets one line per spectrum: averaging N spectra gives sample rate / (FFT size x N)
// lines per second, and weak signals stand out of the noise
void RBSWindow::applySpectrumSettings()
{
    DeviceSet *deviceSet = m_controller->getDeviceSet();
    int rate = m_controller->getSampleRate();

    if (!deviceSet || !deviceSet->m_spectrumVis || (rate <= 0)) {
        return;
    }

    SpectrumSettings settings = deviceSet->m_spectrumVis->getSettings();
    double spectraPerSecond = (double) rate / m_displayFFTSize;
    int average = std::max(1, (int) (spectraPerSecond / std::max(1, m_displayLinesPerSecond)));
    settings.m_fftSize = m_displayFFTSize;
    settings.m_fftOverlap = 0;
    settings.m_averagingMode = average > 1 ? SpectrumSettings::AvgModeFixed : SpectrumSettings::AvgModeNone;
    settings.m_averagingIndex = SpectrumSettings::getAveragingIndex(average, SpectrumSettings::AvgModeFixed);
    settings.m_averagingValue = SpectrumSettings::getAveragingValue(settings.m_averagingIndex, SpectrumSettings::AvgModeFixed);
    deviceSet->m_spectrumVis->getInputMessageQueue()->push(SpectrumVis::MsgConfigureSpectrumVis::create(settings, false));

    // So that the waterfall time scale counts averaged spectra right
    m_spectrum->setFFTOverlap(0);
    m_spectrum->setTimingRate(average > 1 ? (qint32) settings.m_averagingValue : 1);
}

// Zoom around the decoder: FT8 is 14 kHz above the centre, a centre zoom would leave it out
void RBSWindow::applyZoom()
{
    float zoom = std::pow(2.0f, m_zoom->value() / 10.0f);
    float position = 0.5f;
    int rate = m_controller->getSampleRate();

    if (rate > 0) {
        position += (float) m_controller->getMode().m_channelOffset / rate;
    }

    float limit = 0.5f / zoom;
    position = std::max(limit, std::min(1.0f - limit, position));
    m_spectrum->setFrequencyZooming(zoom, position);
}

void RBSWindow::setupDecodeView(const RBSMode& mode)
{
    m_viewModeId = mode.m_id;
    m_messageCount = 0;
    m_callsToday.clear();
    m_stationsByCountry.clear();
    m_countryFirstHeard.clear();
    m_seenCountries.clear();
    m_countryTreeDirty = true;

    for (int column = 0; column < 12; column++) {
        m_decodeTable->setItemDelegateForColumn(column, nullptr);
    }
    m_farthestKm = 0.0;
    m_statsMessages->setText("0");
    m_statsCalls->setText("0");
    m_statsCountries->setText("0");
    m_statsFarthest->setText("—");

    // Call 2, the calling station, is the column that matters most: in bold
    if (mode.m_view == "ft8")
    {
        m_decodesTitle->setText("Décodages FT8");
        m_decodeModel->setColumns({"UTC", "dB", "DT", "Δf", "Call 1", "Call 2", "Message", "Pays", "Locator", "Distance"}, 5);
        m_decodeTable->setItemDelegateForColumn(4, m_call1Chip);
        m_decodeTable->setItemDelegateForColumn(5, m_call2Chip);
        m_decodeTable->setItemDelegateForColumn(6, m_messageChip);
        m_decodeTable->setColumnWidth(0, 80);
        m_decodeTable->setColumnWidth(1, 50);
        m_decodeTable->setColumnWidth(2, 50);
        m_decodeTable->setColumnWidth(3, 60);
        m_decodeTable->setColumnWidth(4, 110);
        m_decodeTable->setColumnWidth(5, 110);
        m_decodeTable->setColumnWidth(6, 100);
    }
    else if (mode.m_view == "aprs")
    {
        m_decodesTitle->setText("Stations APRS");
        m_decodeModel->setColumns({"Heure", "Indicatif", "Pays", "Vers", "Via", "Position", "Distance", "Commentaire"}, 1);
    }
    else if (mode.m_view == "adsb")
    {
        m_decodesTitle->setText("Avions ADS-B");
        m_decodeModel->setColumns({"Vu", "Indicatif", "ICAO", "Altitude", "Cap", "Distance", "Infos"}, 1);
    }
    else
    {
        m_decodesTitle->setText("Décodages");
        m_decodeModel->setColumns({"Heure", "Message"});
    }

    bool countries = mode.m_view != "adsb";
    m_groupByCountry->setVisible(countries);

    if (!countries) {
        m_groupByCountry->setChecked(false);
    }

    m_tabs->setTabText(1, m_decodesTitle->text());
    rebuildCountryTree();
    updateCountryBadge();
}

void RBSWindow::onControllerState(RBSController::State state, const QString& message)
{
    m_statusState->setText(message);

    if (state == RBSController::Error)
    {
        m_statusState->setStyleSheet("color: #ef4444;");
        addEvent(QString("Erreur : %1").arg(message));
    }
    else
    {
        m_statusState->setStyleSheet(QString());
    }
}

void RBSWindow::onModeStarted(const RBSMode& mode)
{
    addEvent(QString("%1 lancé sur %2").arg(mode.m_name).arg(RBSBandPopup::formatMHz(mode.tunedFrequency())));
    attachSpectrum();
    applySpectrumSettings();
    applyZoom();
    readDeviceSettings();

    if (!m_listen.m_demod.isEmpty()) {
        m_controller->setListen(m_listen);
    }

    if (mode.m_view == "ft8") {
        m_ft8Log->start(m_controller->getDeviceSetIndex(), m_controller->getChannelIndex());
    } else if (mode.m_view == "adsb") {
        m_adsbTimer.start(1000);
    }
}

void RBSWindow::readDeviceSettings()
{
    if (!m_controller->isSDRplay()) {
        return;
    }

    int value, decim;
    m_updatingControls = true;

    if (m_controller->getDeviceSetting("devSampleRate", value) && m_controller->getDeviceSetting("log2Decim", decim))
    {
        int index = m_sampleRate->findData(QVariant::fromValue(QPoint(value, decim)));

        if (index < 0)
        {
            int baseband = value >> decim;
            m_sampleRate->addItem(QString("%1 kHz").arg(baseband / 1e3, 0, 'g', 4), QVariant::fromValue(QPoint(value, decim)));
            index = m_sampleRate->count() - 1;
        }

        m_sampleRate->setCurrentIndex(index);
    }
    if (m_controller->getDeviceSetting("lnaIndex", value))
    {
        m_lna->setValue(9 - value);
        m_lnaText->setText(QString("%1 / 9").arg(9 - value));
    }
    if (m_controller->getDeviceSetting("ifGain", value))
    {
        m_ifGain->setValue(value + 59);
        m_ifGainText->setText(QString("%1 dB").arg(value + 59));
    }
    if (m_controller->getDeviceSetting("ifAGC", value))
    {
        m_ifAGC->setChecked(value != 0);
        m_ifGain->setEnabled(value == 0);
    }
    if (m_controller->getDeviceSetting("amNotch", value)) {
        m_amNotch->setChecked(value != 0);
    }
    if (m_controller->getDeviceSetting("fmNotch", value)) {
        m_fmNotch->setChecked(value != 0);
    }
    if (m_controller->getDeviceSetting("dabNotch", value)) {
        m_dabNotch->setChecked(value != 0);
    }
    if (m_controller->getDeviceSetting("biasTee", value)) {
        m_biasTee->setChecked(value != 0);
    }

    m_updatingControls = false;
}

// ---------------------------------------------------------------------------
// Listening (Radio section). Remembered per mode

void RBSWindow::loadListen(const RBSMode& mode)
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.beginGroup("listen_" + mode.m_id);
    m_listen.m_demod = settings.value("demod", QString()).toString();
    m_listen.m_bandwidth = settings.value("bandwidth", 0).toInt();
    m_listen.m_squelch = settings.value("squelch", -40).toInt();
    m_listen.m_volume = settings.value("volume", 50).toInt();
    settings.endGroup();
    settings.endGroup();
    showListen();
}

void RBSWindow::saveListen()
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.beginGroup("listen_" + m_controller->getMode().m_id);
    settings.setValue("demod", m_listen.m_demod);
    settings.setValue("bandwidth", m_listen.m_bandwidth);
    settings.setValue("squelch", m_listen.m_squelch);
    settings.setValue("volume", m_listen.m_volume);
    settings.endGroup();
    settings.endGroup();
}

void RBSWindow::showListen()
{
    m_updatingControls = true;
    const QStringList demodNames = {"", "NFM", "AM", "WFM", "USB", "LSB", "CW"};
    int id = std::max(0, (int) demodNames.indexOf(m_listen.m_demod));
    m_demodGroup->button(id)->setChecked(true);

    bool on = !m_listen.m_demod.isEmpty();
    bool squelch = RBSController::hasSquelch(m_listen.m_demod);
    m_bandwidth->setValue(m_listen.m_bandwidth > 0 ? m_listen.m_bandwidth : RBSController::defaultBandwidth(m_listen.m_demod));
    m_bandwidth->setEnabled(on);
    m_squelch->setValue(m_listen.m_squelch);
    m_squelchText->setText(QString("%1 dB").arg(m_listen.m_squelch));
    m_squelch->setEnabled(squelch);
    m_squelch->setToolTip(squelch || !on ? "Seuil du squelch : en dessous, le son est coupé" : "Pas de squelch en BLU / CW dans SDRangel");
    m_volume->setValue(m_listen.m_volume);
    m_volumeText->setText(QString("%1 %").arg(m_listen.m_volume));
    m_volume->setEnabled(on);
    m_updatingControls = false;
}

void RBSWindow::onDemodChanged(const QString& demod)
{
    if (demod == m_listen.m_demod) {
        return;
    }

    m_listen.m_demod = demod;
    m_listen.m_bandwidth = RBSController::defaultBandwidth(demod);
    showListen();
    saveListen();
    m_controller->setListen(m_listen);
    addEvent(demod.isEmpty() ? QString("Écoute coupée") : QString("Écoute %1").arg(demod));
}

void RBSWindow::attachSpectrum()
{
    DeviceSet *deviceSet = m_controller->getDeviceSet();
    int deviceSetIndex = m_controller->getDeviceSetIndex();

    if (!deviceSet || !deviceSet->m_spectrumVis || m_spectrumTee->isAttachedTo(deviceSet->m_spectrumVis)) {
        return;
    }

    GLSpectrum *original = nullptr;
    MainWindow *mainWindow = MainWindow::getInstance();

    if (mainWindow && (deviceSetIndex >= 0) && (deviceSetIndex < (int) mainWindow->getDeviceUISets().size())) {
        original = mainWindow->getDeviceUISets()[deviceSetIndex]->getSpectrum();
    }

    m_spectrumTee->attach(deviceSet->m_spectrumVis, original);
    m_spectrumCenter = -1; // force the scale update
}

// ---------------------------------------------------------------------------
// Periodic update

void RBSWindow::poll()
{
    m_statusClock->setText(QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " UTC");

    bool hasDevice = m_controller->hasDevice();
    bool running = m_controller->isDeviceRunning();

    m_play->blockSignals(true);
    m_play->setChecked(running);
    m_play->setText(running ? "■" : "▶");
    m_play->blockSignals(false);

    m_deviceStatus->setText(QString("■ %1").arg(m_device->currentText().section("  ·", 0, 0)));

    if (!hasDevice) {
        m_deviceStatus->setStyleSheet(m_controller->getState() == RBSController::Error ? "color: #ef4444;" : "color: #7f8a98;");
        return;
    }

    m_deviceStatus->setStyleSheet(running ? "color: #3ecf7a;" : "color: #f2c94c;");

    if (running) {
        attachSpectrum(); // late start, or the device set was recreated
    }

    // Frequency, unless the mouse is on the dial
    if (!m_frequency->isInteracting())
    {
        qint64 tuned = m_controller->getTunedFrequency();

        if ((tuned > 0) && (tuned != m_frequency->value()))
        {
            m_updatingControls = true;
            m_frequency->setValue(tuned);
            m_updatingControls = false;
            const RBSMode& mode = m_controller->getMode();
            m_bandButton->setText(mode.bandOf(tuned) + "  ▾");
            m_decoder->setText(QString("Décodeur %1 sur %2").arg(mode.m_name).arg(RBSBandPopup::formatMHz(tuned)));
        }
    }

    // Spectrum scale follows the device
    qint64 center = m_controller->getCenterFrequency();
    int rate = m_controller->getSampleRate();

    if ((center != m_spectrumCenter) || (rate != m_spectrumRate))
    {
        m_spectrumCenter = center;
        m_spectrumRate = rate;
        m_spectrum->setCenterFrequency(center);
        m_spectrum->setSampleRate(rate);
        applySpectrumSettings();   // waterfall speed depends on the rate
        applyZoom();
        m_statusRate->setText(rate >= 1000000 ? QString("%1 MS/s").arg(rate / 1e6, 0, 'f', 3) : QString("%1 kS/s").arg(rate / 1e3, 0, 'f', 1));
    }

    // Channel level
    int channelIndex = m_controller->getChannelIndex();
    double powerDB;

    if ((channelIndex >= 0) && ChannelWebAPIUtils::getChannelReportValue(m_controller->getDeviceSetIndex(), channelIndex, "channelPowerDB", powerDB))
    {
        m_level->setValue((int) std::max(-130.0, std::min(0.0, powerDB)));
        m_levelText->setText(QString("%1 dB").arg(powerDB, 0, 'f', 1));
    }
    else
    {
        m_level->setValue(-130);
        m_levelText->setText("—");
    }
}

// ---------------------------------------------------------------------------
// Decodes

double RBSWindow::distanceKm(float latitude, float longitude) const
{
    float homeLatitude = MainCore::instance()->getSettings().getLatitude();
    float homeLongitude = MainCore::instance()->getSettings().getLongitude();

    if ((homeLatitude == 0.0f) && (homeLongitude == 0.0f)) {
        return -1.0;
    }

    // Haversine
    double lat1 = qDegreesToRadians((double) homeLatitude);
    double lat2 = qDegreesToRadians((double) latitude);
    double dLat = lat2 - lat1;
    double dLon = qDegreesToRadians((double) longitude - (double) homeLongitude);
    double a = qSin(dLat / 2) * qSin(dLat / 2) + qCos(lat1) * qCos(lat2) * qSin(dLon / 2) * qSin(dLon / 2);
    return 6371.0 * 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
}

double RBSWindow::distanceKm(const QString& locator) const
{
    float latitude, longitude;

    if ((locator.size() >= 4) && (locator != "RR73") && Maidenhead::fromMaidenhead(locator, latitude, longitude)) {
        return distanceKm(latitude, longitude);
    }

    return -1.0;
}

static QString formatKm(double km)
{
    return km < 0 ? QString() : QString("%1 km").arg(QLocale().toString(qRound(km)));
}

QString RBSWindow::countryOf(const QString& callsign)
{
    QString base = callsign.section('-', 0, 0).trimmed(); // APRS SSID

    if (base.isEmpty() || base.startsWith("<")) {
        return QString();
    }

    return Callsign::instance()->getCountryInfo(base).country;
}

void RBSWindow::addStation(const QString& call, const QString& call1, const QString& message, int snr, double km)
{
    QString country = countryOf(call);

    if (country.isEmpty()) {
        country = "Inconnu";
    }

    bool newCountry = !m_stationsByCountry.contains(country);

    if (newCountry) {
        m_countryFirstHeard.insert(country, QDateTime::currentDateTime());
    }

    Station& station = m_stationsByCountry[country][call];
    station.m_lastHeard = QDateTime::currentDateTime();
    station.m_lastCall1 = call1;
    station.m_lastMessage = message;
    station.m_messages++;
    station.m_bestSNR = std::max(station.m_bestSNR, snr);

    if (km >= 0) {
        station.m_km = km;
    }

    m_countryTreeDirty = true;
    m_statsCountries->setText(QString::number(m_stationsByCountry.size()));

    if (newCountry && (country != "Inconnu")) {
        addEvent(QString("Nouveau pays : %1 (%2)").arg(country).arg(call));
    }
    if (newCountry) {
        updateCountryBadge();
    }
}

// A country is new for 5 minutes after it is first heard, or until the user opens it
bool RBSWindow::isNewCountry(const QString& country) const
{
    if ((country == "Inconnu") || m_seenCountries.contains(country)) {
        return false;
    }

    QDateTime firstHeard = m_countryFirstHeard.value(country);
    return firstHeard.isValid() && (firstHeard.secsTo(QDateTime::currentDateTime()) < 5 * 60);
}

void RBSWindow::updateCountryBadge()
{
    int countries = m_stationsByCountry.size() - (m_stationsByCountry.contains("Inconnu") ? 1 : 0);
    int newCountries = 0;

    for (auto it = m_stationsByCountry.constBegin(); it != m_stationsByCountry.constEnd(); ++it) {
        newCountries += isNewCountry(it.key()) ? 1 : 0;
    }

    m_groupByCountry->setText(countries > 0 ? QString("Regrouper par pays (%1)").arg(countries) : QString("Regrouper par pays"));
    m_newCountryBadge->setText(QString("● %1 nouveau%2").arg(newCountries).arg(newCountries > 1 ? "x" : ""));
    m_newCountryBadge->setVisible(newCountries > 0);
}

void RBSWindow::rebuildCountryTree()
{
    if (!m_countryTreeDirty || !m_groupByCountry->isChecked()) {
        return;
    }

    m_countryTreeDirty = false;
    QString search = m_search->text().trimmed();

    // Keep what the user opened
    QSet<QString> expanded;

    for (int i = 0; i < m_countryTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *item = m_countryTree->topLevelItem(i);

        if (item->isExpanded()) {
            expanded.insert(item->data(0, RBSCountryDelegate::KeyRole).toString());
        }
    }

    int scroll = m_countryTree->verticalScrollBar()->value();
    m_countryTree->setSortingEnabled(false);
    m_countryTree->clear();
    QFont bold = m_countryTree->font();
    bold.setBold(true);

    for (auto country = m_stationsByCountry.constBegin(); country != m_stationsByCountry.constEnd(); ++country)
    {
        QTreeWidgetItem *countryItem = new QTreeWidgetItem();
        countryItem->setData(0, RBSCountryDelegate::KeyRole, country.key());
        int messages = 0;
        int best = -100;
        double farthest = -1.0;

        for (auto station = country.value().constBegin(); station != country.value().constEnd(); ++station)
        {
            if (!search.isEmpty() && !country.key().contains(search, Qt::CaseInsensitive) && !station.key().contains(search, Qt::CaseInsensitive)) {
                continue;
            }

            QTreeWidgetItem *stationItem = new QTreeWidgetItem(countryItem);
            stationItem->setText(0, station.key());
            stationItem->setFont(0, bold);
            stationItem->setData(1, Qt::DisplayRole, station.value().m_messages);
            stationItem->setText(2, station.value().m_lastHeard.toString("HH:mm"));
            stationItem->setText(3, station.value().m_lastCall1);
            stationItem->setText(4, station.value().m_lastMessage);
            stationItem->setData(5, Qt::DisplayRole, station.value().m_bestSNR);
            stationItem->setText(6, formatKm(station.value().m_km));
            messages += station.value().m_messages;
            best = std::max(best, station.value().m_bestSNR);
            farthest = std::max(farthest, station.value().m_km);
        }

        if (countryItem->childCount() == 0)
        {
            delete countryItem;
            continue;
        }

        // Drawn as a box by RBSCountryDelegate; the columns are kept for sorting
        QStringList summary = {QString("%1 message%2").arg(messages).arg(messages > 1 ? "s" : ""), QString("meilleur %1 dB").arg(best)};

        if (farthest >= 0) {
            summary.append(formatKm(farthest));
        }

        countryItem->setText(0, country.key());
        countryItem->setData(0, RBSCountryDelegate::StationsRole, countryItem->childCount());
        countryItem->setData(0, RBSCountryDelegate::SummaryRole, summary.join("   ·   "));
        countryItem->setData(0, RBSCountryDelegate::NewRole, isNewCountry(country.key()));
        countryItem->setData(1, Qt::DisplayRole, messages);
        countryItem->setData(5, Qt::DisplayRole, best);
        countryItem->setData(6, Qt::DisplayRole, farthest);
        m_countryTree->addTopLevelItem(countryItem);
        countryItem->setFirstColumnSpanned(true);
        countryItem->setExpanded(expanded.contains(country.key()));
    }

    m_countryTree->setSortingEnabled(true);
    m_countryTree->verticalScrollBar()->setValue(scroll);
}

void RBSWindow::onFT8Decode(const RBSFT8Log::Decode& decode)
{
    QString myCall = MainCore::instance()->getSettings().getStationName().trimmed().toUpper();
    RBSDecodeModel::Kind kind = RBSDecodeModel::Normal;

    if (!myCall.isEmpty() && (decode.m_call1.toUpper() == myCall)) {
        kind = RBSDecodeModel::Mine;
    }

    bool isLocator = (decode.m_loc.size() == 4) && (decode.m_loc != "RR73") && Maidenhead::isMaidenhead(decode.m_loc);
    double km = isLocator ? distanceKm(decode.m_loc) : -1.0;
    QString country = countryOf(decode.m_call2);

    m_decodeModel->addRow({
        decode.m_dateTime.toString("HH:mm:ss"),
        QString::number(decode.m_snr),
        QString::number(decode.m_dt, 'f', 1),
        QString::number(decode.m_df),
        decode.m_call1,
        decode.m_call2,
        decode.m_loc,
        country,
        isLocator ? decode.m_loc : QString(),
        formatKm(km)
    }, kind);

    m_messageCount++;
    m_statsMessages->setText(QString::number(m_messageCount));

    if (!decode.m_call2.isEmpty() && !decode.m_call2.startsWith("<"))
    {
        m_callsToday.insert(decode.m_call2);
        m_statsCalls->setText(QString::number(m_callsToday.size()));
        addStation(decode.m_call2, decode.m_call1, decode.m_loc, decode.m_snr, km);
    }

    if (km > m_farthestKm)
    {
        m_farthestKm = km;
        m_statsFarthest->setText(QString("%1 · %2 · %3").arg(decode.m_call2).arg(formatKm(km)).arg(country));
        addEvent(QString("Plus loin : %1 à %2 (%3)").arg(decode.m_call2).arg(formatKm(km)).arg(country));
    }

    if (kind == RBSDecodeModel::Mine) {
        addEvent(QString("Message pour toi : %1 %2 %3").arg(decode.m_call1).arg(decode.m_call2).arg(decode.m_loc));
    }
}

void RBSWindow::onPackets()
{
    QJsonObject result = m_dataFeed->getPackets(QString(), "PacketDemod", 200, m_lastPacketSequence);
    QJsonArray packets = result.value("packets").toArray();

    for (const auto& value : packets)
    {
        QJsonObject packet = value.toObject();
        m_lastPacketSequence = std::max(m_lastPacketSequence, (qint64) packet.value("sequence").toDouble());

        if (m_controller->getMode().m_view != "aprs") {
            continue;
        }

        QJsonObject decoded = packet.value("decoded").toObject();
        QString from = decoded.value("from").toString();

        if (from.isEmpty()) {
            continue;
        }

        QJsonObject aprs = decoded.value("aprs").toObject();
        QString position;
        double km = -1.0;

        if (aprs.contains("latitude"))
        {
            double latitude = aprs.value("latitude").toDouble();
            double longitude = aprs.value("longitude").toDouble();
            position = QString("%1%2 %3%4")
                .arg(std::abs(latitude), 0, 'f', 3).arg(latitude >= 0 ? "N" : "S")
                .arg(std::abs(longitude), 0, 'f', 3).arg(longitude >= 0 ? "E" : "W");
            km = distanceKm((float) latitude, (float) longitude);
        }

        QString comment = aprs.value("comment").toString();

        if (comment.isEmpty()) {
            comment = aprs.value("status").toString();
        }
        if (comment.isEmpty()) {
            comment = decoded.value("data").toString();
        }

        QDateTime time = QDateTime::fromString(packet.value("time").toString(), Qt::ISODateWithMs);
        bool isNew = !m_callsToday.contains(from);
        QString country = countryOf(from);

        m_decodeModel->upsertRow(from, {
            time.toLocalTime().toString("HH:mm:ss"),
            from,
            country,
            decoded.value("to").toString(),
            decoded.value("via").toString(),
            position,
            formatKm(km),
            comment
        }, isNew ? RBSDecodeModel::Highlight : RBSDecodeModel::Normal);

        m_messageCount++;
        m_statsMessages->setText(QString::number(m_messageCount));
        m_callsToday.insert(from);
        m_statsCalls->setText(QString::number(m_callsToday.size()));
        addStation(from, decoded.value("to").toString(), comment, 0, km);

        if (isNew) {
            addEvent(QString("Nouvelle station : %1").arg(from));
        }

        if (km > m_farthestKm)
        {
            m_farthestKm = km;
            m_statsFarthest->setText(QString("%1 · %2").arg(from).arg(formatKm(km)));
        }
    }
}

void RBSWindow::refreshAircraft()
{
    if (m_controller->getMode().m_view != "adsb") {
        return;
    }

    QJsonObject result = m_dataFeed->getMapItems("ADSBDemod", QString(), 1000, false);
    QJsonArray items = result.value("items").toArray();

    for (const auto& value : items)
    {
        QJsonObject item = value.toObject();
        QString icao = item.value("name").toString();

        if (icao.isEmpty()) {
            continue;
        }

        QString callsign = item.value("label").toString().trimmed();
        double latitude = item.value("latitude").toDouble();
        double longitude = item.value("longitude").toDouble();
        double km = ((latitude != 0.0) || (longitude != 0.0)) ? distanceKm((float) latitude, (float) longitude) : -1.0;
        int altitudeFeet = qRound(item.value("altitude").toDouble() * 3.28084);
        QDateTime updated = QDateTime::fromString(item.value("updated").toString(), Qt::ISODateWithMs);
        QString info = item.value("text").toString().section('\n', 1, 2).simplified();
        bool isNew = !m_callsToday.contains(icao);

        m_decodeModel->upsertRow(icao, {
            updated.toLocalTime().toString("HH:mm:ss"),
            callsign.isEmpty() ? icao : callsign,
            icao,
            altitudeFeet > 0 ? QString("%1 ft").arg(QLocale().toString(altitudeFeet)) : QString(),
            QString("%1°").arg(qRound(item.value("heading").toDouble())),
            formatKm(km),
            info
        }, isNew ? RBSDecodeModel::Highlight : RBSDecodeModel::Normal);

        if (isNew)
        {
            m_callsToday.insert(icao);
            m_statsCalls->setText(QString::number(m_callsToday.size()));
        }

        if (km > m_farthestKm)
        {
            m_farthestKm = km;
            m_statsFarthest->setText(QString("%1 · %2").arg(callsign.isEmpty() ? icao : callsign).arg(formatKm(km)));
            addEvent(QString("Portée max : %1").arg(formatKm(km)));
        }
    }

    m_statsMessages->setText(QString::number(items.size()) + " en vue");
}

void RBSWindow::addEvent(const QString& text)
{
    m_events->insertItem(0, QString("%1  %2").arg(QDateTime::currentDateTime().toString("HH:mm")).arg(text));

    while (m_events->count() > 200) {
        delete m_events->takeItem(m_events->count() - 1);
    }
}

void RBSWindow::exportCSV()
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString suggested = QDir(documents).absoluteFilePath(QString("rbs_%1_%2.csv")
        .arg(m_controller->getMode().m_id)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmm")));
    QString fileName = QFileDialog::getSaveFileName(this, "Exporter le journal", suggested, "CSV (*.csv)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write("\xEF\xBB\xBF"); // BOM so that Excel reads the accents
        file.write(m_decodeModel->toCSV().toUtf8());
        addEvent(QString("Journal exporté : %1").arg(QFileInfo(fileName).fileName()));
    }
    else
    {
        addEvent(QString("Erreur : impossible d'écrire %1").arg(fileName));
    }
}

void RBSWindow::copySelection()
{
    QStringList lines;
    const QModelIndexList rows = m_decodeTable->selectionModel()->selectedRows();

    for (const auto& row : rows)
    {
        QStringList cells;

        for (int c = 0; c < m_decodeFilter->columnCount(); c++) {
            cells.append(m_decodeFilter->data(m_decodeFilter->index(row.row(), c)).toString());
        }

        lines.append(cells.join('\t'));
    }

    if (!lines.isEmpty()) {
        QApplication::clipboard()->setText(lines.join('\n'));
    }
}
