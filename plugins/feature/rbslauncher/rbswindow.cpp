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
#include <QRegularExpression>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
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
#include "dsp/channelmarker.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "util/callsign.h"
#include "util/maidenhead.h"
#include "maincore.h"
#include "mainwindow.h"

#include "mcpdatafeed.h"
#include "rbsbandpopup.h"
#include "rbscwtap.h"
#include "rbsdelegates.h"
#include "rbsfrequencydial.h"
#include "rbsmeters.h"
#include "rbsmodepopup.h"
#include "rbssection.h"
#include "rbsspectrumtee.h"
#include "rbswindow.h"

// RBS settings live apart from SDRangel's: MainSettings::save() removes every group but its own
static const char* const m_settingsApplication = "RBS";
static const char* const m_settingsGroup = "RBSLauncher";
static constexpr int m_sidebarWidth = 310;      //!< Fixed, as the SDR++ menu
static constexpr int m_rightbarWidth = 290;     //!< Decoder panel, as the MultiPSK Deck
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
RBSWindow { background: #0e0e10; }
QToolBar#rbsTopBarToolbar { background: #18181b; border: none; border-bottom: 1px solid #35353b; padding: 0; spacing: 0; }
QWidget#rbsTopBar { background: #18181b; }
QToolButton#rbsSectionHeader { background: #212125; border: none; border-bottom: 1px solid #35353b; padding: 5px 6px; color: #e3e3e6; text-align: left; }
QWidget#rbsSectionBody { background: #18181b; border-bottom: 1px solid #35353b; }
QWidget#rbsPanel, QScrollArea, QScrollArea > QWidget > QWidget { background: #18181b; border: none; }
QScrollArea#rbsSidebar { border-right: 1px solid #35353b; }
QWidget#rbsPanelHeader { background: #212125; border-top: 1px solid #35353b; border-bottom: 1px solid #35353b; }
QTabWidget::pane { border: none; background: #0e0e10; }
QTabBar { background: #18181b; }
QTabBar::tab { background: #18181b; color: #9696a0; padding: 7px 28px; border: none; border-right: 1px solid #35353b; border-bottom: 1px solid #35353b; min-width: 140px; }
QTabBar::tab:selected { color: #ffffff; background: #0e0e10; border-bottom: 2px solid #3d8bff; }
QTabBar::tab:hover { color: #e3e3e6; }
QTableView, QTreeWidget { background: #18181b; alternate-background-color: #1c1c20; gridline-color: #26262b; border: none; selection-background-color: #1b3a66; selection-color: #ffffff; }
QHeaderView::section { background: #18181b; color: #9696a0; border: none; border-bottom: 1px solid #35353b; padding: 3px 8px; }
QLineEdit, QSpinBox { background: #18181b; border: 1px solid #35353b; border-radius: 0; padding: 2px 6px; color: #e3e3e6; min-height: 18px; }
QComboBox { combobox-popup: 0; background: #2a2a2f; border: 1px solid #3d3d44; border-radius: 3px; padding: 3px 8px; color: #e3e3e6; min-height: 18px; }
QComboBox:hover, QComboBox:on { border-color: #3d8bff; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; border: none; width: 22px; }
QComboBox::down-arrow { image: url(:/rbslauncher/images/combo_arrow.png); width: 10px; height: 6px; }
QComboBox QAbstractItemView { background: #2a2a2f; border: 1px solid #45454c; outline: 0; padding: 2px 0; color: #e3e3e6; }
QComboBox QAbstractItemView::item { min-height: 26px; padding: 0 10px; }
QComboBox QAbstractItemView::item:hover { background: #35353b; }
QComboBox QAbstractItemView::item:selected { background: #3d8bff; color: #ffffff; }
QScrollArea#rbsRightbar { border-left: 1px solid #35353b; }
QLabel#rbsModeTitle { font-size: 20px; font-weight: bold; color: #ffffff; padding: 2px 0 4px 0; }
QWidget#rbsDecodeHeader { background: #212125; border-bottom: 1px solid #35353b; }
QWidget#rbsDecodeHeader QToolButton { border: none; border-left: 1px solid #35353b; background: transparent; padding: 7px 12px; color: #e3e3e6; }
QWidget#rbsDecodeHeader QToolButton:hover { background: #2a2a2f; }
QWidget#rbsDecodeHeader QToolButton:checked { background: #3d8bff; color: #ffffff; }
QTabBar#rbsDecodeTabs::tab { background: transparent; color: #9696a0; padding: 7px 14px; border: none; border-right: 1px solid #35353b; min-width: 0px; font-weight: bold; }
QTabBar#rbsDecodeTabs::tab:selected { background: #18181b; color: #e3e3e6; border-top: 2px solid #3d8bff; }
QTabBar#rbsDecodeTabs::tab:hover:!selected { color: #e3e3e6; }
QLabel#rbsDecodeCount { color: #9696a0; padding: 0 6px; }
QTextEdit#rbsCwText { background: #18181b; border: none; color: #e3e3e6; padding: 6px; selection-background-color: #1b3a66; }
QToolButton#rbsBandButton { border: none; background: transparent; padding: 0; text-align: left; font-size: 15px; font-weight: bold; }
QToolButton#rbsBandButton:hover { color: #3d8bff; }
QLabel#rbsNewCountry { color: #ef4444; }
QToolButton { border: 1px solid #35353b; background: #212125; padding: 3px 10px; border-radius: 0; color: #e3e3e6; }
QToolButton:hover { border-color: #3d8bff; }
QToolButton:checked { background: #3d8bff; border-color: #3d8bff; color: #ffffff; }
QToolButton#rbsPlay { min-width: 52px; min-height: 42px; font-size: 24px; padding: 0; }
QToolButton#rbsMenu { min-width: 36px; min-height: 42px; font-size: 20px; padding: 0; border: none; background: transparent; }
QToolButton::menu-indicator { image: none; }
QMenu { background: #18181b; border: 1px solid #35353b; color: #e3e3e6; }
QMenu::item { padding: 6px 18px; }
QMenu::item:selected { background: #1b3a66; }
QProgressBar { background: #212125; border: none; max-height: 6px; }
QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3ecf7a, stop:0.8 #f2c94c, stop:1 #ef4444); }
QCheckBox, QRadioButton { color: #e3e3e6; spacing: 6px; }
QCheckBox::indicator { width: 11px; height: 11px; border: 1px solid #55555d; background: #18181b; }
QCheckBox::indicator:checked { background: #3d8bff; border-color: #3d8bff; }
QRadioButton::indicator { width: 11px; height: 11px; border: 1px solid #55555d; border-radius: 6px; background: #18181b; }
QRadioButton::indicator:checked { background: #3d8bff; border-color: #3d8bff; }
QRadioButton:disabled, QCheckBox:disabled, QLabel:disabled { color: #55555d; }
QListWidget { background: #18181b; border: none; outline: 0; }
QListWidget::item { padding: 3px 8px; border-left: 2px solid transparent; color: #e3e3e6; }
QListWidget::item:selected { background: #13213a; border-left: 2px solid #3d8bff; color: #e3e3e6; }
QListWidget::item:disabled { color: #9696a0; }
QStatusBar { background: #0e0e10; color: #9696a0; border-top: 1px solid #35353b; }
QStatusBar QLabel { color: #9696a0; padding: 0 8px; }
QStatusBar::item { border: none; }
QSlider { min-height: 20px; }
QSlider::groove:horizontal { height: 8px; border-radius: 4px; background: #e6e7ea; }
QSlider::sub-page:horizontal { height: 8px; border-radius: 4px; background: #3d8bff; }
QSlider::add-page:horizontal { height: 8px; border-radius: 4px; background: #e6e7ea; }
QSlider::handle:horizontal { width: 18px; height: 18px; margin: -5px 0; border-radius: 9px; background: #ffffff; border: 1px solid #c9ccd2; }
QSlider::handle:horizontal:hover { border-color: #3d8bff; }
QSlider::sub-page:horizontal:disabled { background: #55555d; }
QSlider::add-page:horizontal:disabled, QSlider::groove:horizontal:disabled { background: #3d3d44; }
QSlider::handle:horizontal:disabled { background: #9696a0; border-color: #55555d; }
QSlider#rbsScaleSlider { min-width: 20px; }
QSlider#rbsScaleSlider::groove:vertical { width: 8px; border-radius: 4px; background: #e6e7ea; }
QSlider#rbsScaleSlider::add-page:vertical { width: 8px; border-radius: 4px; background: #3d8bff; }
QSlider#rbsScaleSlider::sub-page:vertical { width: 8px; border-radius: 4px; background: #e6e7ea; }
QSlider#rbsScaleSlider::handle:vertical { width: 18px; height: 18px; margin: 0 -5px; border-radius: 9px; background: #ffffff; border: 1px solid #c9ccd2; }
QSlider#rbsScaleSlider::handle:vertical:hover { border-color: #3d8bff; }
QLabel { color: #e3e3e6; }
QLabel[rbsDim="true"] { color: #9696a0; }
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
    m_spectrumTee(nullptr),
    m_displayFFTSize(4096),
    m_displayLinesPerSecond(12),
    m_displaySmoothing(0),
    m_scaleFloor(-130.0),
    m_snrAverage(-100.0),
    m_frozen(false),
    m_cwMinConfidence(30),
    m_deckWordStart(-1),
    m_deckWordConfidence(1.0f),
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

    // RBS Morse decoder, fed with the decoder channel's audio in Morse
    m_cwTap = new RBSCWTap(this);
    connect(m_cwTap, &RBSCWTap::decoded, this, &RBSWindow::onCWDecoded);

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
    m_decoderPanel = createDecoderPanel();
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
    m_cwTap->attach(nullptr);
    m_spectrum->removeChannelMarker(m_marker);
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

    // Frequency: none with a sound card (the audio comes as it is), the dial is hidden then
    m_frequencyBox = new QWidget(bar);
    QHBoxLayout *frequencyLayout = new QHBoxLayout(m_frequencyBox);
    frequencyLayout->setContentsMargins(0, 0, 0, 0);
    m_frequency = new RBSFrequencyDial(m_frequencyBox);
    m_frequency->setRange(0, 2000000000LL);   // RSP1B: up to 2 GHz
    connect(m_frequency, &RBSFrequencyDial::changed, this, [this](qint64 value) {
        if (!m_updatingControls) {
            tuneTo(value);
        }
    });
    frequencyLayout->addWidget(m_frequency);
    frequencyLayout->addWidget(dimLabel("Hz", m_frequencyBox));
    layout->addWidget(m_frequencyBox);

    // Mode: opens the mode selector (categories, modes), as the MultiPSK Deck
    QVBoxLayout *modeBox = new QVBoxLayout();
    modeBox->setSpacing(0);
    modeBox->addWidget(dimLabel("Mode", bar));
    m_modeButton = new QToolButton(bar);
    m_modeButton->setObjectName("rbsBandButton");
    m_modeButton->setText("—  ▾");
    m_modeButton->setCursor(Qt::PointingHandCursor);
    m_modeButton->setToolTip("Choisir le mode : FT8, Morse, APRS, ADS-B…");
    modeBox->addWidget(m_modeButton);
    layout->addLayout(modeBox);
    m_modePopup = new RBSModePopup(this);
    connect(m_modeButton, &QToolButton::clicked, this, [this]() {
        m_modePopup->showFor(m_modeButton, m_modes, m_controller->getMode().m_id);
    });
    connect(m_modePopup, &RBSModePopup::modeChosen, this, [this](const QString& modeId) {
        startMode(modeId);
    });

    // Band: opens the known frequencies of the mode
    m_bandBox = new QWidget(bar);
    QVBoxLayout *bandBox = new QVBoxLayout(m_bandBox);
    bandBox->setContentsMargins(0, 0, 0, 0);
    bandBox->setSpacing(0);
    bandBox->addWidget(dimLabel("Bande", m_bandBox));
    m_bandButton = new QToolButton(m_bandBox);
    m_bandButton->setObjectName("rbsBandButton");
    m_bandButton->setText("—  ▾");
    m_bandButton->setCursor(Qt::PointingHandCursor);
    m_bandButton->setToolTip("Choisir une fréquence connue du mode");
    bandBox->addWidget(m_bandButton);
    layout->addWidget(m_bandBox);
    m_bandPopup = new RBSBandPopup(this);
    connect(m_bandButton, &QToolButton::clicked, this, [this]() {
        m_bandPopup->showFor(m_bandButton, m_controller->getMode(), m_frequency->value());
    });
    connect(m_bandPopup, &RBSBandPopup::frequencyChosen, this, &RBSWindow::tuneTo);

    // S/N, level and quality, as the MultiPSK Deck
    m_meters = new RBSMeters(bar);
    layout->addWidget(m_meters);

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

    // --- Display: waterfall speed and smoothing, remembered per mode (the resolution is the mode's)
    RBSSection *display = new RBSSection("Affichage", panel);
    m_waterfallSpeed = new QSlider(Qt::Horizontal);
    m_waterfallSpeed->setRange(1, 50);
    m_waterfallSpeed->setToolTip("Lignes de cascade par seconde (et rafraîchissement du spectre). Plus lent = signaux faibles plus visibles");
    m_waterfallSpeedText = new QLabel();
    display->form()->addRow(dimLabel("Vitesse cascade"), sliderRow(m_waterfallSpeed, m_waterfallSpeedText));
    m_smoothing = new QSlider(Qt::Horizontal);
    m_smoothing->setRange(0, 95);
    m_smoothing->setToolTip("Lissage du spectre et de la cascade : moyenne des derniers spectres. Moins de bruit, réaction plus lente");
    m_smoothingText = new QLabel();
    display->form()->addRow(dimLabel("Lissage"), sliderRow(m_smoothing, m_smoothingText));
    layout->addWidget(display);

    connect(m_waterfallSpeed, &QSlider::valueChanged, this, [this](int value) {
        m_waterfallSpeedText->setText(QString("%1 /s").arg(value));
        if (m_spectrumTee) {
            m_spectrumTee->setLinesPerSecond(value);
        }

        if (!m_updatingControls)
        {
            m_displayLinesPerSecond = value;
            saveDisplay();
            applySpectrumSettings();
        }
    });
    connect(m_smoothing, &QSlider::valueChanged, this, [this](int value) {
        m_smoothingText->setText(QString("%1 %").arg(value));
        if (m_spectrumTee) {
            m_spectrumTee->setSmoothing(value / 100.0f);
        }

        if (!m_updatingControls)
        {
            m_displaySmoothing = value;
            saveDisplay();
        }
    });

    // Zoom around the decoder, brightness and contrast of the spectrum and waterfall. Fine steps:
    // zoom x1 to x64, brightness by 0.5 dB, contrast by 1 %. Up = more of each
    m_zoom = new QSlider(Qt::Horizontal);
    m_zoom->setRange(0, 600);
    m_zoom->setToolTip("Zoom autour de la fréquence du décodeur, jusqu'à x64");
    m_zoomText = new QLabel();
    display->form()->addRow(dimLabel("Zoom"), sliderRow(m_zoom, m_zoomText));
    m_brightness = new QSlider(Qt::Horizontal);
    m_brightness->setRange(-60, 60);
    m_brightness->setToolTip("Luminosité : vers la droite, le bruit et les signaux faibles ressortent");
    m_brightnessText = new QLabel();
    display->form()->addRow(dimLabel("Luminosité"), sliderRow(m_brightness, m_brightnessText));
    m_contrast = new QSlider(Qt::Horizontal);
    m_contrast->setRange(0, 100);
    m_contrast->setToolTip("Contraste : vers la droite, moins de dB entre le noir et le blanc, les signaux tranchent");
    m_contrastText = new QLabel();
    display->form()->addRow(dimLabel("Contraste"), sliderRow(m_contrast, m_contrastText));
    m_autoLevel = new QCheckBox("Luminosité automatique");
    m_autoLevel->setToolTip("Le bas de l'échelle suit le niveau du bruit : la cascade garde le même fond quand le bruit change");
    display->form()->addRow(m_autoLevel);

    auto showLevels = [this]() {
        m_zoomText->setText(QString("x%1").arg(std::pow(2.0, m_zoom->value() / 100.0), 0, 'f', m_zoom->value() < 400 ? 1 : 0));
        m_brightnessText->setText(QString("%1%2 dB").arg(m_brightness->value() > 0 ? "+" : "").arg(m_brightness->value() / 2.0, 0, 'f', 1));
        m_contrastText->setText(QString("%1 %").arg(m_contrast->value()));
    };
    auto levelChanged = [this, showLevels]() {
        showLevels();
        m_scaleTimer.start();

        if (!m_updatingControls) {
            saveDisplay();
        }
    };
    connect(m_brightness, &QSlider::valueChanged, this, levelChanged);
    connect(m_contrast, &QSlider::valueChanged, this, levelChanged);
    connect(m_zoom, &QSlider::valueChanged, this, [this, showLevels]() {
        showLevels();
        m_zoomTimer.start();

        if (!m_updatingControls) {
            saveDisplay();
        }
    });
    connect(m_autoLevel, &QCheckBox::toggled, this, [this]() {
        if (!m_updatingControls) {
            saveDisplay();
        }
    });

    // Waterfall colours, for all modes
    m_colors = new QComboBox();
    m_colors->addItem("Bleu (type SDR#)", "RBS Bleu");
    m_colors->addItem("Jet (type SDR++)", "Jet");
    m_colors->addItem("Turbo", "Turbo");
    m_colors->addItem("Gris", "Loggray");
    display->form()->insertRow(0, dimLabel("Couleurs"), m_colors);
    {
        QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
        settings.beginGroup(m_settingsGroup);
        int index = m_colors->findData(settings.value("colorMap", "RBS Bleu").toString());
        m_colors->setCurrentIndex(index < 0 ? 0 : index);
        settings.endGroup();
    }
    connect(m_colors, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        m_spectrum->setColorMapName(m_colors->currentData().toString());
        QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
        settings.beginGroup(m_settingsGroup);
        settings.setValue("colorMap", m_colors->currentData().toString());
        settings.endGroup();
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
    m_spectrum->setDisplayGridIntensity(22);
    m_spectrum->setDisplayTraceIntensity(90);
    // As the MultiPSK Deck: white trace over a fine grid, waterfall right under it, dB only
    m_spectrum->setSpectrumStyle(SpectrumSettings::Gradient);
    m_spectrum->setSpectrumColor(qRgb(232, 238, 248));
    m_spectrum->setCompactLayout(true);
    m_spectrum->setColorMapName(m_colors->currentData().toString());
    m_spectrum->setWaterfallShare(0.66f);   // SDR++: spectrum a third, waterfall two thirds
    m_spectrum->setReferenceLevel(-30.0f);
    m_spectrum->setPowerRange(80.0f);
    m_spectrum->setDecay(1);
    m_spectrum->setFPSPeriodMs(40);
    m_spectrum->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Keep the spectra of the waterfall, so that a zoom, a retune, a new scale or a resize redraws
    // the old waterfall (moved to its frequencies) instead of starting again from black, as SDR++.
    // Its scroll bar (going back in time) is not shown
    m_spectrum->setScrolling(true, 1200);

    if (QScrollBar *scrollBar = m_spectrum->findChild<QScrollBar*>()) {
        scrollBar->hide();
    }

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

    // Decoder passband. Alone on the spectrum, a click anywhere moves it there (and drag, wheel),
    // when the mode allows it (CW)
    m_marker = new ChannelMarker(this);
    m_marker->setColor(QColor(61, 139, 255));
    m_marker->setMovable(false);
    m_marker->setVisible(false);
    m_spectrum->addChannelMarker(m_marker);
    connect(m_marker, &ChannelMarker::changedByCursor, this, &RBSWindow::onMarkerMoved);

    // Zoom, brightness and contrast are in the Display section. A slider move redraws the whole
    // waterfall: the redraws are grouped, at most one every 20 ms, so that dragging stays smooth
    m_scaleTimer.setSingleShot(true);
    m_scaleTimer.setInterval(20);
    connect(&m_scaleTimer, &QTimer::timeout, this, &RBSWindow::applyScale);
    m_zoomTimer.setSingleShot(true);
    m_zoomTimer.setInterval(20);
    connect(&m_zoomTimer, &QTimer::timeout, this, &RBSWindow::applyZoom);

    return panel;
}

// ---------------------------------------------------------------------------
// Decodes

QWidget *RBSWindow::createDecodes()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = noMargins(new QVBoxLayout(panel));

    // Header as the MultiPSK Deck: tabs on the left, count and actions on the right
    QWidget *tools = new QWidget(panel);
    tools->setObjectName("rbsDecodeHeader");
    QHBoxLayout *toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(0);
    m_decodesTitle = new QLabel("Décodages", tools);  // name of the decodes tab in the tabs layout
    m_decodesTitle->hide();
    m_decodeTabs = new QTabBar(tools);
    m_decodeTabs->setObjectName("rbsDecodeTabs");
    m_decodeTabs->setDrawBase(false);
    m_decodeTabs->setExpanding(false);
    m_decodeTabs->addTab("TABLEAU");
    m_decodeTabs->addTab("TEXTE");
    m_decodeTabs->setTabToolTip(0, "Les décodages en colonnes");
    m_decodeTabs->setTabToolTip(1, "Le texte décodé, ligne par ligne (le Morse s'y écrit lettre par lettre)");
    m_newCountryBadge = new QLabel(tools);
    m_newCountryBadge->setObjectName("rbsNewCountry");
    m_newCountryBadge->setToolTip("Pays entendus pour la première fois : ouvre-les dans la vue par pays");
    m_newCountryBadge->hide();
    m_decodeCount = new QLabel("0 / 0 messages", tools);
    m_decodeCount->setObjectName("rbsDecodeCount");
    m_groupByCountry = new QToolButton(tools);
    m_groupByCountry->setText("Grouper par pays");
    m_groupByCountry->setCheckable(true);
    m_freeze = new QToolButton(tools);
    m_freeze->setText("Figer");
    m_freeze->setCheckable(true);
    m_freeze->setToolTip("Arrête l'affichage des nouveaux décodages pour lire tranquillement ; ils arrivent quand tu relâches");
    QToolButton *copy = new QToolButton(tools);
    copy->setText("Copier");
    QToolButton *exportButton = new QToolButton(tools);
    exportButton->setText("Exporter");
    QToolButton *clear = new QToolButton(tools);
    clear->setText("Effacer");
    toolsLayout->addWidget(m_decodeTabs);
    toolsLayout->addSpacing(10);
    toolsLayout->addWidget(m_newCountryBadge);
    toolsLayout->addStretch(1);
    toolsLayout->addWidget(m_decodeCount);
    toolsLayout->addSpacing(10);
    toolsLayout->addWidget(m_groupByCountry);
    toolsLayout->addWidget(m_freeze);
    toolsLayout->addWidget(copy);
    toolsLayout->addWidget(exportButton);
    toolsLayout->addWidget(clear);
    layout->addWidget(tools);

    // Shown in the decoder panel (right)
    m_search = new QLineEdit();
    m_search->setPlaceholderText("F4, JN25, Italy…");
    m_search->setClearButtonEnabled(true);
    m_cwStats = dimLabel(QString());
    m_cwStats->setToolTip("Tonalité, vitesse et rapport signal / bruit trouvés par le décodeur Morse");
    m_distanceReference = dimLabel(QString());
    m_distanceReference->setWordWrap(true);

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

    // Column titles as the MultiPSK Deck: small capitals, spaced
    QFont headerFont = m_decodeTable->horizontalHeader()->font();
    headerFont.setBold(true);
    headerFont.setCapitalization(QFont::AllUppercase);
    headerFont.setLetterSpacing(QFont::PercentageSpacing, 108);
    headerFont.setPointSizeF(headerFont.pointSizeF() * 0.88);
    m_decodeTable->horizontalHeader()->setFont(headerFont);
    m_countryTree->header()->setFont(headerFont);

    // TEXTE tab: the decodes as text, the Morse written as it comes
    m_cwText = new QTextEdit(panel);
    m_cwText->setObjectName("rbsCwText");
    m_cwText->setReadOnly(true);
    m_cwText->setUndoRedoEnabled(false);
    QFont mono("Liberation Mono");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(11);
    m_cwText->setFont(mono);

    // MORSE DECK tab, in Morse instead of the table: the text as it comes, callsigns in red, calls
    // (CQ, DE, K) in green, codes and courtesy (73, TU, 5NN, QTH) in blue
    m_morseDeck = new QTextEdit(panel);
    m_morseDeck->setObjectName("rbsCwText");
    m_morseDeck->setReadOnly(true);
    m_morseDeck->setUndoRedoEnabled(false);
    QFont deckFont = mono;
    deckFont.setPointSize(13);
    m_morseDeck->setFont(deckFont);

    m_decodePages = new QStackedWidget(panel);
    m_decodePages->addWidget(m_decodeViews);
    m_decodePages->addWidget(m_cwText);
    m_decodePages->addWidget(m_morseDeck);
    layout->addWidget(m_decodePages, 1);

    connect(m_decodeTabs, &QTabBar::currentChanged, this, [this](int index) {
        bool cw = m_controller->getMode().m_view == "cw";
        m_decodePages->setCurrentWidget(index == 1 ? (QWidget*) m_cwText : cw ? (QWidget*) m_morseDeck : (QWidget*) m_decodeViews);
    });
    connect(m_freeze, &QToolButton::toggled, this, [this](bool frozen) {
        m_frozen = frozen;

        if (!frozen)
        {
            // What came in meanwhile
            const QList<RBSFT8Log::Decode> decodes = m_frozenFT8;
            m_frozenFT8.clear();

            for (const auto& decode : decodes) {
                onFT8Decode(decode);
            }

            onPackets();
        }
    });

    connect(m_search, &QLineEdit::textChanged, m_decodeFilter, &RBSDecodeFilter::setSearch);
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        m_countryTreeDirty = true;
    });
    connect(m_groupByCountry, &QToolButton::toggled, this, [this](bool checked) {
        m_decodeTabs->setCurrentIndex(0);
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
        m_cwText->clear();
        m_morseDeck->clear();
        m_deckWordStart = -1;
        m_deckWord.clear();
        m_cwBuffer.clear();
        m_cwLastText = QDateTime();
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

// ---------------------------------------------------------------------------
// Decoder panel (right side, as the MultiPSK Deck): the mode, the options of its decoder, filters

QWidget *RBSWindow::createDecoderPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = noMargins(new QVBoxLayout(panel));

    // --- Mode
    RBSSection *mode = new RBSSection("Mode", panel);
    m_modeForm = mode->form();
    m_modeTitle = new QLabel();
    m_modeTitle->setObjectName("rbsModeTitle");
    mode->form()->addRow(m_modeTitle);
    m_modeDemod = new QLabel();
    m_modeGroup = new QLabel();
    m_modeFrequencies = new QLabel();
    mode->form()->addRow(dimLabel("Démodulation"), m_modeDemod);
    mode->form()->addRow(dimLabel("Catégorie"), m_modeGroup);
    mode->form()->addRow(dimLabel("Fréquences"), m_modeFrequencies);
    m_modeDescription = dimLabel(QString());
    m_modeDescription->setWordWrap(true);
    mode->addWidget(m_modeDescription);
    layout->addWidget(mode);

    // --- Decoder: one group of options per decode view, the one of the mode shown
    m_decoderSection = new RBSSection("Décodeur", panel);

    m_ft8Options = new QWidget();
    QVBoxLayout *ft8 = noMargins(new QVBoxLayout(m_ft8Options));
    ft8->setSpacing(5);
    m_ft8CQOnly = new QCheckBox("CQ seulement");
    m_ft8CQOnly->setToolTip("Le tableau ne garde que les appels généraux (CQ)");
    m_ft8MineOnly = new QCheckBox("Messages pour mon indicatif");
    m_ft8MineOnly->setToolTip("Le tableau ne garde que les messages adressés à l'indicatif de Ma station");
    QLabel *ft8Note = dimLabel("Fenêtre décodée : 200 à 3 000 Hz au-dessus de la fréquence, le cadre bleu du spectre.");
    ft8Note->setWordWrap(true);
    ft8->addWidget(m_ft8CQOnly);
    ft8->addWidget(m_ft8MineOnly);
    ft8->addWidget(ft8Note);
    m_decoderSection->addWidget(m_ft8Options);

    m_cwOptions = new QWidget();
    QVBoxLayout *cw = noMargins(new QVBoxLayout(m_cwOptions));
    cw->setSpacing(5);
    m_cwAuto = new QCheckBox("Tonalité et vitesse automatiques");
    m_cwAuto->setToolTip("Le décodeur suit la tonalité (200 à 3 000 Hz) et la vitesse (5 à 60 WPM). Décoche pour les figer sur les valeurs trouvées");
    m_cwAuto->setChecked(true);
    cw->addWidget(m_cwAuto);
    cw->addWidget(m_cwStats);
    QFormLayout *cwForm = new QFormLayout();
    cwForm->setContentsMargins(0, 4, 0, 0);
    cwForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_cwFilter = new QSlider(Qt::Horizontal);
    m_cwFilter->setRange(0, 90);
    m_cwFilter->setToolTip("Le texte décodé avec moins de confiance n'est pas affiché : moins de lettres au hasard dans le bruit");
    m_cwFilterText = new QLabel();
    cwForm->addRow(dimLabel("Anti-bruit"), sliderRow(m_cwFilter, m_cwFilterText));
    cw->addLayout(cwForm);
    QLabel *cwLegend = new QLabel(
        "<span style='color:#ef4444'><b>Indicatifs</b></span> · "
        "<span style='color:#3ecf7a'><b>Appels</b> (CQ, DE, K)</span> · "
        "<span style='color:#60a5fa'>Codes et politesse (73, TU, 5NN)</span>");
    cwLegend->setWordWrap(true);
    cw->addWidget(cwLegend);
    QToolButton *cwClear = new QToolButton();
    cwClear->setText("Effacer le texte");
    cw->addWidget(cwClear, 0, Qt::AlignLeft);
    m_decoderSection->addWidget(m_cwOptions);

    m_aprsOptions = new QWidget();
    QVBoxLayout *aprs = noMargins(new QVBoxLayout(m_aprsOptions));
    m_aprsPositionOnly = new QCheckBox("Stations avec position seulement");
    aprs->addWidget(m_aprsPositionOnly);
    QLabel *aprsNote = dimLabel("Paquets AX.25 à 1 200 bauds, décodés par le démodulateur de paquets de SDRangel.");
    aprsNote->setWordWrap(true);
    aprs->addWidget(aprsNote);
    m_decoderSection->addWidget(m_aprsOptions);

    m_adsbOptions = new QWidget();
    QVBoxLayout *adsb = noMargins(new QVBoxLayout(m_adsbOptions));
    m_adsbPositionOnly = new QCheckBox("Avions avec position seulement");
    adsb->addWidget(m_adsbPositionOnly);
    m_decoderSection->addWidget(m_adsbOptions);
    layout->addWidget(m_decoderSection);

    // --- Filters
    RBSSection *filters = new RBSSection("Filtres", panel);
    filters->form()->addRow(dimLabel("Chercher"), m_search);
    filters->addWidget(m_distanceReference);
    layout->addWidget(filters);

    connect(m_ft8CQOnly, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_decodeFilter->setRequirement("cq", 4, "CQ");
        } else {
            m_decodeFilter->removeRequirement("cq");
        }
    });
    connect(m_ft8MineOnly, &QCheckBox::toggled, this, [this](bool checked) {
        QString myCall = MainCore::instance()->getSettings().getStationName().trimmed().toUpper();

        if (checked && !myCall.isEmpty()) {
            m_decodeFilter->setRequirement("mine", 4, myCall, true);
        } else {
            m_decodeFilter->removeRequirement("mine");
        }
    });
    connect(m_aprsPositionOnly, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_decodeFilter->setRequirement("position", 5, QString());
        } else {
            m_decodeFilter->removeRequirement("position");
        }
    });
    connect(m_adsbPositionOnly, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_decodeFilter->setRequirement("position", 5, QString());
        } else {
            m_decodeFilter->removeRequirement("position");
        }
    });
    connect(m_cwAuto, &QCheckBox::toggled, this, [this](bool checked) {
        // Unchecked: tone and speed stay where they are now
        RBSCWDecoder::Settings settings = m_cwTap->decoder().getSettings();
        RBSCWDecoder::Status status = m_cwTap->decoder().getStatus();
        settings.m_autoSpeed = checked;
        settings.m_toneLocked = !checked;
        settings.m_lockedToneHz = status.m_toneHz;
        settings.m_manualWPM = status.m_wpm;
        m_cwTap->decoder().setSettings(settings);
        addEvent(checked ? QString("Morse : tonalité et vitesse automatiques") : QString("Morse : tonalité et vitesse figées"));
    });
    connect(m_cwFilter, &QSlider::valueChanged, this, [this](int value) {
        m_cwMinConfidence = value;
        m_cwFilterText->setText(value == 0 ? QString("aucun") : QString("%1 %").arg(value));
        QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
        settings.beginGroup(m_settingsGroup);
        settings.setValue("cwMinConfidence", value);
        settings.endGroup();
    });
    connect(cwClear, &QToolButton::clicked, this, [this]() {
        m_cwText->clear();
        m_morseDeck->clear();
        m_deckWordStart = -1;
        m_deckWord.clear();
        m_cwBuffer.clear();
        m_cwLastText = QDateTime();
    });

    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    m_cwFilter->setValue(settings.value("cwMinConfidence", 30).toInt());
    m_cwFilterText->setText(m_cwFilter->value() == 0 ? QString("aucun") : QString("%1 %").arg(m_cwFilter->value()));
    m_cwMinConfidence = m_cwFilter->value();
    settings.endGroup();

    layout->addStretch(1);
    return panel;
}

void RBSWindow::updateDecoderPanel(const RBSMode& mode)
{
    const QString& view = mode.m_view;
    m_modeTitle->setText(mode.m_name);
    m_modeDemod->setText(mode.m_demod);
    m_modeGroup->setText(mode.m_group);
    m_modeFrequencies->setText(QString("%1 connue%2").arg(mode.m_frequencies.size()).arg(mode.m_frequencies.size() > 1 ? "s" : ""));
    m_modeDescription->setText(mode.m_description);
    m_modeDescription->setVisible(!mode.m_description.isEmpty());

    m_decoderSection->setTitle(view == "ft8" ? "Décodeur FT8" : view == "cw" ? "Décodeur Morse" :
        view == "aprs" ? "Décodeur APRS" : view == "adsb" ? "Décodeur ADS-B" : "Décodeur");
    m_ft8Options->setVisible(view == "ft8");
    m_cwOptions->setVisible(view == "cw");
    m_aprsOptions->setVisible(view == "aprs");
    m_adsbOptions->setVisible(view == "adsb");

    // Filters of the previous mode: the columns are not the same
    for (QCheckBox *check : {m_ft8CQOnly, m_ft8MineOnly, m_aprsPositionOnly, m_adsbPositionOnly})
    {
        check->blockSignals(true);
        check->setChecked(false);
        check->blockSignals(false);
    }

    m_decodeFilter->clearRequirements();
    bool hasCall = !MainCore::instance()->getSettings().getStationName().trimmed().isEmpty();
    m_ft8MineOnly->setEnabled(hasCall);
    m_cwAuto->blockSignals(true);
    m_cwAuto->setChecked(true);
    m_cwAuto->blockSignals(false);
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
    m_centerSplitter->setStyleSheet("QSplitter::handle { background: #35353b; } QSplitter::handle:hover { background: #3d8bff; }");
    allInOne->addWidget(m_centerSplitter, 1);

    // Decoder panel on the right, as the MultiPSK Deck
    QScrollArea *rightbar = scrollColumn(m_allInOnePage, m_rightBox);
    rightbar->setObjectName("rbsRightbar");
    rightbar->setFixedWidth(m_rightbarWidth);
    allInOne->addWidget(rightbar);
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
    m_tabs->addTab(scrollColumn(m_tabs, m_tabDecoderBox), "Décodeur");
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
        // Right: the decoder, then today's statistics and events
        m_rightBox->insertWidget(0, m_decoderPanel);
        m_rightBox->insertWidget(1, m_infoPanel);
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
        m_tabDecoderBox->insertWidget(0, m_decoderPanel);
        m_pages->setCurrentWidget(m_tabs);
    }

    m_spectrumPanel->show();
    m_decodesPanel->show();
    m_controlsPanel->show();
    m_infoPanel->show();
    m_decoderPanel->show();

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
    m_cwTap->attach(nullptr);
    updateSourceKind();

    // The frequency chosen last for this mode (band selector, dial) instead of the sheet's one
    RBSMode started = *mode;
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    qint64 tuned = settings.value("tuned_" + mode->m_id, 0).toLongLong();
    settings.endGroup();

    if (tuned > 0) {
        started.m_device.insert("centerFrequency", (double) (tuned - started.m_channelOffset - started.m_tuneShift));
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
    setupMarker(started);
    m_modeButton->setText(started.m_name + "  ▾");
    emit modeChanged(started.m_id);
}

void RBSWindow::showModeSettings(const RBSMode& mode)
{
    if (m_controller->isAudioSource())
    {
        m_decoder->setText(QString("Décodeur %1 sur l'audio de la carte son").arg(mode.m_name));
        return;
    }

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
    if (m_controller->isAudioSource()) {
        return;
    }

    const RBSMode& mode = m_controller->getMode();
    int rate = m_controller->getSampleRate();
    qint64 offset = frequency - mode.m_tuneShift - m_controller->getCenterFrequency();

    // Tunable mode (CW) and still within the spectrum: the decoder moves, not the device, so that
    // the waterfall stays as it is (as the VFO of SDR++)
    if (mode.m_tunable && (rate > 0) && (std::abs(offset) < rate / 2 - 5000))
    {
        m_controller->setChannelOffset((int) offset);
        m_marker->setCenterFrequency((int) offset);
    }
    else
    {
        m_controller->setTunedFrequency(frequency);
    }

    showTuned(frequency);
}

// ---------------------------------------------------------------------------
// Spectrum display: resolution, waterfall speed and zoom

void RBSWindow::loadDisplay(const RBSMode& mode)
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.beginGroup("display_" + mode.m_id);
    m_displayFFTSize = std::min(mode.m_fftSize, 1 << m_log2FFTSizeMax);
    m_displayLinesPerSecond = std::max(1, std::min(50, settings.value("linesPerSecond", mode.m_waterfallLinesPerSecond).toInt()));
    m_displaySmoothing = std::max(0, std::min(95, settings.value("smoothing", 0).toInt()));
    // The sheet gives the scale as max and min: the floor is the min, the contrast comes from max - min
    int sheetContrast = std::max(0, std::min(100, qRound((100.0 - (mode.m_maxLevel - mode.m_minLevel)) / 0.8)));
    m_scaleFloor = settings.value("floor", (double) mode.m_minLevel).toDouble();
    int brightness = settings.value("brightness", 0).toInt();
    int contrast = settings.value("contrast", sheetContrast).toInt();
    bool autoLevel = settings.value("autoLevel", true).toBool();
    int zoom = settings.value("zoom2", 0).toInt();
    settings.endGroup();
    settings.endGroup();

    m_updatingControls = true;
    m_waterfallSpeed->setValue(m_displayLinesPerSecond);
    m_waterfallSpeedText->setText(QString("%1 /s").arg(m_displayLinesPerSecond));
    m_smoothing->setValue(m_displaySmoothing);
    m_smoothingText->setText(QString("%1 %").arg(m_displaySmoothing));
    m_brightness->setValue(brightness);
    m_contrast->setValue(contrast);
    m_autoLevel->setChecked(autoLevel);
    m_zoom->setValue(zoom);
    m_updatingControls = false;
    m_spectrumTee->setLinesPerSecond(m_displayLinesPerSecond);
    m_spectrumTee->setSmoothing(m_displaySmoothing / 100.0f);
    m_scaleTimer.stop();
    applyScale();
}

// Bottom of the scale: the floor (the noise in automatic) lowered by the brightness; top: bottom +
// a span that shrinks with the contrast. Brighter and more contrasted to the right, as expected
void RBSWindow::applyScale()
{
    double span = 100.0 - 0.8 * m_contrast->value();
    double bottom = m_scaleFloor - m_brightness->value() / 2.0;
    m_spectrum->setScale((float) (bottom + span), (float) span);
}

void RBSWindow::saveDisplay()
{
    QSettings settings(QCoreApplication::organizationName(), m_settingsApplication);
    settings.beginGroup(m_settingsGroup);
    settings.beginGroup("display_" + m_controller->getMode().m_id);
    settings.setValue("linesPerSecond", m_displayLinesPerSecond);
    settings.setValue("smoothing", m_displaySmoothing);
    settings.setValue("floor", m_scaleFloor);
    settings.setValue("brightness", m_brightness->value());
    settings.setValue("contrast", m_contrast->value());
    settings.setValue("autoLevel", m_autoLevel->isChecked());
    settings.setValue("zoom2", m_zoom->value());
    settings.endGroup();
    settings.endGroup();
}

// The SpectrumVis computes at least 60 spectra per second at the mode's resolution (FFTs overlap
// when the rate is too low for that, averaged by groups when it is much higher). The spectrum tee
// then makes the waterfall lines at the chosen speed, with the chosen smoothing.
void RBSWindow::applySpectrumSettings()
{
    DeviceSet *deviceSet = m_controller->getDeviceSet();
    int rate = m_controller->getSampleRate();

    if (!deviceSet || !deviceSet->m_spectrumVis || (rate <= 0)) {
        return;
    }

    static constexpr int spectraPerSecondMin = 60;
    SpectrumSettings settings = deviceSet->m_spectrumVis->getSettings();
    int fftSize = m_displayFFTSize;
    double spectraPerSecond = (double) rate / fftSize;
    int overlap = 0;
    int average = 1;

    if (spectraPerSecond < spectraPerSecondMin) {
        overlap = std::max(0, fftSize - rate / spectraPerSecondMin);
    } else {
        average = std::max(1, (int) (spectraPerSecond / spectraPerSecondMin));
    }

    settings.m_fftSize = fftSize;
    settings.m_fftOverlap = overlap;
    settings.m_averagingMode = average > 1 ? SpectrumSettings::AvgModeFixed : SpectrumSettings::AvgModeNone;
    settings.m_averagingIndex = SpectrumSettings::getAveragingIndex(average, SpectrumSettings::AvgModeFixed);
    settings.m_averagingValue = SpectrumSettings::getAveragingValue(settings.m_averagingIndex, SpectrumSettings::AvgModeFixed);
    deviceSet->m_spectrumVis->getInputMessageQueue()->push(SpectrumVis::MsgConfigureSpectrumVis::create(settings, false));

    // Waterfall time scale: one line every 1/speed s, that is rate / FFT size / speed FFT lengths
    m_spectrum->setFFTOverlap(0);
    m_spectrum->setTimingRate(std::max(1, qRound((double) rate / fftSize / m_displayLinesPerSecond)));
}

// Zoom around the decoder: FT8 is 14 kHz above the centre, a centre zoom would leave it out
void RBSWindow::applyZoom()
{
    float zoom = std::pow(2.0f, m_zoom->value() / 100.0f);
    float position = 0.5f;
    int rate = m_controller->getSampleRate();

    if (rate > 0) {
        position += (float) (m_controller->getChannelOffset() + m_controller->getMode().m_tuneShift) / rate;
    }

    float limit = 0.5f / zoom;
    position = std::max(limit, std::min(1.0f - limit, position));
    m_spectrum->setFrequencyZooming(zoom, position);
}

// ---------------------------------------------------------------------------
// Decoder marker and tuning

void RBSWindow::setupMarker(const RBSMode& mode)
{
    m_marker->setTitle(mode.m_name);
    m_marker->setMovable(mode.m_tunable && !m_controller->isAudioSource());
    m_marker->setLowCutoff(mode.m_markerLow);
    m_marker->setBandwidth(2 * mode.m_markerHigh);   // as SDRangel's SSB demodulator: twice the high edge
    m_marker->setSidebands(mode.m_markerSideband == "usb" ? ChannelMarker::usb :
        mode.m_markerSideband == "lsb" ? ChannelMarker::lsb : ChannelMarker::dsb);
    m_marker->setCenterFrequency(m_controller->getChannelOffset());
    m_marker->setVisible(!mode.m_markerSideband.isEmpty());
}

// A click (or drag, or wheel) on the spectrum put the marker where the signal is: the decoder goes
// there, the tone pitch (tune shift) below so that the signal sits in the middle of the passband
void RBSWindow::onMarkerMoved()
{
    const RBSMode& mode = m_controller->getMode();
    int rate = m_controller->getSampleRate();

    if (!mode.m_tunable || (rate <= 0) || m_controller->isAudioSource()) {
        return;
    }

    int offset = m_marker->getCenterFrequency() - mode.m_tuneShift;
    int low = -rate / 2 - std::min(0, mode.m_markerLow) + 500;
    int high = rate / 2 - std::max(mode.m_markerHigh, 0) - 500;
    offset = std::max(low, std::min(high, offset));
    m_controller->setChannelOffset(offset);
    m_marker->setCenterFrequency(offset);
    showTuned(m_controller->getTunedFrequency());
}

// Frequency of the decoder shown in the dial, band, decoder text, and kept for the next start
void RBSWindow::showTuned(qint64 frequency)
{
    if (m_controller->isAudioSource()) {
        return;
    }

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
    else if (mode.m_view == "cw")
    {
        m_decodesTitle->setText("Morse (CW)");
        m_decodeModel->setColumns({"Heure", "Indicatif", "Pays", "Fréquence", "Contexte"}, 1);
        m_decodeTable->setItemDelegateForColumn(1, m_call2Chip);
        m_decodeTable->setColumnWidth(0, 70);
        m_decodeTable->setColumnWidth(1, 100);
        m_decodeTable->setColumnWidth(2, 110);
        m_decodeTable->setColumnWidth(3, 100);
    }
    else
    {
        m_decodesTitle->setText("Décodages");
        m_decodeModel->setColumns({"Heure", "Message"});
    }

    // Morse: MORSE DECK (coloured) instead of the table, and TEXTE (plain). Other modes: TABLEAU,
    // and TEXTE with the decodes as lines; none for aircraft
    bool cw = mode.m_view == "cw";
    m_cwText->clear();
    m_morseDeck->clear();
    m_deckWordStart = -1;
    m_deckWord.clear();
    m_deckPreviousWord.clear();
    m_cwBuffer.clear();
    m_cwLastText = QDateTime();
    m_cwText->setPlaceholderText(cw ?
        QString("Le texte Morse décodé s'écrit ici lettre par lettre, sans couleurs (à copier ou exporter).") :
        QString("Les décodages s'écrivent ici ligne par ligne, tels qu'ils arrivent."));
    m_morseDeck->setPlaceholderText("Le Morse décodé s'écrit ici lettre par lettre : les indicatifs en rouge, "
        "les appels (CQ, DE, K) en vert, les codes et la politesse (73, TU, 5NN, QTH) en bleu.");
    m_decodeTabs->setTabVisible(1, mode.m_view != "adsb");
    m_decodeTabs->setTabText(0, cw ? "MORSE DECK" : "TABLEAU");
    m_decodeTabs->setTabToolTip(0, cw ? QString("Le Morse décodé, indicatifs et mots courants en couleur") : QString("Les décodages en colonnes"));
    m_decodeTabs->setCurrentIndex(0);
    m_decodePages->setCurrentWidget(cw ? (QWidget*) m_morseDeck : (QWidget*) m_decodeViews);
    m_cwStats->setText(cw ? QString("en attente de signal…") : QString());
    updateDecoderPanel(mode);

    // In Morse the deck takes the place of the table: no view by country
    bool countries = (mode.m_view != "adsb") && !cw;
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
    addEvent(m_controller->isAudioSource() ?
        QString("%1 lancé sur la carte son").arg(mode.m_name) :
        QString("%1 lancé sur %2").arg(mode.m_name).arg(RBSBandPopup::formatMHz(mode.tunedFrequency())));
    attachSpectrum();
    applySpectrumSettings();

    // A narrow source (sound card at 48 kS/s...) can leave the mode's decoder out of the spectrum
    int rate = m_controller->getSampleRate();

    if ((rate > 0) && (std::abs(m_controller->getChannelOffset()) > rate / 2 - 3000)) {
        m_controller->setChannelOffset(0);
    }

    setupMarker(mode);
    applyZoom();
    readDeviceSettings();

    if (mode.m_tunable && !m_controller->isAudioSource()) {
        addEvent("Clique sur un signal du spectre pour le décoder");
    }

    if (!m_listen.m_demod.isEmpty()) {
        m_controller->setListen(m_listen);
    }

    if (mode.m_view == "ft8")
    {
        m_ft8Log->start(m_controller->getDeviceSetIndex(), m_controller->getChannelIndex());
    }
    else if (mode.m_view == "adsb")
    {
        m_adsbTimer.start(1000);
    }
    else if (mode.m_view == "cw")
    {
        m_cwTap->decoder().setSettings(RBSCWDecoder::Settings()); // automatic, as the check box
        m_cwTap->attach(m_controller->getChannel());
    }
}

// Sound card: the audio comes as it is, there is no frequency to choose or show
void RBSWindow::updateSourceKind()
{
    bool audio = m_controller->isAudioSource();
    m_frequencyBox->setVisible(!audio);
    m_bandBox->setVisible(!audio);
    m_modeForm->setRowVisible(m_modeFrequencies, !audio);
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
    m_listen.m_demod = settings.value("demod", mode.m_listen).toString();
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

    // setStyleSheet repolishes the widget even when the sheet is the same: only on a change
    auto setStatusColor = [this](const QString& styleSheet) {
        if (m_deviceStatus->styleSheet() != styleSheet) {
            m_deviceStatus->setStyleSheet(styleSheet);
        }
    };

    if (!hasDevice) {
        setStatusColor(m_controller->getState() == RBSController::Error ? "color: #ef4444;" : "color: #9696a0;");
        return;
    }

    setStatusColor(running ? "color: #3ecf7a;" : "color: #f2c94c;");

    if (running) {
        attachSpectrum(); // late start, or the device set was recreated
    }

    // Decodes shown / kept
    const QString& view = m_controller->getMode().m_view;
    m_decodeCount->setText(QString("%1 / %2 %3").arg(m_decodeFilter->rowCount()).arg(m_decodeModel->rowCount())
        .arg(view == "cw" ? "indicatifs" : view == "adsb" ? "avions" : view == "aprs" ? "stations" : "messages"));

    // Decoder marker where the decoder is (moved by the band selector or the dial)
    if (m_marker->getCenterFrequency() != m_controller->getChannelOffset()) {
        m_marker->setCenterFrequency(m_controller->getChannelOffset());
    }

    // Frequency, unless the mouse is on the dial (none with a sound card)
    if (!m_controller->isAudioSource() && !m_frequency->isInteracting())
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

    bool levelValid = (channelIndex >= 0)
        && ChannelWebAPIUtils::getChannelReportValue(m_controller->getDeviceSetIndex(), channelIndex, "channelPowerDB", powerDB);
    m_meters->setLevel(levelValid ? powerDB : -130.0, levelValid);

    // S/N: strongest bin of the decoder passband over the noise floor (median of the spectrum),
    // from the spectrum of the waterfall
    std::vector<float> spectrum;

    if ((rate > 0) && m_spectrumTee->getLastSpectrum(spectrum))
    {
        const RBSMode& mode = m_controller->getMode();
        int size = (int) spectrum.size();
        double binHz = (double) rate / size;
        int offset = m_controller->getChannelOffset();
        int low = mode.m_markerSideband == "usb" ? offset + mode.m_markerLow :
            mode.m_markerSideband == "lsb" ? offset - mode.m_markerHigh :
            mode.m_markerSideband == "dsb" ? offset - mode.m_markerHigh : -rate / 2;
        int high = mode.m_markerSideband == "usb" ? offset + mode.m_markerHigh :
            mode.m_markerSideband == "lsb" ? offset - mode.m_markerLow :
            mode.m_markerSideband == "dsb" ? offset + mode.m_markerHigh : rate / 2;
        int first = std::max(0, (int) (low / binHz) + size / 2);
        int last = std::min(size - 1, (int) (high / binHz) + size / 2);
        float peak = -1000.0f;

        for (int i = first; i <= last; i++) {
            peak = std::max(peak, spectrum[i]);
        }

        // The strongest noise bins reach well above the median (noise is not flat): compare with
        // the noise peaks (98th percentile of the spectrum), so that noise or silence gives 0 dB
        std::vector<float> sorted = spectrum;
        std::nth_element(sorted.begin(), sorted.begin() + size / 2, sorted.end());
        float noiseFloor = sorted[size / 2];
        std::nth_element(sorted.begin(), sorted.begin() + (size * 98) / 100, sorted.end());
        float noisePeaks = sorted[(size * 98) / 100];
        double snr = std::max(0.0, (double) (peak - noisePeaks));
        m_snrAverage = m_snrAverage < -50.0 ? snr : 0.6 * m_snrAverage + 0.4 * snr;
        int steps = m_snrAverage >= 20 ? 5 : m_snrAverage >= 14 ? 4 : m_snrAverage >= 9 ? 3 : m_snrAverage >= 5 ? 2 : m_snrAverage >= 3 ? 1 : 0;
        static const char* const words[] = {"", "faible", "faible", "moyen", "bon", "excellent"};
        bool signal = m_snrAverage >= 3.0;
        m_meters->setSNR(m_snrAverage, signal);
        m_meters->setQuality(steps, words[steps]);

        // Automatic brightness: the bottom of the scale follows the noise floor, slowly, and only
        // when it moved enough to be worth redrawing the waterfall
        if (m_autoLevel->isChecked())
        {
            double target = noiseFloor - 4.0;

            if (std::abs(target - m_scaleFloor) > 1.5)
            {
                m_scaleFloor += 0.5 * (target - m_scaleFloor);
                m_scaleTimer.start();
            }
        }
    }
    else
    {
        m_snrAverage = -100.0;
        m_meters->setSNR(0.0, false);
        m_meters->setQuality(0, QString());
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

    m_groupByCountry->setText(countries > 0 ? QString("Grouper par pays (%1)").arg(countries) : QString("Grouper par pays"));
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
    if (m_frozen)
    {
        m_frozenFT8.append(decode);
        return;
    }

    QString myCall = MainCore::instance()->getSettings().getStationName().trimmed().toUpper();
    RBSDecodeModel::Kind kind = RBSDecodeModel::Normal;
    appendTextLine(decode.m_dateTime.toString("HH:mm:ss"), QString("%1 %2 %3  %4  %5 %6")
        .arg(decode.m_snr, 3).arg(decode.m_dt, 4, 'f', 1).arg(decode.m_df, 4)
        .arg(decode.m_call1, -8).arg(decode.m_call2, -10).arg(decode.m_loc));

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
    if (m_frozen) {
        return; // the data feed keeps them: read when "Figer" is released
    }

    const QString& view = m_controller->getMode().m_view;

    if (view != "aprs") {
        return; // FT8, Morse and aircraft come their own ways
    }

    QJsonObject result = m_dataFeed->getPackets(QString(), "PacketDemod", 200, m_lastPacketSequence);
    QJsonArray packets = result.value("packets").toArray();

    for (const auto& value : packets)
    {
        QJsonObject packet = value.toObject();
        m_lastPacketSequence = std::max(m_lastPacketSequence, (qint64) packet.value("sequence").toDouble());

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
        appendTextLine(time.toLocalTime().toString("HH:mm:ss"), QString("%1 > %2%3  %4")
            .arg(from).arg(decoded.value("to").toString())
            .arg(decoded.value("via").toString().isEmpty() ? QString() : "," + decoded.value("via").toString())
            .arg(decoded.value("data").toString()));

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

// TEXTE tab, other than Morse: one line per decode, time dimmed. Stays at the bottom unless the
// user scrolled up to read
void RBSWindow::appendTextLine(const QString& time, const QString& text)
{
    QScrollBar *scroll = m_cwText->verticalScrollBar();
    bool atBottom = scroll->value() >= scroll->maximum() - 4;
    QTextCursor cursor(m_cwText->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat timeFormat;
    timeFormat.setForeground(QColor("#6e6e78"));
    QTextCharFormat textFormat;
    textFormat.setForeground(QColor("#e3e3e6"));
    cursor.insertText(QString("%1%2  ").arg(m_cwText->document()->isEmpty() ? "" : "\n").arg(time), timeFormat);
    cursor.insertText(text, textFormat);

    while (m_cwText->document()->blockCount() > 5000)
    {
        QTextCursor first(m_cwText->document()->firstBlock());
        first.select(QTextCursor::BlockUnderCursor);
        first.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        first.removeSelectedText();
    }

    if (atBottom) {
        scroll->setValue(scroll->maximum());
    }
}

// Characters from the RBS Morse decoder: written in the MORSE DECK and TEXTE tabs as they come;
// tone, speed and S/N in the decoder panel
void RBSWindow::onCWDecoded()
{
    RBSCWDecoder& decoder = m_cwTap->decoder();
    std::vector<RBSCWDecoder::Character> characters = decoder.takeCharacters();
    decoder.takeScope(); // not shown: keep it from growing

    QDateTime now = QDateTime::currentDateTimeUtc();

    if (!m_cwStatsShown.isValid() || (m_cwStatsShown.msecsTo(now) >= 300))
    {
        RBSCWDecoder::Status status = decoder.getStatus();
        m_cwStatsShown = now;
        m_cwStats->setText(status.m_signal ?
            QString("Tonalité %1 Hz  ·  %2 WPM  ·  S/B %3 dB").arg(qRound(status.m_toneHz)).arg(qRound(status.m_wpm)).arg(qRound(status.m_snrDB)) :
            QString("en attente de signal…"));
    }

    if (characters.empty() || m_frozen) {
        return;
    }

    int newFrom = m_cwBuffer.size();

    for (const auto& character : characters) {
        addMorseCharacter(QString::fromStdString(character.m_text), character.m_confidence);
    }

    findCallsigns(newFrom);

    if (m_cwBuffer.size() > 600) {
        m_cwBuffer = m_cwBuffer.right(400);
    }
}

void RBSWindow::addMorseCharacter(const QString& text, float confidence)
{
    QDateTime now = QDateTime::currentDateTimeUtc();

    if (text == " ")
    {
        // End of a word: coloured in the deck, one space in both tabs
        finishDeckWord();

        if (!m_cwBuffer.isEmpty() && !m_cwBuffer.endsWith(' '))
        {
            for (QTextEdit *edit : {m_cwText, m_morseDeck})
            {
                QTextCursor cursor(edit->document());
                cursor.movePosition(QTextCursor::End);
                cursor.insertText(" ", QTextCharFormat());
            }

            m_cwBuffer += ' ';
        }

        return;
    }

    // Anti-noise: what was decoded with too little confidence is left out (letters made of noise)
    if (100.0f * confidence < m_cwMinConfidence) {
        return;
    }

    // A new line with the time after a pause
    if (!m_cwLastText.isValid() || (m_cwLastText.secsTo(now) > 20))
    {
        finishDeckWord();
        startMorseLine(m_cwText, now.toString("HH:mm:ss"));
        startMorseLine(m_morseDeck, now.toString("HH:mm:ss"));
        m_deckPreviousWord.clear();

        if (!m_cwBuffer.endsWith(' ')) {
            m_cwBuffer += ' ';
        }
    }

    // Brightness from the confidence, not below 40 %
    int alpha = std::max(100, std::min(255, (int) (255.0f * confidence)));
    QTextCharFormat format;
    format.setForeground(QColor(227, 227, 230, alpha));

    for (QTextEdit *edit : {m_cwText, m_morseDeck})
    {
        QScrollBar *scroll = edit->verticalScrollBar();
        bool atBottom = scroll->value() >= scroll->maximum() - 4;
        QTextCursor cursor(edit->document());
        cursor.movePosition(QTextCursor::End);

        if ((edit == m_morseDeck) && (m_deckWordStart < 0))
        {
            m_deckWordStart = cursor.position();
            m_deckWordConfidence = 1.0f;
        }

        cursor.insertText(text, format);

        if (atBottom) {
            scroll->setValue(scroll->maximum());
        }
    }

    m_deckWord += text;
    m_deckWordConfidence = std::min(m_deckWordConfidence, confidence);
    m_cwBuffer += text;
    m_cwLastText = now;
}

// The word just received, coloured in the deck: callsigns in red, calls in green, codes and
// courtesy in blue
void RBSWindow::finishDeckWord()
{
    if (m_deckWordStart < 0) {
        return;
    }

    static const QSet<QString> calls = {
        "CQ", "CQD", "QRZ", "QRZ?", "DE", "K", "KN", "<KN>", "BK", "AR", "<AR>", "PSE", "TEST", "SOS"
    };
    static const QSet<QString> codes = {
        "73", "88", "TU", "TNX", "TKS", "SK", "<SK>", "5NN", "599", "589", "579", "569", "559", "RST", "UR",
        "QSL", "QSO", "QTH", "QRL", "QRM", "QRN", "QSB", "QRP", "QRO", "QRS", "QRQ", "QRT", "QRV", "QRX", "QSY",
        "NAME", "OP", "ES", "FB", "GL", "GM", "GA", "GE", "GN", "HR", "WX", "VY", "R", "<BT>", "=", "VVV",
        "DX", "RIG", "ANT", "PWR", "HW", "HW?", "CPY", "AGN", "SRI", "CUL", "BCNU", "OM", "YL", "DR", "GUD"
    };
    static const QRegularExpression callPattern("^[A-Z0-9]{1,3}[0-9][A-Z0-9]{0,3}[A-Z](/[A-Z0-9]{1,4})?$");

    const QString& word = m_deckWord;
    QColor color;
    bool bold = false;

    if (calls.contains(word))
    {
        color = QColor("#3ecf7a");
        bold = true;
    }
    else if (codes.contains(word))
    {
        color = QColor("#60a5fa");
    }
    else if (callPattern.match(word).hasMatch() && !countryOf(word).isEmpty() &&
        ((m_deckWordConfidence >= 0.5f) || (m_deckPreviousWord == "DE") || m_callsToday.contains(word)))
    {
        color = QColor("#ef4444");
        bold = true;
    }

    if (color.isValid())
    {
        QTextCursor cursor(m_morseDeck->document());
        cursor.setPosition(m_deckWordStart);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setForeground(color);

        if (bold) {
            format.setFontWeight(QFont::Bold);
        }

        cursor.mergeCharFormat(format);
    }

    m_deckPreviousWord = word;
    m_deckWord.clear();
    m_deckWordStart = -1;
    m_messageCount++;
    m_statsMessages->setText(QString::number(m_messageCount));
}

// A new line, the time dimmed in front; the text kept to a day's worth
void RBSWindow::startMorseLine(QTextEdit *edit, const QString& time)
{
    trimText(edit, 3000);
    QTextCursor cursor(edit->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat timeFormat;
    timeFormat.setForeground(QColor("#6e6e78"));
    timeFormat.setFontWeight(QFont::Normal);
    cursor.insertText(QString("%1%2  ").arg(edit->document()->isEmpty() ? "" : "\n").arg(time), timeFormat);
}

void RBSWindow::trimText(QTextEdit *edit, int maxBlocks)
{
    while (edit->document()->blockCount() > maxBlocks)
    {
        QTextCursor first(edit->document()->firstBlock());
        first.select(QTextCursor::BlockUnderCursor);
        first.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        first.removeSelectedText();
    }
}

// Callsigns named in the decoded text: after DE ("CQ CQ DE F4ABC", "VVV DE DK0WCY"), before DE
// ("F4ABC DE DL1XYZ"), or given twice ("F4ABC F4ABC"). Only whole words, each one once: a match is
// taken when the text that completes it (up to the space after the callsign) has just come in, from
// newFrom on.
void RBSWindow::findCallsigns(int newFrom)
{
    static const QString call = "([A-Z0-9]{1,3}[0-9][A-Z0-9]{0,3}[A-Z](?:/[A-Z0-9]{1,4})?)";
    // Pattern, and characters after the match needed to complete it (the space looked ahead for)
    static const QList<QPair<QRegularExpression, int>> patterns = {
        {QRegularExpression("\\bDE\\s+" + call + "(?=\\s)"), 1},
        {QRegularExpression("\\b" + call + "\\s+DE\\s"), 0},
        {QRegularExpression("\\b" + call + "\\s+\\1(?=\\s)"), 1}
    };
    QString context = m_cwBuffer.right(60).simplified();
    qint64 frequency = m_controller->getTunedFrequency();
    QSet<QString> found; // "CQ DE F4ABC F4ABC": one station, not two

    for (const auto& pattern : patterns)
    {
        QRegularExpressionMatchIterator it = pattern.first.globalMatch(m_cwBuffer);

        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();

            if (match.capturedEnd(0) + pattern.second <= newFrom) {
                continue; // complete before: taken when its text came in
            }

            QString callsign = match.captured(1);
            QString country = countryOf(callsign);

            if (country.isEmpty() || found.contains(callsign)) {
                continue; // not a callsign (garbled text), or already taken from this text
            }

            found.insert(callsign);

            bool isNew = !m_callsToday.contains(callsign);
            m_decodeModel->upsertRow(callsign, {
                QDateTime::currentDateTimeUtc().toString("HH:mm:ss"),
                callsign,
                country,
                RBSBandPopup::formatMHz(frequency),
                context
            });
            m_callsToday.insert(callsign);
            m_statsCalls->setText(QString::number(m_callsToday.size()));
            addStation(callsign, QString(), context, 0, -1.0);

            if (isNew) {
                addEvent(QString("Morse : %1 (%2)").arg(callsign).arg(country));
            }
        }
    }
}

void RBSWindow::refreshAircraft()
{
    if ((m_controller->getMode().m_view != "adsb") || m_frozen) {
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
    // TEXTE tab: the text (the Morse is worth keeping as text), TABLEAU tab: the columns as CSV
    QTextEdit *textPage = qobject_cast<QTextEdit*>(m_decodePages->currentWidget());
    bool cw = textPage != nullptr;
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString suggested = QDir(documents).absoluteFilePath(QString("rbs_%1_%2.%3")
        .arg(m_controller->getMode().m_id)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmm"))
        .arg(cw ? "txt" : "csv"));
    QString fileName = QFileDialog::getSaveFileName(this, cw ? "Exporter le texte Morse" : "Exporter le journal", suggested,
        cw ? "Texte (*.txt)" : "CSV (*.csv)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write("\xEF\xBB\xBF"); // BOM so that Excel reads the accents
        file.write((cw ? textPage->toPlainText() + "\n" : m_decodeModel->toCSV()).toUtf8());
        addEvent(QString("Journal exporté : %1").arg(QFileInfo(fileName).fileName()));
    }
    else
    {
        addEvent(QString("Erreur : impossible d'écrire %1").arg(fileName));
    }
}

void RBSWindow::copySelection()
{
    // TEXTE tab: the selected text, else all of it
    if (QTextEdit *textPage = qobject_cast<QTextEdit*>(m_decodePages->currentWidget()))
    {
        QString selected = textPage->textCursor().selectedText();
        QApplication::clipboard()->setText(selected.isEmpty() ? textPage->toPlainText() : selected);
        return;
    }

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
