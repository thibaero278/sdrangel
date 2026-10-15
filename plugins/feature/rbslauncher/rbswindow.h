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

#include <QList>
#include <QMainWindow>
#include <QSet>
#include <QTimer>

#include "rbscontroller.h"
#include "rbsdecodemodel.h"
#include "rbsft8log.h"
#include "rbsmode.h"

class QCheckBox;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QSlider;
class QSpinBox;
class QTableView;
class QToolButton;
class GLSpectrum;
class ValueDial;
class MCPDataFeed;
class RBSSpectrumTee;
class WebAPIAdapterInterface;

// The RBS window: SDR++ style top bar, and four panels (controls, spectrum, decodes, info)
// that are QDockWidgets. In the "Tout-en-un" layout they are docked in this window; in the
// "Fenêtres" layout each one floats in its own window. Both layouts are remembered.
class RBSWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum Layout {
        AllInOne,
        Windows
    };

    explicit RBSWindow(WebAPIAdapterInterface *webAPIAdapterInterface, QWidget *parent = nullptr);
    ~RBSWindow() override;

    void startMode(const QString& modeId);

signals:
    void modeChanged(const QString& modeId);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    RBSController *m_controller;
    QList<RBSMode> m_modes;
    Layout m_layout;
    bool m_updatingControls;

    // Top bar
    QToolButton *m_play;
    ValueDial *m_frequency;
    QComboBox *m_modeCombo;
    QLabel *m_band;
    QProgressBar *m_level;
    QLabel *m_levelText;
    QLabel *m_deviceStatus;
    QLabel *m_antennaStatus;
    QLabel *m_biasTeeStatus;
    QToolButton *m_layoutAllInOne;
    QToolButton *m_layoutWindows;

    // Docks
    QDockWidget *m_controlsDock;
    QDockWidget *m_spectrumDock;
    QDockWidget *m_decodesDock;
    QDockWidget *m_infoDock;

    // Controls
    QLabel *m_sampleRate;
    QSpinBox *m_lna;
    QCheckBox *m_ifAGC;
    QCheckBox *m_amNotch;
    QCheckBox *m_fmNotch;
    QCheckBox *m_dabNotch;
    QCheckBox *m_biasTee;
    QLabel *m_demod;
    QLabel *m_decoder;
    QLabel *m_offset;
    QListWidget *m_modeList;

    // Spectrum
    GLSpectrum *m_spectrum;
    RBSSpectrumTee *m_spectrumTee;
    QSlider *m_refLevel;
    QSlider *m_range;
    qint64 m_spectrumCenter;
    int m_spectrumRate;

    // Decodes
    RBSDecodeModel *m_decodeModel;
    RBSDecodeFilter *m_decodeFilter;
    QTableView *m_decodeTable;
    QLineEdit *m_search;
    QCheckBox *m_highlightedOnly;
    RBSFT8Log *m_ft8Log;
    MCPDataFeed *m_dataFeed;
    qint64 m_lastPacketSequence;

    // Info
    QLabel *m_statsMessages;
    QLabel *m_statsCalls;
    QLabel *m_statsFarthest;
    QListWidget *m_events;
    QLabel *m_antennaInfo;
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
    QDockWidget *createDock(const QString& title, const QString& objectName, QWidget *widget);
    void applyStyle();

    void setLayoutMode(Layout layout, bool save = true);
    void saveLayout();
    bool restoreLayout(Layout layout);
    void arrangeAllInOne();
    void arrangeWindows();

    void setupDecodeView(const RBSMode& mode);
    void showModeSettings(const RBSMode& mode);
    void readDeviceSettings();
    void attachSpectrum();
    void addEvent(const QString& text);
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
