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
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
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
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "channel/channelwebapiutils.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "device/deviceuiset.h"
#include "gui/glspectrum.h"
#include "gui/valuedial.h"
#include "util/maidenhead.h"
#include "maincore.h"
#include "mainwindow.h"

#include "mcpdatafeed.h"
#include "rbssection.h"
#include "rbsspectrumtee.h"
#include "rbswindow.h"

static const char* const m_settingsGroup = "RBSLauncher";

static const char* const m_styleSheet = R"(
RBSWindow, QMainWindow::separator { background: #0b0d10; }
QMainWindow::separator { width: 3px; height: 3px; }
QToolBar#rbsTopBarToolbar { background: #111418; border: none; border-bottom: 1px solid #252a31; padding: 0; spacing: 0; }
QWidget#rbsTopBar { background: #111418; }
QDockWidget { color: #dfe4ea; }
QDockWidget::title { background: #181c22; padding: 4px 8px; border-bottom: 1px solid #252a31; text-align: left; }
QToolButton#rbsSectionHeader { background: #181c22; border: none; border-bottom: 1px solid #252a31; padding: 5px 6px; color: #dfe4ea; text-align: left; }
QWidget#rbsSectionBody { background: #111418; border-bottom: 1px solid #252a31; }
QWidget#rbsPanel, QScrollArea, QScrollArea > QWidget > QWidget { background: #111418; border: none; }
QTableView { background: #08090b; alternate-background-color: #0c0e11; gridline-color: #15191e; border: none; selection-background-color: #1b3a66; selection-color: #ffffff; }
QHeaderView::section { background: #111418; color: #7f8a98; border: none; border-bottom: 1px solid #252a31; padding: 3px 8px; }
QLineEdit, QSpinBox, QComboBox { background: #08090b; border: 1px solid #252a31; border-radius: 0; padding: 2px 6px; color: #dfe4ea; min-height: 18px; }
QComboBox::drop-down { border: none; width: 16px; }
QComboBox QAbstractItemView { background: #111418; border: 1px solid #252a31; selection-background-color: #1b3a66; }
QToolButton { border: 1px solid #252a31; background: #181c22; padding: 3px 10px; border-radius: 0; color: #dfe4ea; }
QToolButton:hover { border-color: #3d8bff; }
QToolButton:checked { background: #3d8bff; border-color: #3d8bff; color: #ffffff; }
QToolButton#rbsPlay { min-width: 30px; font-size: 13px; }
QToolButton::menu-indicator { image: none; }
QProgressBar { background: #1c2128; border: none; max-height: 6px; }
QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3ecf7a, stop:0.8 #f2c94c, stop:1 #ef4444); }
QCheckBox { color: #dfe4ea; spacing: 6px; }
QCheckBox::indicator { width: 11px; height: 11px; border: 1px solid #4a525d; background: #08090b; }
QCheckBox::indicator:checked { background: #3d8bff; border-color: #3d8bff; }
QListWidget { background: #111418; border: none; outline: 0; }
QListWidget::item { padding: 3px 8px; border-left: 2px solid transparent; color: #dfe4ea; }
QListWidget::item:selected { background: #13213a; border-left: 2px solid #3d8bff; color: #dfe4ea; }
QListWidget::item:disabled { color: #7f8a98; }
QStatusBar { background: #050607; color: #7f8a98; border-top: 1px solid #252a31; }
QStatusBar QLabel { color: #7f8a98; padding: 0 8px; }
QStatusBar::item { border: none; }
QSlider::groove:vertical { width: 3px; background: #2d333b; }
QSlider::add-page:vertical { background: #3d8bff; }
QSlider::sub-page:vertical { background: #2d333b; }
QSlider::handle:vertical { height: 4px; margin: 0 -4px; background: #dfe4ea; }
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

RBSWindow::RBSWindow(WebAPIAdapterInterface *webAPIAdapterInterface, QWidget *parent) :
    QMainWindow(parent),
    m_layout(AllInOne),
    m_updatingControls(false),
    m_spectrumCenter(-1),
    m_spectrumRate(-1),
    m_lastPacketSequence(0),
    m_messageCount(0),
    m_farthestKm(0.0)
{
    setObjectName("RBSWindow");
    setWindowTitle("RBS");
    setWindowIcon(QIcon(":/sdrangel_icon.png"));
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
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

    // Panels
    m_controlsDock = createDock("Contrôles", "rbsControlsDock", createControls());
    m_spectrumDock = createDock("Spectre et cascade", "rbsSpectrumDock", createSpectrum());
    m_decodesDock = createDock("Décodages", "rbsDecodesDock", createDecodes());
    m_infoDock = createDock("Infos", "rbsInfoDock", createInfo());

    QToolButton *panels = findChild<QToolButton*>("rbsPanels");
    QMenu *panelsMenu = new QMenu(panels);
    panelsMenu->addAction(m_spectrumDock->toggleViewAction());
    panelsMenu->addAction(m_decodesDock->toggleViewAction());
    panelsMenu->addAction(m_controlsDock->toggleViewAction());
    panelsMenu->addAction(m_infoDock->toggleViewAction());
    panels->setMenu(panelsMenu);

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
    statusBar()->setSizeGripEnabled(true);

    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();
    m_statusLocation->setText(((latitude == 0.0f) && (longitude == 0.0f)) ?
        QString("Position non définie (Préférences > Ma position)") :
        Maidenhead::toMaidenhead(latitude, longitude));

    // Decode sources
    m_ft8Log = new RBSFT8Log(this);
    connect(m_ft8Log, &RBSFT8Log::decoded, this, &RBSWindow::onFT8Decode);
    m_dataFeed = new MCPDataFeed(this);
    connect(m_dataFeed, &MCPDataFeed::packetsChanged, this, &RBSWindow::onPackets, Qt::QueuedConnection);
    connect(&m_adsbTimer, &QTimer::timeout, this, &RBSWindow::refreshAircraft);

    // Mode lists
    for (const auto& mode : m_modes) {
        m_modeCombo->addItem(mode.m_name, mode.m_id);
    }

    QString group;

    for (const auto& mode : m_modes)
    {
        if (mode.m_group != group)
        {
            group = mode.m_group;
            QListWidgetItem *header = new QListWidgetItem(group.toUpper(), m_modeList);
            header->setFlags(Qt::NoItemFlags);
            QFont font = header->font();
            font.setPointSizeF(font.pointSizeF() * 0.85);
            header->setFont(font);
        }

        QListWidgetItem *item = new QListWidgetItem(
            QString("%1    %2").arg(mode.m_name).arg(mode.tunedFrequency() / 1e6, 0, 'f', 3), m_modeList);
        item->setData(Qt::UserRole, mode.m_id);
    }

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        startMode(m_modeCombo->itemData(index).toString());
    });
    connect(m_modeList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QString id = item->data(Qt::UserRole).toString();

        if (!id.isEmpty()) {
            startMode(id);
        }
    });

    // Layout: the one used last, all in one the first time
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    Layout layout = settings.value("layout", "allinone").toString() == "windows" ? Windows : AllInOne;
    settings.endGroup();
    setLayoutMode(layout, false);

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

QDockWidget *RBSWindow::createDock(const QString& title, const QString& objectName, QWidget *widget)
{
    QDockWidget *dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setWidget(widget);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    return dock;
}

QWidget *RBSWindow::createTopBar()
{
    QWidget *bar = new QWidget(this);
    bar->setObjectName("rbsTopBar");
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(10);

    m_play = new QToolButton(bar);
    m_play->setObjectName("rbsPlay");
    m_play->setCheckable(true);
    m_play->setText("▶");
    m_play->setToolTip("Démarrer / arrêter le RSP1B");
    connect(m_play, &QToolButton::clicked, this, [this](bool checked) {
        if (checked) {
            m_controller->startDevice();
        } else {
            m_controller->stopDevice();
        }
    });
    layout->addWidget(m_play);

    m_frequency = new ValueDial(bar);
    m_frequency->setValueRange(10, 0, 9999999999ULL);
    QFont dialFont = font();
    dialFont.setPointSize(20);
    m_frequency->setFont(dialFont);
    m_frequency->setToolTip("Fréquence écoutée : molette ou flèches sur un chiffre pour la régler");
    connect(m_frequency, &ValueDial::changed, this, [this](quint64 value) {
        if (!m_updatingControls) {
            m_controller->setTunedFrequency((qint64) value);
        }
    });
    layout->addWidget(m_frequency);
    layout->addWidget(dimLabel("Hz", bar));

    QVBoxLayout *modeBox = new QVBoxLayout();
    modeBox->setSpacing(0);
    modeBox->addWidget(dimLabel("Mode", bar));
    m_modeCombo = new QComboBox(bar);
    m_modeCombo->setMinimumWidth(130);
    modeBox->addWidget(m_modeCombo);
    layout->addLayout(modeBox);

    QVBoxLayout *bandBox = new QVBoxLayout();
    bandBox->setSpacing(0);
    bandBox->addWidget(dimLabel("Bande", bar));
    m_band = new QLabel("—", bar);
    bandBox->addWidget(m_band);
    layout->addLayout(bandBox);

    QVBoxLayout *levelBox = new QVBoxLayout();
    levelBox->setSpacing(2);
    levelBox->addWidget(dimLabel("Niveau du canal", bar));
    m_level = new QProgressBar(bar);
    m_level->setRange(-130, 0);
    m_level->setValue(-130);
    m_level->setTextVisible(false);
    m_level->setFixedWidth(120);
    levelBox->addWidget(m_level);
    m_levelText = new QLabel("—", bar);
    levelBox->addWidget(m_levelText);
    layout->addLayout(levelBox);

    QVBoxLayout *statusBox = new QVBoxLayout();
    statusBox->setSpacing(0);
    m_deviceStatus = new QLabel("■ RSP1B", bar);
    m_antennaStatus = new QLabel("■ —", bar);
    m_biasTeeStatus = new QLabel("■ Bias-T", bar);
    statusBox->addWidget(m_deviceStatus);
    statusBox->addWidget(m_antennaStatus);
    statusBox->addWidget(m_biasTeeStatus);
    layout->addLayout(statusBox);

    layout->addStretch(1);

    QToolButton *panels = new QToolButton(bar);
    panels->setObjectName("rbsPanels");
    panels->setText("Panneaux ▾");
    panels->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(panels);

    m_layoutAllInOne = new QToolButton(bar);
    m_layoutAllInOne->setText("Tout-en-un");
    m_layoutAllInOne->setCheckable(true);
    m_layoutWindows = new QToolButton(bar);
    m_layoutWindows->setText("Fenêtres");
    m_layoutWindows->setCheckable(true);
    QButtonGroup *layoutGroup = new QButtonGroup(bar);
    layoutGroup->setExclusive(true);
    layoutGroup->addButton(m_layoutAllInOne);
    layoutGroup->addButton(m_layoutWindows);
    QHBoxLayout *layoutSwitch = new QHBoxLayout();
    layoutSwitch->setSpacing(0);
    layoutSwitch->addWidget(m_layoutAllInOne);
    layoutSwitch->addWidget(m_layoutWindows);
    layout->addLayout(layoutSwitch);
    connect(m_layoutAllInOne, &QToolButton::clicked, this, [this]() { setLayoutMode(AllInOne); });
    connect(m_layoutWindows, &QToolButton::clicked, this, [this]() { setLayoutMode(Windows); });

    QLabel *logo = new QLabel("RBS", bar);   // Placeholder until the logo files are supplied
    logo->setObjectName("rbsLogo");
    layout->addWidget(logo);

    return bar;
}

QWidget *RBSWindow::createControls()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *panel = new QWidget(scroll);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Source
    RBSSection *source = new RBSSection("Source", panel);
    source->form()->addRow(dimLabel("Récepteur"), new QLabel("SDRplay RSP1B"));
    m_sampleRate = new QLabel("—");
    source->form()->addRow(dimLabel("Échantillonnage"), m_sampleRate);
    m_lna = new QSpinBox();
    m_lna->setRange(0, 9);
    m_lna->setToolTip("Atténuation LNA : 0 = gain maximal, 9 = atténuation maximale");
    source->form()->addRow(dimLabel("LNA"), m_lna);
    m_ifAGC = new QCheckBox("AGC IF");
    m_amNotch = new QCheckBox("Notch AM (PO)");
    m_fmNotch = new QCheckBox("Notch FM broadcast");
    m_dabNotch = new QCheckBox("Notch DAB");
    m_biasTee = new QCheckBox("Bias-T (alimente l'antenne)");
    m_biasTee->setToolTip("Envoie une tension sur le câble : seulement pour une antenne active (GNSS, MLA-30+ alimentée par le câble)");
    source->addWidget(m_ifAGC);
    source->addWidget(m_amNotch);
    source->addWidget(m_fmNotch);
    source->addWidget(m_dabNotch);
    source->addWidget(m_biasTee);
    layout->addWidget(source);

    connect(m_lna, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_updatingControls) {
            m_controller->setDeviceSetting("lnaIndex", value);
        }
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

    // Radio
    RBSSection *radio = new RBSSection("Radio", panel);
    m_demod = new QLabel("—");
    m_decoder = new QLabel("—");
    m_offset = new QLabel("—");
    radio->form()->addRow(dimLabel("Démodulation"), m_demod);
    radio->form()->addRow(dimLabel("Décodeur"), m_decoder);
    radio->form()->addRow(dimLabel("Décalage canal"), m_offset);
    QToolButton *openSDRangel = new QToolButton();
    openSDRangel->setText("Réglages avancés dans SDRangel");
    openSDRangel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(openSDRangel, &QToolButton::clicked, this, []() {
        if (MainWindow *mainWindow = MainWindow::getInstance())
        {
            mainWindow->showNormal();
            mainWindow->raise();
            mainWindow->activateWindow();
        }
    });
    radio->addWidget(openSDRangel);
    layout->addWidget(radio);

    // Modes
    RBSSection *modes = new RBSSection("Mes modes", panel);
    m_modeList = new QListWidget();
    m_modeList->setMinimumHeight(160);
    modes->addWidget(m_modeList);
    QToolButton *openModes = new QToolButton();
    openModes->setText("Ouvrir le dossier des fiches de modes");
    openModes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    openModes->setToolTip("Les fichiers .json de ce dossier ajoutent des modes ou remplacent ceux de base (même \"id\")");
    connect(openModes, &QToolButton::clicked, this, []() {
        QString path = RBSMode::userModesPath();
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    modes->addWidget(openModes);
    layout->addWidget(modes, 1);

    scroll->setWidget(panel);
    scroll->setMinimumWidth(240);
    return scroll;
}

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
    m_spectrum->setWaterfallShare(0.55f);
    m_spectrum->setReferenceLevel(-30.0f);
    m_spectrum->setPowerRange(80.0f);
    m_spectrum->setDecay(1);
    m_spectrum->setFPSPeriodMs(40);
    m_spectrum->setMinimumHeight(220);
    layout->addWidget(m_spectrum, 1);
    m_spectrumTee = new RBSSpectrumTee(m_spectrum);

    // SDR++ style vertical sliders: top and bottom of the scale
    QWidget *sliders = new QWidget(panel);
    sliders->setObjectName("rbsPanel");
    sliders->setFixedWidth(40);
    QVBoxLayout *slidersLayout = new QVBoxLayout(sliders);
    slidersLayout->setContentsMargins(4, 6, 4, 6);
    slidersLayout->setSpacing(4);
    m_refLevel = new QSlider(Qt::Vertical, sliders);
    m_refLevel->setRange(-120, 20);
    m_refLevel->setValue(-30);
    m_refLevel->setToolTip("Haut de l'échelle (dB)");
    m_range = new QSlider(Qt::Vertical, sliders);
    m_range->setRange(-160, -20);
    m_range->setValue(-110);
    m_range->setToolTip("Bas de l'échelle (dB)");
    slidersLayout->addWidget(dimLabel("Max", sliders), 0, Qt::AlignHCenter);
    slidersLayout->addWidget(m_refLevel, 1, Qt::AlignHCenter);
    slidersLayout->addWidget(dimLabel("Min", sliders), 0, Qt::AlignHCenter);
    slidersLayout->addWidget(m_range, 1, Qt::AlignHCenter);
    layout->addWidget(sliders);

    auto applyScale = [this]() {
        int top = m_refLevel->value();
        int bottom = std::min(m_range->value(), top - 10);
        m_spectrum->setReferenceLevel((float) top);
        m_spectrum->setPowerRange((float) (top - bottom));
    };
    connect(m_refLevel, &QSlider::valueChanged, this, applyScale);
    connect(m_range, &QSlider::valueChanged, this, applyScale);

    return panel;
}

QWidget *RBSWindow::createDecodes()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget *tools = new QWidget(panel);
    QHBoxLayout *toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(6, 4, 6, 4);
    toolsLayout->setSpacing(6);
    m_search = new QLineEdit(tools);
    m_search->setPlaceholderText("Chercher : indicatif, locator, texte…");
    m_search->setClearButtonEnabled(true);
    m_highlightedOnly = new QCheckBox("CQ / nouveaux seulement", tools);
    QToolButton *copy = new QToolButton(tools);
    copy->setText("Copier");
    QToolButton *exportButton = new QToolButton(tools);
    exportButton->setText("Exporter CSV");
    QToolButton *clear = new QToolButton(tools);
    clear->setText("Effacer");
    toolsLayout->addWidget(m_search, 1);
    toolsLayout->addWidget(m_highlightedOnly);
    toolsLayout->addWidget(copy);
    toolsLayout->addWidget(exportButton);
    toolsLayout->addWidget(clear);
    layout->addWidget(tools);

    m_decodeModel = new RBSDecodeModel(this);
    m_decodeFilter = new RBSDecodeFilter(this);
    m_decodeFilter->setSourceModel(m_decodeModel);
    m_decodeTable = new QTableView(panel);
    m_decodeTable->setModel(m_decodeFilter);
    m_decodeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_decodeTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_decodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_decodeTable->setShowGrid(false);
    m_decodeTable->setWordWrap(false);
    m_decodeTable->verticalHeader()->setVisible(false);
    m_decodeTable->verticalHeader()->setDefaultSectionSize(20);
    m_decodeTable->horizontalHeader()->setStretchLastSection(true);
    m_decodeTable->horizontalHeader()->setHighlightSections(false);
    m_decodeTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(m_decodeTable, 1);

    connect(m_search, &QLineEdit::textChanged, m_decodeFilter, &RBSDecodeFilter::setSearch);
    connect(m_highlightedOnly, &QCheckBox::toggled, m_decodeFilter, &RBSDecodeFilter::setHighlightedOnly);
    connect(copy, &QToolButton::clicked, this, &RBSWindow::copySelection);
    connect(exportButton, &QToolButton::clicked, this, &RBSWindow::exportCSV);
    connect(clear, &QToolButton::clicked, m_decodeModel, &RBSDecodeModel::clear);

    return panel;
}

QWidget *RBSWindow::createInfo()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *panel = new QWidget(scroll);
    panel->setObjectName("rbsPanel");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    RBSSection *today = new RBSSection("Aujourd'hui", panel);
    m_statsMessages = new QLabel("0");
    m_statsCalls = new QLabel("0");
    m_statsFarthest = new QLabel("—");
    m_statsFarthest->setWordWrap(true);
    today->form()->addRow(dimLabel("Messages"), m_statsMessages);
    today->form()->addRow(dimLabel("Indicatifs"), m_statsCalls);
    today->form()->addRow(dimLabel("Plus loin"), m_statsFarthest);
    layout->addWidget(today);

    RBSSection *events = new RBSSection("Événements", panel);
    m_events = new QListWidget();
    m_events->setMinimumHeight(140);
    m_events->setSelectionMode(QAbstractItemView::NoSelection);
    events->addWidget(m_events);
    layout->addWidget(events, 1);

    RBSSection *antenna = new RBSSection("Antenne", panel);
    m_antennaInfo = new QLabel("—");
    antenna->form()->addRow(dimLabel("Conseillée"), m_antennaInfo);
    layout->addWidget(antenna);

    scroll->setWidget(panel);
    scroll->setMinimumWidth(220);
    return scroll;
}

// ---------------------------------------------------------------------------
// Layouts

void RBSWindow::setLayoutMode(Layout layout, bool save)
{
    if (save) {
        saveLayout();
    }

    m_layout = layout;
    m_layoutAllInOne->setChecked(layout == AllInOne);
    m_layoutWindows->setChecked(layout == Windows);

    if (!restoreLayout(layout))
    {
        if (layout == AllInOne) {
            arrangeAllInOne();
        } else {
            arrangeWindows();
        }
    }

    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue("layout", layout == Windows ? "windows" : "allinone");
    settings.endGroup();
}

void RBSWindow::saveLayout()
{
    QString key = m_layout == Windows ? "windows" : "allinone";
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue(key + "/geometry", saveGeometry());
    settings.setValue(key + "/state", saveState());
    settings.endGroup();
}

bool RBSWindow::restoreLayout(Layout layout)
{
    QString key = layout == Windows ? "windows" : "allinone";
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    QByteArray geometry = settings.value(key + "/geometry").toByteArray();
    QByteArray state = settings.value(key + "/state").toByteArray();
    settings.endGroup();

    if (geometry.isEmpty() || state.isEmpty()) {
        return false;
    }

    restoreGeometry(geometry);
    return restoreState(state);
}

void RBSWindow::arrangeAllInOne()
{
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    const QList<QDockWidget*> docks = {m_controlsDock, m_spectrumDock, m_decodesDock, m_infoDock};

    for (auto dock : docks)
    {
        dock->setFloating(false);
        removeDockWidget(dock);
    }

    addDockWidget(Qt::LeftDockWidgetArea, m_controlsDock);
    addDockWidget(Qt::TopDockWidgetArea, m_spectrumDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_decodesDock);
    addDockWidget(Qt::RightDockWidgetArea, m_infoDock);

    for (auto dock : docks) {
        dock->show();
    }

    QRect available = screen() ? screen()->availableGeometry() : QRect(0, 0, 1600, 900);
    resize(std::min(1500, available.width() - 40), std::min(950, available.height() - 60));
    resizeDocks({m_controlsDock, m_infoDock}, {260, 240}, Qt::Horizontal);
    resizeDocks({m_spectrumDock, m_decodesDock}, {480, 260}, Qt::Vertical);
}

void RBSWindow::arrangeWindows()
{
    QRect available = screen() ? screen()->availableGeometry() : QRect(0, 0, 1600, 900);
    const QList<QDockWidget*> docks = {m_controlsDock, m_spectrumDock, m_decodesDock, m_infoDock};

    for (auto dock : docks)
    {
        if (!dockWidgetArea(dock)) {
            addDockWidget(Qt::LeftDockWidgetArea, dock);
        }

        dock->setFloating(true);
        dock->show();
    }

    // This window keeps only the top bar and the status bar, across the top of the screen
    int margin = 16;
    int barHeight = minimumSizeHint().height();
    setGeometry(available.x() + margin, available.y() + margin + 30, available.width() - 2 * margin, barHeight);

    int x0 = available.x() + margin;
    int y0 = available.y() + margin + 30 + barHeight + 40;
    int width = available.width() - 2 * margin;
    int height = available.bottom() - y0 - margin;
    int leftWidth = width * 62 / 100;
    int rightWidth = width - leftWidth - 8;
    int topHeight = height * 60 / 100;
    int bottomHeight = height - topHeight - 40;

    m_spectrumDock->setGeometry(x0, y0, leftWidth, topHeight);
    m_decodesDock->setGeometry(x0, y0 + topHeight + 40, leftWidth, bottomHeight);
    m_controlsDock->setGeometry(x0 + leftWidth + 8, y0, rightWidth, topHeight);
    m_infoDock->setGeometry(x0 + leftWidth + 8, y0 + topHeight + 40, rightWidth, bottomHeight);
}

void RBSWindow::closeEvent(QCloseEvent *event)
{
    saveLayout();

    // Floating panels belong to this window: hide them with it
    for (auto dock : {m_controlsDock, m_spectrumDock, m_decodesDock, m_infoDock})
    {
        if (dock->isFloating()) {
            dock->hide();
        }
    }

    event->accept();
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
    m_lastPacketSequence = 0;

    m_modeCombo->blockSignals(true);
    m_modeCombo->setCurrentIndex(m_modeCombo->findData(mode->m_id));
    m_modeCombo->blockSignals(false);

    for (int i = 0; i < m_modeList->count(); i++)
    {
        if (m_modeList->item(i)->data(Qt::UserRole).toString() == mode->m_id) {
            m_modeList->setCurrentRow(i);
        }
    }

    showModeSettings(*mode);
    setupDecodeView(*mode);
    setWindowTitle(QString("RBS — %1").arg(mode->m_name));
    m_controller->startMode(*mode);
    emit modeChanged(mode->m_id);
}

void RBSWindow::showModeSettings(const RBSMode& mode)
{
    m_band->setText(mode.m_band.isEmpty() ? "—" : mode.m_band);
    m_demod->setText(mode.m_demod);
    m_decoder->setText(mode.m_channelType);
    m_offset->setText(QString("%1 kHz").arg(mode.m_channelOffset / 1000.0, 0, 'f', 1));
    m_antennaStatus->setText(QString("■ %1").arg(mode.m_antenna.isEmpty() ? "—" : mode.m_antenna));
    m_antennaInfo->setText(mode.m_antenna.isEmpty() ? "—" : mode.m_antenna);

    m_updatingControls = true;
    m_frequency->setValue((quint64) mode.tunedFrequency());
    m_updatingControls = false;
}

void RBSWindow::setupDecodeView(const RBSMode& mode)
{
    m_messageCount = 0;
    m_callsToday.clear();
    m_farthestKm = 0.0;
    m_statsMessages->setText("0");
    m_statsCalls->setText("0");
    m_statsFarthest->setText("—");

    if (mode.m_view == "ft8")
    {
        m_decodesDock->setWindowTitle("Décodages FT8");
        m_highlightedOnly->setText("CQ seulement");
        m_decodeModel->setColumns({"UTC", "dB", "DT", "Δf", "Message", "Locator", "Distance"});
    }
    else if (mode.m_view == "aprs")
    {
        m_decodesDock->setWindowTitle("Stations APRS");
        m_highlightedOnly->setText("Nouvelles seulement");
        m_decodeModel->setColumns({"Heure", "Indicatif", "Vers", "Via", "Position", "Distance", "Commentaire"});
    }
    else if (mode.m_view == "adsb")
    {
        m_decodesDock->setWindowTitle("Avions ADS-B");
        m_highlightedOnly->setText("Nouveaux seulement");
        m_decodeModel->setColumns({"Vu", "Indicatif", "ICAO", "Altitude", "Cap", "Distance", "Infos"});
    }
    else
    {
        m_decodesDock->setWindowTitle("Décodages");
        m_decodeModel->setColumns({"Heure", "Message"});
    }

    m_decodeTable->resizeColumnsToContents();
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
    addEvent(QString("%1 lancé sur %2 MHz").arg(mode.m_name).arg(mode.tunedFrequency() / 1e6, 0, 'f', 3));
    attachSpectrum();
    readDeviceSettings();

    if (mode.m_view == "ft8") {
        m_ft8Log->start(m_controller->getDeviceSetIndex(), m_controller->getChannelIndex());
    } else if (mode.m_view == "adsb") {
        m_adsbTimer.start(1000);
    }
}

void RBSWindow::readDeviceSettings()
{
    int value;
    m_updatingControls = true;

    if (m_controller->getDeviceSetting("lnaIndex", value)) {
        m_lna->setValue(value);
    }
    if (m_controller->getDeviceSetting("ifAGC", value)) {
        m_ifAGC->setChecked(value != 0);
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

    DeviceSet *deviceSet = m_controller->getDeviceSet();
    bool hasRSP = deviceSet && deviceSet->m_deviceAPI && (deviceSet->m_deviceAPI->getHardwareId() == RBSController::m_hwType);
    bool running = m_controller->isDeviceRunning();

    m_play->blockSignals(true);
    m_play->setChecked(running);
    m_play->setText(running ? "■" : "▶");
    m_play->blockSignals(false);

    if (!hasRSP) {
        m_deviceStatus->setStyleSheet(m_controller->getState() == RBSController::Error ? "color: #ef4444;" : "color: #7f8a98;");
    } else {
        m_deviceStatus->setStyleSheet(running ? "color: #3ecf7a;" : "color: #f2c94c;");
    }

    m_biasTeeStatus->setStyleSheet(m_biasTee->isChecked() ? "color: #f2c94c;" : "color: #7f8a98;");
    m_antennaStatus->setStyleSheet(hasRSP ? "color: #3ecf7a;" : "color: #7f8a98;");

    if (!hasRSP) {
        return;
    }

    if (running && (m_controller->getState() == RBSController::Running)) {
        attachSpectrum(); // in case the device set was recreated
    }

    // Frequency, unless the user is turning the dial
    if (!m_frequency->hasFocus())
    {
        qint64 tuned = m_controller->getTunedFrequency();

        if ((tuned > 0) && ((quint64) tuned != m_frequency->getValueNew()))
        {
            m_updatingControls = true;
            m_frequency->setValue((quint64) tuned);
            m_updatingControls = false;
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
        QString rateText = rate >= 1000000 ? QString("%1 MS/s").arg(rate / 1e6, 0, 'f', 3) : QString("%1 kS/s").arg(rate / 1e3, 0, 'f', 1);
        m_sampleRate->setText(rateText);
        m_statusRate->setText(rateText);
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

void RBSWindow::onFT8Decode(const RBSFT8Log::Decode& decode)
{
    QString myCall = MainCore::instance()->getSettings().getStationName().trimmed().toUpper();
    RBSDecodeModel::Kind kind = RBSDecodeModel::Normal;

    if (decode.m_call1.startsWith("CQ")) {
        kind = RBSDecodeModel::Highlight;
    }
    if (!myCall.isEmpty() && (decode.m_call1.toUpper() == myCall)) {
        kind = RBSDecodeModel::Mine;
    }

    bool isLocator = (decode.m_loc.size() == 4) && (decode.m_loc != "RR73") && Maidenhead::isMaidenhead(decode.m_loc);
    double km = isLocator ? distanceKm(decode.m_loc) : -1.0;
    QString message = QString("%1 %2 %3").arg(decode.m_call1).arg(decode.m_call2).arg(decode.m_loc).trimmed();

    m_decodeModel->addRow({
        decode.m_dateTime.toString("HH:mm:ss"),
        QString::number(decode.m_snr),
        QString::number(decode.m_dt, 'f', 1),
        QString::number(decode.m_df),
        message,
        isLocator ? decode.m_loc : QString(),
        formatKm(km)
    }, kind);

    m_messageCount++;
    m_statsMessages->setText(QString::number(m_messageCount));

    if (!decode.m_call2.startsWith("<"))
    {
        m_callsToday.insert(decode.m_call2);
        m_statsCalls->setText(QString::number(m_callsToday.size()));
    }

    if (km > m_farthestKm)
    {
        m_farthestKm = km;
        m_statsFarthest->setText(QString("%1 · %2 (%3)").arg(decode.m_call2).arg(formatKm(km)).arg(decode.m_loc));
        addEvent(QString("Plus loin : %1 à %2").arg(decode.m_call2).arg(formatKm(km)));
    }

    if (kind == RBSDecodeModel::Mine) {
        addEvent(QString("Message pour toi : %1").arg(message));
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

        m_decodeModel->upsertRow(from, {
            time.toLocalTime().toString("HH:mm:ss"),
            from,
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
