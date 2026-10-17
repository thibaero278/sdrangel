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
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QSlider;
class QSplitter;
class QSpinBox;
class QStackedWidget;
class QTabBar;
class QTabWidget;
class QTextEdit;
class ChannelMarker;
class QTableView;
class QToolButton;
class QTreeWidget;
class QVBoxLayout;
class QWidget;
class GLSpectrum;
class MCPDataFeed;
class RBSBandPopup;
class RBSCWTap;
class RBSModePopup;
class RBSChipDelegate;
class RBSFrequencyDial;
class RBSMeters;
class RBSSection;
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
        QString m_lastCall1;
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
    QWidget *m_frequencyBox;       //!< Dial and its unit, hidden with a sound card (no frequency)
    RBSFrequencyDial *m_frequency;
    QWidget *m_bandBox;
    QToolButton *m_bandButton;
    RBSBandPopup *m_bandPopup;
    RBSMeters *m_meters;
    double m_snrAverage;           //!< Smoothed S/N shown, < -50 when unknown
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
    QSplitter *m_centerSplitter;   //!< Spectrum over decodes, in the all in one layout
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
    QToolButton *m_modeButton;     //!< Top bar: the mode, opens the mode selector
    RBSModePopup *m_modePopup;

    // My station
    QLineEdit *m_callsign;
    QLineEdit *m_locator;

    // Spectrum
    GLSpectrum *m_spectrum;
    RBSSpectrumTee *m_spectrumTee;
    ChannelMarker *m_marker;       //!< Decoder passband on the spectrum; a click moves it when the mode is tunable
    QSlider *m_zoom;               //!< 0..600: x2^(v/100)
    QLabel *m_zoomText;
    QSlider *m_brightness;         //!< Half dB: the bottom of the scale goes down by value / 2 dB
    QLabel *m_brightnessText;
    QSlider *m_contrast;           //!< %: 100 - 0.8 x value dB from black to white
    QLabel *m_contrastText;
    QCheckBox *m_autoLevel;        //!< The bottom of the scale follows the noise floor
    QComboBox *m_colors;
    QTimer m_scaleTimer;           //!< Groups the waterfall redraws while a slider moves
    QTimer m_zoomTimer;
    double m_scaleFloor;           //!< Bottom of the scale before brightness, dB (noise floor in auto)
    qint64 m_spectrumCenter;
    int m_spectrumRate;
    QSlider *m_waterfallSpeed;
    QLabel *m_waterfallSpeedText;
    QSlider *m_smoothing;
    QLabel *m_smoothingText;
    int m_displayFFTSize;          //!< Spectrum resolution of the mode
    int m_displayLinesPerSecond;   //!< Waterfall speed and smoothing, per mode
    int m_displaySmoothing;

    // Decodes
    QLabel *m_decodesTitle;
    QLabel *m_distanceReference;
    RBSDecodeModel *m_decodeModel;
    RBSDecodeFilter *m_decodeFilter;
    QStackedWidget *m_decodeViews;
    QTabBar *m_decodeTabs;          //!< TABLEAU (MORSE DECK in Morse), TEXTE
    QStackedWidget *m_decodePages;
    QLabel *m_decodeCount;
    QToolButton *m_freeze;
    bool m_frozen;                 //!< "Figer": new decodes wait
    QList<RBSFT8Log::Decode> m_frozenFT8;
    QTextEdit *m_cwText;           //!< TEXTE tab: Morse decoded text, other decodes as lines
    QTextEdit *m_morseDeck;        //!< MORSE DECK tab: the Morse text, callsigns and usual words coloured
    RBSCWTap *m_cwTap;             //!< The RBS Morse decoder on the decoder channel's audio
    QString m_deckWord;            //!< Word being received, not coloured yet
    int m_deckWordStart;           //!< Its position in the deck, -1 if none
    float m_deckWordConfidence;    //!< Lowest confidence of its characters
    QString m_deckPreviousWord;
    QDateTime m_cwStatsShown;
    QLabel *m_cwStats;             //!< Pitch and speed detected
    QString m_cwBuffer;            //!< Last decoded characters, searched for callsigns
    QDateTime m_cwLastText;
    QTableView *m_decodeTable;
    QTreeWidget *m_countryTree;
    QLineEdit *m_search;
    QToolButton *m_groupByCountry;
    QLabel *m_newCountryBadge;
    RBSChipDelegate *m_call1Chip;
    RBSChipDelegate *m_call2Chip;
    RBSChipDelegate *m_messageChip;
    QString m_viewModeId;          //!< Mode the decodes shown belong to
    RBSFT8Log *m_ft8Log;
    MCPDataFeed *m_dataFeed;
    qint64 m_lastPacketSequence;
    QHash<QString, QHash<QString, Station>> m_stationsByCountry;
    bool m_countryTreeDirty;
    QHash<QString, QDateTime> m_countryFirstHeard;
    QSet<QString> m_seenCountries; //!< New countries the user opened
    QTimer m_countryTreeTimer;

    // Decoder panel (right): the mode, the options of its decoder, filters
    QWidget *m_decoderPanel;
    QLabel *m_modeTitle;
    QLabel *m_modeDemod;
    QLabel *m_modeGroup;
    QLabel *m_modeFrequencies;
    QFormLayout *m_modeForm;
    QLabel *m_modeDescription;
    RBSSection *m_decoderSection;
    QWidget *m_ft8Options;
    QWidget *m_cwOptions;
    QWidget *m_aprsOptions;
    QWidget *m_adsbOptions;
    QCheckBox *m_ft8CQOnly;
    QCheckBox *m_ft8MineOnly;
    QCheckBox *m_cwAuto;
    QSlider *m_cwFilter;
    QLabel *m_cwFilterText;
    int m_cwMinConfidence;         //!< % : Morse text decoded with less confidence is not shown
    QCheckBox *m_aprsPositionOnly;
    QCheckBox *m_adsbPositionOnly;
    QVBoxLayout *m_rightBox;
    QVBoxLayout *m_tabDecoderBox;

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
    QWidget *createDecoderPanel();
    void updateDecoderPanel(const RBSMode& mode);
    void appendTextLine(const QString& time, const QString& text);
    void createPages();

    void setLayoutMode(Layout layout);
    void populateDevices();

    void showModeSettings(const RBSMode& mode);
    void setupDecodeView(const RBSMode& mode);
    void readDeviceSettings();
    void attachSpectrum();
    void addEvent(const QString& text);
    void updateStationDisplay();
    void tuneTo(qint64 frequency);
    void showTuned(qint64 frequency);
    void setupMarker(const RBSMode& mode);
    void onMarkerMoved();
    void onCWDecoded();
    void addMorseCharacter(const QString& text, float confidence);
    void finishDeckWord();
    void startMorseLine(QTextEdit *edit, const QString& time);
    void trimText(QTextEdit *edit, int maxBlocks);
    void updateSourceKind();
    void findCallsigns(int newFrom);
    void applyZoom();
    void applyScale();
    void loadDisplay(const RBSMode& mode);
    void saveDisplay();
    void applySpectrumSettings();
    void updateCountryBadge();
    bool isNewCountry(const QString& country) const;

    void loadListen(const RBSMode& mode);
    void saveListen();
    void showListen();
    void onDemodChanged(const QString& demod);

    void addStation(const QString& call, const QString& call1, const QString& message, int snr, double km);
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
