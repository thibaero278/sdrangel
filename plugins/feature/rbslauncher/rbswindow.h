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

#ifndef INCLUDE_FEATURE_RBSWINDOW_H_
#define INCLUDE_FEATURE_RBSWINDOW_H_

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QSet>
#include <QTimer>

#include "rbscontroller.h"
#include "rbsdecodemodel.h"
#include "rbsft8log.h"
#include "rbsmode.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTableView;
class QToolButton;
class QTreeWidget;
class QVBoxLayout;
class QWidget;
class GLSpectrum;
class MCPDataFeed;
class RBSFrequencyDial;
class RBSSpectrumTee;
class WebAPIAdapterInterface;

// The RBS window: an SDR++ style top bar and four panels (controls, spectrum and waterfall,
// decodes, info), shown in one of two layouts:
// - "Tout-en-un": fixed size side menu on the left, spectrum and waterfall taking all the rest,
//   decodes under them at a fixed height, as in SDR++
// - "Onglets": one tab per panel, each one full window, as the SDRangel workspaces
// The panels are moved between the two layouts; the layout used last is remembered.
class RBSWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum Layout {
        AllInOne,
        Tabs
    };

    explicit RBSWindow(WebAPIAdapterInterface *webAPIAdapterInterface, QWidget *parent = nullptr);
    ~RBSWindow() override;

    void startMode(const QString& modeId);

signals:
    void modeChanged(const QString& modeId);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // One station heard, for the view grouped by country
    struct Station
    {
        QDateTime m_lastHeard;
        QString m_lastMessage;
        int m_messages = 0;
        int m_bestSNR = -100;
        double m_km = -1.0;
    };

    RBSController *m_controller;
    QList<RBSMode> m_modes;
    Layout m_layout;
    bool m_updatingControls;

    // Top bar
    QToolButton *m_play;
    RBSFrequencyDial *m_frequency;
    QLabel *m_band;
    QProgressBar *m_level;
    QLabel *m_levelText;
    QLabel *m_deviceStatus;
    QToolButton *m_layoutAllInOne;
    QToolButton *m_layoutTabs;

    // Panels and the layouts they move between
    QWidget *m_controlsPanel;
    QWidget *m_spectrumPanel;
    QWidget *m_decodesPanel;
    QWidget *m_infoPanel;
    QStackedWidget *m_pages;
    QWidget *m_allInOnePage;
    QTabWidget *m_tabs;
    QVBoxLayout *m_sidebarBox;
    QVBoxLayout *m_centerBox;
    QVBoxLayout *m_tabSpectrumBox;
    QVBoxLayout *m_tabDecodesBox;
    QVBoxLayout *m_tabControlsBox;
    QVBoxLayout *m_tabInfoBox;

    // Source
    QComboBox *m_device;
    QWidget *m_sdrplayControls;
    QComboBox *m_sampleRate;
    QSlider *m_lna;
    QLabel *m_lnaText;
    QCheckBox *m_ifAGC;
    QSlider *m_ifGain;
    QLabel *m_ifGainText;
    QCheckBox *m_amNotch;
    QCheckBox *m_fmNotch;
    QCheckBox *m_dabNotch;
    QCheckBox *m_biasTee;

    // Radio
    QButtonGroup *m_demodGroup;
    QSpinBox *m_bandwidth;
    QSlider *m_squelch;
    QLabel *m_squelchText;
    QSlider *m_volume;
    QLabel *m_volumeText;
    QLabel *m_decoder;
    RBSController::Listen m_listen;
    QTimer m_listenUpdateTimer;

    // Modes
    QLineEdit *m_modeSearch;
    QComboBox *m_modeCategory;
    QListWidget *m_modeList;

    // My station
    QLineEdit *m_callsign;
    QLineEdit *m_locator;

    // Spectrum
    GLSpectrum *m_spectrum;
    RBSSpectrumTee *m_spectrumTee;
    QSlider *m_zoom;
    QSlider *m_refLevel;
    QSlider *m_range;
    qint64 m_spectrumCenter;
    int m_spectrumRate;

    // Decodes
    QLabel *m_decodesTitle;
    QLabel *m_distanceReference;
    RBSDecodeModel *m_decodeModel;
    RBSDecodeFilter *m_decodeFilter;
    QStackedWidget *m_decodeViews;
    QTableView *m_decodeTable;
    QTreeWidget *m_countryTree;
    QLineEdit *m_search;
    QCheckBox *m_groupByCountry;
    RBSFT8Log *m_ft8Log;
    MCPDataFeed *m_dataFeed;
    qint64 m_lastPacketSequence;
    QHash<QString, QHash<QString, Station>> m_stationsByCountry;
    bool m_countryTreeDirty;
    QTimer m_countryTreeTimer;

    // Info
    QLabel *m_statsMessages;
    QLabel *m_statsCalls;
    QLabel *m_statsCountries;
    QLabel *m_statsFarthest;
    QListWidget *m_events;
    QSet<QString> m_callsToday;
    int m_messageCount;
    double m_farthestKm;

    // Status bar
    QLabel *m_statusLocation;
    QLabel *m_statusRate;
    QLabel *m_statusState;
    QLabel *m_statusClock;

    QTimer m_pollTimer;
    QTimer m_adsbTimer;

    QWidget *createTopBar();
    QWidget *createControls();
    QWidget *createSpectrum();
    QWidget *createDecodes();
    QWidget *createInfo();
    void createPages();

    void setLayoutMode(Layout layout);
    void populateDevices();
    void populateModeList();
    void showModeSettings(const RBSMode& mode);
    void setupDecodeView(const RBSMode& mode);
    void readDeviceSettings();
    void attachSpectrum();
    void addEvent(const QString& text);
    void updateStationDisplay();

    void loadListen(const RBSMode& mode);
    void saveListen();
    void showListen();
    void onDemodChanged(const QString& demod);

    void addStation(const QString& call, const QString& message, int snr, double km);
    void rebuildCountryTree();
    static QString countryOf(const QString& callsign);

    double distanceKm(float latitude, float longitude) const;
    double distanceKm(const QString& locator) const;
    const RBSMode *findMode(const QString& modeId) const;

private slots:
    void onControllerState(RBSController::State state, const QString& message);
    void onModeStarted(const RBSMode& mode);
    void onFT8Decode(const RBSFT8Log::Decode& decode);
    void onPackets();
    void refreshAircraft();
    void poll();
    void exportCSV();
    void copySelection();
};

#endif // INCLUDE_FEATURE_RBSWINDOW_H_
