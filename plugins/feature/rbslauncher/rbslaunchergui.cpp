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

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "feature/featureuiset.h"
#include "gui/rollupcontents.h"
#include "mainwindow.h"

#include "rbslauncher.h"
#include "rbslaunchergui.h"
#include "rbswindow.h"

RBSLauncherGUI* RBSLauncherGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    return new RBSLauncherGUI(pluginAPI, featureUISet, feature);
}

void RBSLauncherGUI::destroy()
{
    delete this;
}

RBSLauncherGUI::RBSLauncherGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    m_rbsLauncher(reinterpret_cast<RBSLauncher*>(feature)),
    m_windowShownOnce(false)
{
    (void) pluginAPI;
    (void) featureUISet;

    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/rbslauncher/readme.md";

    RollupContents *rollupContents = getRollupContents();
    QWidget *widget = new QWidget(rollupContents);
    widget->setObjectName("rbsLauncherWidget");
    widget->setWindowTitle("RBS");
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    QLabel *text = new QLabel("Interface simplifiée RBS : spectre, décodages et modes en un clic.", widget);
    text->setWordWrap(true);
    layout->addWidget(text);

    QPushButton *open = new QPushButton("Ouvrir la fenêtre RBS", widget);
    connect(open, &QPushButton::clicked, this, &RBSLauncherGUI::openWindow);
    layout->addWidget(open);

    m_openOnStart = new QCheckBox("Ouvrir RBS au démarrage", widget);
    connect(m_openOnStart, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.m_openOnStart = checked;
        applySettings({"openOnStart"});
    });
    layout->addWidget(m_openOnStart);

    rollupContents->arrangeRollups();
    sizeToContents();

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    m_rbsLauncher->setMessageQueueToGUI(&m_inputMessageQueue);
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();

    // Wait for the configuration to be applied (deserialize) before deciding whether to open
    QTimer::singleShot(500, this, [this]() {
        if (m_settings.m_openOnStart && !m_windowShownOnce) {
            openWindow();
        }
    });
}

RBSLauncherGUI::~RBSLauncherGUI()
{
    m_rbsLauncher->setMessageQueueToGUI(nullptr);
    delete m_window;
}

void RBSLauncherGUI::openWindow()
{
    if (!m_window)
    {
        m_window = new RBSWindow(m_rbsLauncher->getWebAPIAdapterInterface());
        connect(m_window, &RBSWindow::modeChanged, this, [this](const QString& modeId) {
            m_settings.m_lastModeId = modeId;
            applySettings({"lastModeId"});
        });
        m_window->show();
        m_window->startMode(m_settings.m_lastModeId);
    }
    else
    {
        m_window->show();
        m_window->showNormal();
    }

    m_windowShownOnce = true;
    m_window->raise();
    m_window->activateWindow();

    // RBS is the visible application, SDRangel the engine behind it: hide its window
    // ("Réglages avancés dans SDRangel" in RBS shows it again)
    if (MainWindow *mainWindow = MainWindow::getInstance()) {
        mainWindow->hide();
    }
}

void RBSLauncherGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray RBSLauncherGUI::serialize() const
{
    return m_settings.serialize();
}

bool RBSLauncherGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    }

    resetToDefaults();
    return false;
}

void RBSLauncherGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

bool RBSLauncherGUI::handleMessage(const Message& message)
{
    if (RBSLauncher::MsgConfigureRBSLauncher::match(message))
    {
        const RBSLauncher::MsgConfigureRBSLauncher& cfg = (const RBSLauncher::MsgConfigureRBSLauncher&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }

    return false;
}

void RBSLauncherGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("RBSLauncherGUI::handleInputMessages: unhandled message: %s", message->getIdentifier());
        }

        delete message;
    }
}

void RBSLauncherGUI::displaySettings()
{
    setTitle(m_settings.m_title);
    setWindowTitle(m_settings.m_title);
    m_openOnStart->blockSignals(true);
    m_openOnStart->setChecked(m_settings.m_openOnStart);
    m_openOnStart->blockSignals(false);
}

void RBSLauncherGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_rbsLauncher->getInputMessageQueue()->push(RBSLauncher::MsgConfigureRBSLauncher::create(m_settings, settingsKeys, force));
}

void RBSLauncherGUI::onMenuDialogCalled(const QPoint& p)
{
    (void) p;
    resetContextMenuType();
}
