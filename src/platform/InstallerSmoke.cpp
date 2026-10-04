#include "InstallerSmoke.h"
#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "ui/EditorPane.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QWebEngineProfile>
#include <QWindow>

namespace {
const QString noteTitle = QStringLiteral("Welcome to Nexus");
const QString noteMarker = QStringLiteral("Packaged installer verification");
const QString noteContent = QStringLiteral(
    "<h1>Welcome to Nexus</h1><p>Packaged installer verification</p>"
    "<p>This synthetic note verifies the deployed Qt WebEngine editor, SQLite "
    "database, and three-pane layout.</p>"
    "<h2>Release checklist</h2><ul><li>Install the native package</li>"
    "<li>Render and save this note</li><li>Capture the desktop for review</li></ul>"
    "<p><strong>No personal data is used in this capture.</strong></p>");

bool meaningfulImage(const QImage &image)
{
    if (image.width() < 800 || image.height() < 500) return false;
    const QRgb reference = image.pixel(0, 0);
    for (int y = 0; y < image.height(); y += 16)
        for (int x = 0; x < image.width(); x += 16)
            if (image.pixel(x, y) != reference) return true;
    return false;
}
}

InstallerSmoke::InstallerSmoke(const QString &outputDirectory)
    : m_outputDirectory(QFileInfo(outputDirectory).absoluteFilePath())
{
    m_poll.setInterval(250);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(60000);
    connect(&m_poll, &QTimer::timeout, this, &InstallerSmoke::checkEditor);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        fail("Timed out waiting for the packaged editor and screenshot capture.");
    });
}

bool InstallerSmoke::prepare()
{
    if (QFileInfo::exists(m_outputDirectory)) {
        qCritical() << "Installer smoke output directory must not already exist:"
                    << m_outputDirectory;
        return false;
    }
    if (!QDir().mkpath(m_outputDirectory)) {
        qCritical() << "Cannot create installer smoke output directory:" << m_outputDirectory;
        return false;
    }

    qApp->setApplicationName("NexusInstallerSmoke");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                      m_outputDirectory + "/settings");
    QStandardPaths::setTestModeEnabled(true);
    QWebEngineProfile::defaultProfile()->setPersistentStoragePath(m_outputDirectory + "/webengine");
    QWebEngineProfile::defaultProfile()->setCachePath(m_outputDirectory + "/cache");
    auto &db = DatabaseManager::instance();
    if (!db.initialize(m_outputDirectory + "/nexus.db")) {
        qCritical() << "Installer smoke database initialization failed.";
        return false;
    }
    QSqlQuery settings;
    if (!settings.exec("INSERT OR REPLACE INTO settings(key, value) VALUES "
                       "('check_updates', 'false'), ('auto_install_updates', 'false')")) {
        qCritical() << "Installer smoke cannot disable updates:" << settings.lastError();
        return false;
    }
    m_productId = db.addProduct("Release verification");
    const int otherProduct = db.addProduct("Example workspace");
    m_taskId = db.addTask(m_productId, noteTitle, TaskPriority::High,
                         QDateTime::currentDateTime().addDays(7));
    const int otherTask = db.addTask(m_productId, "Review native installer", TaskPriority::Medium);
    const int subtask = db.addSubtask(m_taskId, "Inspect screenshot artifacts");
    if (m_productId <= 0 || otherProduct <= 0 || m_taskId <= 0 || otherTask <= 0
        || subtask <= 0 || !db.updateTaskContent(m_taskId, noteContent)
        || !db.updateTaskWorkStatus(m_taskId, TaskWorkStatus::Ongoing)
        || !db.updateSubtaskWorkStatus(subtask, TaskWorkStatus::Completed)) {
        qCritical() << "Installer smoke could not create synthetic task data.";
        return false;
    }
    return true;
}

void InstallerSmoke::start(MainWindow *window)
{
    m_window = window;
    window->resize(1400, 800);
    auto *products = window->findChild<ProductPane *>();
    auto *tasks = window->findChild<TaskPane *>();
    auto *editor = window->findChild<EditorPane *>();
    auto *view = window->findChild<QWebEngineView *>();
    if (!products || !tasks || !editor || !view
        || !products->selectProductById(m_productId)) {
        QTimer::singleShot(0, this, [this]() { fail("Cannot select the synthetic note."); });
        return;
    }
    tasks->restoreView(m_productId, TaskPane::ViewMode::Active, m_taskId);
    editor->loadTask(m_taskId);
    connect(view, &QWebEngineView::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus, int exitCode) {
        fail(QString("Packaged WebEngine renderer terminated (%1).").arg(exitCode));
    });
    m_timeout.start();
    m_poll.start();
}

void InstallerSmoke::checkEditor()
{
    if (m_finished || m_checkPending) return;
    m_checkPending = true;
    auto *view = m_window->findChild<QWebEngineView *>();
    view->page()->runJavaScript(
        "(() => { const title = document.getElementById('page-title');"
        "const body = document.querySelector('.ProseMirror');"
        "return !!title && title.innerText === 'Welcome to Nexus' && !!body"
        "&& body.innerText.includes('Packaged installer verification')"
        "&& body.getBoundingClientRect().width > 100; })()",
        [this](const QVariant &result) {
            m_checkPending = false;
            if (m_finished || !result.toBool()) return;
            m_rendered = true;
            m_poll.stop();
            auto *editor = m_window->findChild<EditorPane *>();
            if (!editor->saveCurrentContent()
                || !DatabaseManager::instance().getTask(m_taskId).content.contains(noteMarker)) {
                fail("The rendered note could not be saved/read back.");
                return;
            }
            m_saved = true;
            // Wait for two Chromium animation frames before capturing the composed window.
            auto *view = m_window->findChild<QWebEngineView *>();
            view->page()->runJavaScript(
                "requestAnimationFrame(() => requestAnimationFrame(() => "
                "window.__nexusSmokePainted = true));");
            m_poll.disconnect(this);
            connect(&m_poll, &QTimer::timeout, this, [this]() {
                if (m_checkPending || m_finished) return;
                m_checkPending = true;
                m_window->findChild<QWebEngineView *>()->page()->runJavaScript(
                    "window.__nexusSmokePainted === true", [this](const QVariant &painted) {
                        m_checkPending = false;
                        if (m_finished || !painted.toBool()) return;
                        m_poll.stop();
                        capture();
                    });
            });
            m_poll.start();
        });
}

void InstallerSmoke::capture()
{
    const QImage appImage = m_window->grab().toImage();
    QScreen *screen = m_window->windowHandle() ? m_window->windowHandle()->screen() : nullptr;
    const QImage desktop = screen ? screen->grabWindow(0).toImage() : QImage();
    if (!meaningfulImage(appImage) || !meaningfulImage(desktop)) {
        fail("Native application/desktop screenshot is missing, too small or blank.");
        return;
    }
    if (!appImage.save(m_outputDirectory + "/app-window.png")
        || !desktop.save(m_outputDirectory + "/desktop.png")) {
        fail("Cannot write installer screenshot PNG files.");
        return;
    }
    if (!saveReport("passed", "Editor rendered, note saved and native screenshots captured.")) {
        fail("Cannot write installer verification report.");
        return;
    }
    m_finished = true;
    m_timeout.stop();
    qInfo() << "Installer smoke passed:" << m_outputDirectory;
    qApp->exit(0);
}

bool InstallerSmoke::saveReport(const QString &status, const QString &message)
{
    const QJsonObject report{
        {"status", status}, {"message", message},
        {"executable", QCoreApplication::applicationFilePath()},
        {"platform", QSysInfo::productType()},
        {"architecture", QSysInfo::currentCpuArchitecture()},
        {"qtVersion", QString::fromLatin1(qVersion())},
        {"applicationVersion", QCoreApplication::applicationVersion()},
        {"editorRendered", m_rendered}, {"databaseSaved", m_saved},
        {"database", DatabaseManager::instance().currentDbPath()},
        {"capturedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
    };
    QSaveFile file(m_outputDirectory + "/report.json");
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray data = QJsonDocument(report).toJson();
    return file.write(data) == data.size() && file.commit();
}

void InstallerSmoke::fail(const QString &message)
{
    if (m_finished) return;
    m_finished = true;
    m_poll.stop();
    m_timeout.stop();
    qCritical().noquote() << "Installer smoke:" << message;
    if (!saveReport("failed", message)) qCritical() << "Cannot save installer failure report.";
    qApp->exit(1);
}
