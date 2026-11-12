#include "window.hpp"
#include "platform.hpp"
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <unistd.h>

Window::Window() {
    setWindowTitle("WiredPad");
    resize(560, 480);
    setMinimumWidth(420);
    hostPath_ = QCoreApplication::applicationDirPath() + "/wiredpad-host";
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);
    auto heading = new QLabel("WiredPad");
    auto font = heading->font();
    font.setPointSize(24);
    font.setBold(true);
    heading->setFont(font);
    root->addWidget(heading);
    auto intro = new QLabel("Use your tablet as an extra display over USB.");
    intro->setWordWrap(true);
    root->addWidget(intro);
    auto panel = new QFrame;
    panel->setFrameShape(QFrame::StyledPanel);
    auto state = new QVBoxLayout(panel);
    state->setContentsMargins(16, 16, 16, 16);
    status_ = new QLabel("Ready");
    font = status_->font();
    font.setBold(true);
    status_->setFont(font);
    detail_ = new QLabel("Connect your iPad and open Wired XDisplay, then start sharing.");
    detail_->setWordWrap(true);
    state->addWidget(status_);
    state->addWidget(detail_);
    root->addWidget(panel);
    auto form = new QFormLayout;
    form->setSpacing(12);
    fps_ = new QSpinBox;
    fps_->setRange(1, 60);
    fps_->setSuffix(" fps");
    fps_->setValue(settings_.value("fps", 30).toInt());
    fps_->setAccessibleName("Frame rate");
    form->addRow("&Frame rate", fps_);
    input_ = new QCheckBox("Enable tablet input");
    input_->setChecked(settings_.value("input", true).toBool());
    form->addRow("", input_);
    output_ = new QComboBox;
    output_->setAccessibleName("Input display");
    form->addRow("&Input display", output_);
    desiredOutput_ = settings_.value("output").toString();
    root->addLayout(form);
    mapping_ = new QLabel("The tablet output appears after connecting.");
    mapping_->setWordWrap(true);
    root->addWidget(mapping_);
    start_ = new QPushButton("&Start display");
    start_->setMinimumHeight(44);
    start_->setDefault(true);
    root->addWidget(start_);
    auto actions = new QHBoxLayout;
    load_ = new QPushButton("Load display driver");
    actions->addWidget(load_);
    auto display = new QPushButton("Display settings");
    actions->addWidget(display);
    auto help = new QPushButton("Help");
    actions->addWidget(help);
    root->addLayout(actions);
    auto diagnostics = new QToolButton;
    diagnostics->setText("Diagnostics");
    diagnostics->setCheckable(true);
    diagnostics->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    diagnostics->setArrowType(Qt::RightArrow);
    root->addWidget(diagnostics);
    logs_ = new QPlainTextEdit;
    logs_->setReadOnly(true);
    logs_->setMaximumBlockCount(300);
    logs_->setMinimumHeight(160);
    logs_->setAccessibleName("Connection diagnostics");
    logs_->hide();
    root->addWidget(logs_);
    connect(diagnostics, &QToolButton::toggled, this, [this, diagnostics](bool on) {
        logs_->setVisible(on);
        diagnostics->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });
    connect(start_, &QPushButton::clicked, this, [this] {
        if (host_.state() == QProcess::NotRunning)
            start();
        else
            stop();
    });
    connect(load_, &QPushButton::clicked, this, &Window::loadDriver);
    connect(help, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://github.com/Michy234/WiredPad#readme")); });
    connect(display, &QPushButton::clicked, this, [] {
        if (!QStandardPaths::findExecutable("kcmshell6").isEmpty())
            QProcess::startDetached("kcmshell6", {"kcm_kscreen"});
        else if (!QStandardPaths::findExecutable("gnome-control-center").isEmpty())
            QProcess::startDetached("gnome-control-center", {"display"});
        else
            QDesktopServices::openUrl(
                QUrl("https://github.com/Michy234/WiredPad/blob/main/docs/setup.md"));
    });
    connect(output_, &QComboBox::currentIndexChanged, this, [this] {
        desiredOutput_ = output_->currentData().toString();
        mapped_.clear();
        mapInput();
    });
    host_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&host_, &QProcess::readyReadStandardOutput, this, &Window::consumeOutput);
    connect(&host_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        appendLog(host_.errorString());
        setStatus("Could not start", host_.errorString());
        refresh();
    });
    connect(&host_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        consumeOutput();
        start_->setText("&Start display");
        start_->setEnabled(true);
        fps_->setEnabled(true);
        input_->setEnabled(true);
        if (stopping_)
            setStatus("Stopped", "Your tablet display has been disconnected.");
        else
            setStatus("Display stopped",
                      QString("The host exited (%1). Open Diagnostics for details.").arg(code));
        stopping_ = false;
        mapped_.clear();
        if (closing_)
            close();
    });
    driver_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&driver_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        appendLog(QString::fromUtf8(driver_.readAll()));
        load_->setEnabled(true);
        if (code == 0)
            setStatus("Driver loaded", "Connect your iPad and start the display.");
        else
            setStatus("Driver not loaded", "Install EVDI and matching kernel headers, or check "
                                           "Diagnostics. Authentication may have been cancelled.");
        refresh();
    });
    connect(&driver_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setStatus("Could not load driver", driver_.errorString());
        load_->setEnabled(true);
    });
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { updateScreens(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { updateScreens(); });
    connect(&refresh_, &QTimer::timeout, this, [this] {
        refresh();
        if (host_.state() != QProcess::NotRunning)
            mapInput();
    });
    refresh_.start(2000);
    updateScreens();
    refresh();
}
QStringList Window::cards() const {
    QStringList result;
    for (int number : xd::evdi_cards())
        result << "card" + QString::number(number);
    return result;
}

QString Window::virtualOutput() const {
    const auto devices = cards();
    if (devices.isEmpty())
        return {};
    const QString prefix = devices.first() + "-";
    for (const auto &name :
         QDir("/sys/class/drm").entryList({prefix + "*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto candidate = name.mid(prefix.size());
        for (auto screen : QGuiApplication::screens())
            if (screen->name() == candidate)
                return candidate;
    }
    return {};
}
void Window::updateScreens() {
    output_->blockSignals(true);
    output_->clear();
    output_->addItem("Automatic (tablet display)", QString());
    for (auto screen : QGuiApplication::screens())
        output_->addItem(screen->name(), screen->name());
    const int index = output_->findData(desiredOutput_);
    output_->setCurrentIndex(index >= 0 ? index : 0);
    output_->blockSignals(false);
    mapped_.clear();
}
void Window::refresh() {
    const bool stopped = host_.state() == QProcess::NotRunning;
    load_->setVisible(cards().isEmpty());
    if (stopped && !stopping_ && status_->text() == "Ready" && cards().isEmpty())
        setStatus(
            "Display driver needed",
            "Install EVDI for your kernel, then use Load display driver. See Help for setup.");
}
void Window::setStatus(const QString &title, const QString &detail) {
    status_->setText(title);
    detail_->setText(detail);
}
void Window::appendLog(const QString &line) {
    if (!line.trimmed().isEmpty())
        logs_->appendPlainText(line.trimmed());
}
void Window::startWhenReady() {
    QTimer::singleShot(0, this, [this] { start(); });
}
void Window::start() {
    if (host_.state() != QProcess::NotRunning)
        return;
    auto devices = cards();
    if (devices.isEmpty()) {
        setStatus("Display driver needed",
                  "Load EVDI first. The module must match your running kernel.");
        return;
    }
    if (devices.size() > 1) {
        setStatus("Multiple virtual displays",
                  "Use wiredpad-host --evdi-card N to select a dedicated EVDI device. See Help.");
        return;
    }
    if (!QFileInfo::exists("/var/run/usbmuxd")) {
        setStatus(
            "USB service needed",
            "Start usbmuxd.service, then connect and trust your iPad. See Help for instructions.");
        return;
    }
    if (input_->isChecked() && ::access("/dev/uinput", W_OK) != 0) {
        setStatus("Input permission needed", "Log out and back in after installing the package, or "
                                             "disable tablet input for video only.");
        return;
    }
    const auto card = devices.first();
    if (::access(("/dev/dri/" + card).toLocal8Bit().constData(), R_OK | W_OK) != 0) {
        setStatus("Display permission needed",
                  "Start WiredPad inside your active desktop session. Check access to " + card +
                      ".");
        return;
    }
    settings_.setValue("fps", fps_->value());
    settings_.setValue("input", input_->isChecked());
    settings_.setValue("output", desiredOutput_);
    QStringList args{"--evdi-card", card.mid(4), "--fps", QString::number(fps_->value())};
    if (!input_->isChecked())
        args << "--no-input";
    pending_.clear();
    mapped_.clear();
    stopping_ = false;
    fps_->setEnabled(false);
    input_->setEnabled(false);
    start_->setText("&Stop display");
    setStatus("Waiting for your iPad",
              "Connect the USB cable, unlock your iPad, and open Wired XDisplay.");
    auto env = QProcessEnvironment::systemEnvironment();
    QString localLib = QDir::homePath() + "/.local/lib";
    QString currentLd = env.value("LD_LIBRARY_PATH");
    if (!currentLd.contains(localLib)) {
        env.insert("LD_LIBRARY_PATH", localLib + (currentLd.isEmpty() ? "" : ":" + currentLd));
    }
    host_.setProcessEnvironment(env);
    host_.start(hostPath_, args);
}
void Window::stop() {
    if (host_.state() == QProcess::NotRunning)
        return;
    stopping_ = true;
    start_->setEnabled(false);
    setStatus("Stopping", "Disconnecting the tablet display…");
    host_.terminate();
    QTimer::singleShot(7000, this, [this] {
        if (stopping_ && host_.state() != QProcess::NotRunning)
            host_.kill();
    });
}
void Window::consumeOutput() {
    pending_ += QString::fromUtf8(host_.readAllStandardOutput());
    int newline;
    while ((newline = pending_.indexOf('\n')) >= 0) {
        const auto line = pending_.left(newline).trimmed();
        pending_.remove(0, newline + 1);
        appendLog(line);
        if (stopping_)
            continue;
        if (line.startsWith("iPad "))
            setStatus("iPad connected",
                      "Enable the new output in Display settings. " + line.section(' ', 1, 1));
        else if (line.startsWith("stream frames=")) {
            auto match = QRegularExpression("frames=([0-9]+)").match(line);
            if (match.hasMatch() && match.captured(1).toULongLong() > 0)
                setStatus("Display active", "Your desktop is being sent to the tablet over USB.");
            else
                setStatus("Waiting for display output",
                          "Enable the tablet output in Display settings.");
        } else if (line.contains("retry in 2 seconds"))
            setStatus("Reconnecting", line.section("; retry", 0, 0));
    }
}
void Window::mapInput() {
    if (!input_->isChecked()) {
        mapping_->setText("Tablet input is disabled.");
        return;
    }
    const auto output = desiredOutput_.isEmpty() ? virtualOutput() : desiredOutput_;
    if (output.isEmpty()) {
        mapping_->setText("The tablet output appears after connecting.");
        return;
    }
    const bool kwin =
        QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.KWin");
    int matched = 0;
    for (const auto &event :
         QDir("/sys/class/input").entryList({"event*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile file("/sys/class/input/" + event + "/device/name");
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const auto name = QString::fromUtf8(file.readAll()).trimmed();
        if (name != "WiredPad Touch" && name != "WiredPad Pointer")
            continue;
        const auto key = event + ":" + output;
        if (mapped_.contains(key)) {
            ++matched;
            continue;
        }
        if (kwin) {
            QDBusInterface device("org.kde.KWin", "/org/kde/KWin/InputDevice/" + event,
                                  "org.kde.KWin.InputDevice", QDBusConnection::sessionBus());
            device.setTimeout(500);
            if (device.isValid() && device.setProperty("outputName", output) &&
                device.property("outputName").toString() == output) {
                mapped_.insert(key);
                ++matched;
            }
        } else if (QGuiApplication::platformName() == "xcb" &&
                   !QStandardPaths::findExecutable("xinput").isEmpty()) {
            auto mapper = new QProcess(this);
            mapper->start("xinput", {"map-to-output", name, output});
            mapped_.insert(key);
            connect(mapper, &QProcess::finished, this,
                    [this, mapper, key](int code, QProcess::ExitStatus) {
                        if (code != 0) {
                            mapped_.remove(key);
                            appendLog(QString::fromUtf8(mapper->readAllStandardError()));
                        }
                        mapper->deleteLater();
                    });
        }
    }
    if (matched == 2)
        mapping_->setText("Tablet input is mapped to " + output + ".");
    else if (kwin)
        mapping_->setText("Waiting to map tablet input to " + output + "…");
    else
        mapping_->setText("Map tablet input to " + output + " in your desktop settings. See Help.");
}
void Window::loadDriver() {
    const auto pkexec = QStandardPaths::findExecutable("pkexec");
    QString modprobe =
        QFileInfo::exists("/usr/bin/modprobe") ? "/usr/bin/modprobe" : "/usr/sbin/modprobe";
    if (pkexec.isEmpty()) {
        setStatus("Administrator action needed",
                  "Run sudo modprobe evdi initial_device_count=1, then try again.");
        return;
    }
    load_->setEnabled(false);
    setStatus("Loading display driver", "Your desktop may ask for administrator authentication.");
    driver_.start(pkexec, {modprobe, "evdi", "initial_device_count=1"});
}
void Window::closeEvent(QCloseEvent *event) {
    if (host_.state() != QProcess::NotRunning) {
        closing_ = true;
        event->ignore();
        stop();
        return;
    }
    if (driver_.state() != QProcess::NotRunning)
        driver_.terminate();
    event->accept();
}
