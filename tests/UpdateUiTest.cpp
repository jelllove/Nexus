#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "services/UpdateService.h"
#include "ui/EditorPane.h"
#include "ui/SettingsDialog.h"
#include "platform/GlobalHotkey.h"
#include "FakeUpdateNetwork.h"
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QToolButton>
#include <QtTest>
#include <limits>
#include <memory>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

class TestUrlHandler : public QObject
{
    Q_OBJECT
public:
    QUrl opened;
    QList<QUrl> requests;
    explicit TestUrlHandler(const QString &scheme = "file")
        : m_scheme(scheme)
    {
        QDesktopServices::setUrlHandler(m_scheme, this, "openUrl");
    }
    ~TestUrlHandler() override { QDesktopServices::unsetUrlHandler(m_scheme); }
public slots:
    void openUrl(const QUrl &url) { opened = url; requests.append(url); }
private:
    QString m_scheme;
};

class UpdateUiTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_data;
    std::unique_ptr<TestNetwork> m_network;
    std::unique_ptr<UpdateService> m_updater;
    std::unique_ptr<MainWindow> m_window;
    QStringList m_downloads;
    int m_updateAnswers = 0;
    int m_expectedMessageAnswers = 0;
    QPointer<QTimer> m_answerTimer;
    TestUrlHandler m_downloadPageHandler{"https"};
    QString m_lastMessageText;
    Qt::TextInteractionFlags m_lastMessageFlags;
    int m_taskId = -1;
    int m_subtaskId = -1;

    QVariant editorScript(const QString &script)
    {
        auto *view = m_window->findChild<QWebEngineView *>();
        QEventLoop loop;
        QVariant result;
        auto alive = std::make_shared<bool>(true);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        view->page()->runJavaScript(script, [&, alive](const QVariant &value) {
            if (!*alive) return;
            result = value;
            loop.quit();
        });
        loop.exec();
        *alive = false;
        return result;
    }

    void showRenderedEditor()
    {
        m_window->show();
        QTRY_VERIFY_WITH_TIMEOUT(editorScript("typeof editor !== 'undefined' && !!editor && !!document.querySelector('.ProseMirror')").toBool(), 10000);
    }

    QString updateButtonText() const
    {
#ifdef Q_OS_WIN
        return "Install now";
#else
        return "Open update";
#endif
    }

    void captureDialog(QWidget *dialog, const QString &fileName)
    {
        const QString directory = qEnvironmentVariable("NEXUS_UPDATE_TEST_SCREENSHOTS");
        if (!directory.isEmpty()) {
            QVERIFY(dialog->grab().save(QDir(directory).filePath(fileName)));
        }
    }

    void triggerManualCheck()
    {
        for (auto *action : m_window->findChildren<QAction *>()) {
            if (action->text() == "Check for &Updates...") {
                action->trigger();
                return;
            }
        }
        QFAIL("Manual update action is missing");
    }

    void createWindow()
    {
        m_network = std::make_unique<TestNetwork>();
        m_updater = std::make_unique<UpdateService>(m_network.get());
        connect(m_updater.get(), &UpdateService::downloadFinished, this,
                [this](const QString &path) { m_downloads.append(path); });
        m_window = std::make_unique<MainWindow>(nullptr, m_updater.get());
        m_window->hide();
    }

    void answerUpdate(bool download, int months = 0, bool close = false)
    {
        m_answerTimer = new QTimer(this);
        connect(m_answerTimer, &QTimer::timeout, this, [this, download, months, close]() {
            auto *dialog = m_window->findChild<QDialog *>("updateAvailableDialog");
            if (!dialog || !dialog->isVisible()) return;
            m_answerTimer->stop();
            ++m_updateAnswers;
            auto *delay = dialog->findChild<QComboBox *>("updateReminderDelay");
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(delay);
            QVERIFY(buttons);
            QCOMPARE(delay->count(), 4);
            delay->setCurrentIndex(delay->findData(months));
            QCOMPARE(delay->currentData().toInt(), months);
            captureDialog(dialog, QString("update-prompt-%1-months.png").arg(months));
            QVERIFY(buttons->button(QDialogButtonBox::No)->isDefault());
            // A second result must not open another prompt while this one is active.
            emit m_updater->updateAvailable("v1.0.9", installerUrl("v1.0.9"), {}, true);
            if (close) dialog->close();
            else buttons->button(download ? QDialogButtonBox::Yes : QDialogButtonBox::No)->click();
        });
        m_answerTimer->start(10);
    }

    void checkRelease(bool manual, const QString &version = "v1.0.8")
    {
        if (!manual) {
            m_updater->setAutomaticChecksEnabled(true);
            QCoreApplication::processEvents();
        }
        m_updater->checkForUpdate(manual);
        QVERIFY(m_network->reply);
        m_network->reply->finish(releaseData(version));
        if (m_answerTimer) {
            m_answerTimer->stop();
            m_answerTimer->deleteLater();
            m_answerTimer.clear();
        }
        QCoreApplication::processEvents();
    }

    QProgressDialog *downloadDialog() const
    {
        return m_window->findChild<QProgressDialog *>("updateDownloadProgress");
    }

    QPushButton *updateButton() const
    {
        return m_window->findChild<QPushButton *>("updateButton");
    }

    void answerDialogs(const QStringList &answers)
    {
        m_expectedMessageAnswers += answers.size();
        auto *timer = new QTimer(this);
        auto pending = std::make_shared<QStringList>(answers);
        connect(timer, &QTimer::timeout, this, [this, timer, pending]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box || pending->isEmpty()) return;
            for (auto *button : box->buttons()) {
                if (button->text() == pending->first()) {
                    m_lastMessageText = box->text();
                    m_lastMessageFlags = box->textInteractionFlags();
                    pending->removeFirst();
                    --m_expectedMessageAnswers;
                    button->click();
                    if (pending->isEmpty()) {
                        timer->stop();
                        timer->deleteLater();
                    }
                    return;
                }
            }
        });
        timer->start(10);
        QTimer::singleShot(5000, timer, [timer, pending]() {
            if (!pending->isEmpty()) {
                QTest::qFail("Expected update dialog was not displayed", __FILE__, __LINE__);
                if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                    box->reject();
                timer->stop();
                timer->deleteLater();
            }
        });
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName("NexusUpdateTest");
        QCoreApplication::setOrganizationName("NexusUpdateTest");
        QCoreApplication::setApplicationVersion("1.0.7");
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_data.isValid());
        auto &db = DatabaseManager::instance();
        QVERIFY(db.initialize(m_data.filePath("test.db")));
        db.setSetting("check_updates", "false");
        const int productId = db.addProduct("Update tests");
        m_taskId = db.addTask(productId, "Test task");
        m_subtaskId = db.addSubtask(m_taskId, "Test subtask");
        QVERIFY(m_taskId > 0);
        QVERIFY(m_subtaskId > 0);
    }

    void init()
    {
        auto &db = DatabaseManager::instance();
        db.setSetting("check_updates", "false");
        db.setSetting("auto_install_updates", "true");
        db.setSetting("update_reminder_after", "");
        m_updateAnswers = 0;
        m_expectedMessageAnswers = 0;
        m_lastMessageText.clear();
        m_lastMessageFlags = Qt::NoTextInteraction;
        m_downloadPageHandler.requests.clear();
        QDesktopServices::setUrlHandler("https", &m_downloadPageHandler, "openUrl");
        createWindow();
        QVERIFY(!m_updater->findChild<QTimer *>("updateCheckTimer")->isActive());
    }

    void cleanup()
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        m_window.reset();
        m_updater.reset();
        m_network.reset();
        for (const QString &path : m_downloads) {
            if (QFile::exists(path)) QVERIFY(QFile::remove(path));
        }
        m_downloads.clear();
        QCOMPARE(m_expectedMessageAnswers, 0);
    }

    void editorLinkHoverShowsDestinationAndClears_data()
    {
        QTest::addColumn<QString>("url");
        QTest::newRow("https") << QString("https://example.com/docs?q=hello%20world#section");
        QTest::newRow("email") << QString("mailto:hello@example.com");
        QTest::newRow("long-url") << (QString("https://example.com/") + QString(500, 'a'));
    }

    void editableLinkHoverReachesStatusBar()
    {
        showRenderedEditor();
        QVERIFY(editorScript(
            "editor.commands.setContent('<p><a href=\"https://example.com/hover\"><strong>Hover destination</strong></a> plain text</p>'); "
            "const target = document.querySelector('.ProseMirror a strong'); "
            "target.dispatchEvent(new MouseEvent('mouseover', { bubbles: true })); "
            "true; ").toBool());
        QTRY_COMPARE(m_window->statusBar()->currentMessage(), QString("https://example.com/hover"));
        editorScript("document.querySelector('.ProseMirror a strong').dispatchEvent(new MouseEvent('mouseout', { bubbles: true, relatedTarget: document.querySelector('#ribbon') }));");
        QTRY_VERIFY(m_window->statusBar()->currentMessage().isEmpty());
    }

    void hyperlinkDialogInsertsEditsAndRemovesLinks()
    {
        showRenderedEditor();
        QVERIFY(editorScript(
            "editor.commands.setContent('<p>Selected words</p>'); "
            "editor.commands.setTextSelection({ from: 1, to: 15 }); "
            "document.querySelector('[onclick=\"switchTab(\\'insert\\')\"]').click(); "
            "const button = document.getElementById('btn-link'); "
            "if (button) button.click(); "
            "!!button && document.getElementById('link-dialog').open; ").toBool());
        QCOMPARE(editorScript("document.getElementById('link-text').value").toString(), QString("Selected words"));
        QVERIFY(editorScript(
            "document.getElementById('link-url').value = 'https://example.com/first'; "
            "document.getElementById('link-form').requestSubmit(); "
            "document.querySelector('.ProseMirror a').getAttribute('href') === 'https://example.com/first'; ").toBool());
        QVERIFY(editorScript(
            "editor.commands.setTextSelection(3); "
            "document.getElementById('btn-link').click(); "
            "document.getElementById('link-url').value === 'https://example.com/first' && "
            "document.getElementById('link-text').value === 'Selected words'; ").toBool());
        QVERIFY(editorScript(
            "document.getElementById('link-url').value = 'https://example.com/edited'; "
            "document.getElementById('link-text').value = 'Edited label'; "
            "document.getElementById('link-form').requestSubmit(); "
            "document.querySelector('.ProseMirror a').textContent === 'Edited label' && "
            "document.querySelector('.ProseMirror a').getAttribute('href') === 'https://example.com/edited'; ").toBool());
        QVERIFY(editorScript(
            "editor.commands.setTextSelection(3); "
            "document.getElementById('btn-link').click(); "
            "document.getElementById('link-remove').click(); "
            "!document.querySelector('.ProseMirror a') && editor.getText() === 'Edited label'; ").toBool());
    }

    void hyperlinkShortcutCancelValidationAndPersistence()
    {
        showRenderedEditor();
        auto *pane = m_window->findChild<EditorPane *>();
        pane->loadTask(m_taskId);
        QTRY_VERIFY(editorScript("document.getElementById('page-title').textContent === 'Test task'").toBool());
        QVERIFY(editorScript(
            "editor.commands.setContent('<p><strong>Selected words</strong></p>');"
            "editor.commands.setTextSelection({ from: 1, to: 15 });"
            "document.querySelector('.ProseMirror').dispatchEvent(new KeyboardEvent('keydown', { key: 'k', ctrlKey: true, bubbles: true }));"
            "document.getElementById('link-dialog').open;").toBool());
        QVERIFY(editorScript(
            "document.getElementById('link-url').value = 'https://example.com/cancel';"
            "document.getElementById('link-cancel').click();"
            "!document.getElementById('link-dialog').open && !document.querySelector('.ProseMirror a') && editor.state.selection.from === 1 && editor.state.selection.to === 15;").toBool());
        QVERIFY(editorScript(
            "showLinkDialog();"
            "document.getElementById('link-url').value = 'javascript:alert(1)';"
            "document.getElementById('link-form').requestSubmit();"
            "document.getElementById('link-dialog').open && !!document.getElementById('link-error').textContent && !document.querySelector('.ProseMirror a');").toBool());
        QVERIFY(editorScript(
            "document.getElementById('link-url').value = 'https://example.com/saved';"
            "document.getElementById('link-form').requestSubmit();"
            "!!document.querySelector('.ProseMirror strong a, .ProseMirror a strong');").toBool());
        auto *bridge = pane->findChild<EditorBridge *>();
        QTRY_VERIFY(bridge->content().contains("https://example.com/saved"));
        QVERIFY(pane->saveCurrentContent());
        QVERIFY(DatabaseManager::instance().getTask(m_taskId).content.contains("https://example.com/saved"));
        QVERIFY(editorScript(
            "editor.commands.setContent('<p></p>');"
            "editor.commands.setTextSelection(1); showLinkDialog();"
            "document.getElementById('link-url').value = 'mailto:hello@example.com';"
            "document.getElementById('link-text').value = '<Contact>';"
            "document.getElementById('link-form').requestSubmit();"
            "document.querySelector('.ProseMirror a').textContent === '<Contact>' && document.querySelector('.ProseMirror a').getAttribute('href') === 'mailto:hello@example.com';").toBool());
        QVERIFY(editorScript(
            "editor.commands.setTextSelection(3); showLinkDialog();"
            "document.getElementById('link-dialog').dispatchEvent(new Event('cancel', { cancelable: true }));"
            "!document.getElementById('link-dialog').open;").toBool());
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
    }

    void editableLinkRealMouseHover()
    {
        showRenderedEditor();
        editorScript("editor.commands.setContent('<p><a href=\"https://example.com/mouse\">Hover here</a></p>');");
        const QVariantList location = editorScript(
            "(() => { const r = document.querySelector('.ProseMirror a').getBoundingClientRect(); return [r.x + r.width / 2, r.y + r.height / 2]; })()").toList();
        QCOMPARE(location.size(), 2);
        auto *view = m_window->findChild<QWebEngineView *>();
        QVERIFY(view->focusProxy());
        QTest::mouseMove(view->focusProxy(), QPoint(location[0].toInt(), location[1].toInt()));
        QTRY_COMPARE(m_window->statusBar()->currentMessage(), QString("https://example.com/mouse"));
        QTest::mouseMove(view->focusProxy(), QPoint(5, 5));
        QTRY_VERIFY(m_window->statusBar()->currentMessage().isEmpty());
    }

    void editorLinkHoverShowsDestinationAndClears()
    {
        QFETCH(QString, url);
        auto *view = m_window->findChild<QWebEngineView *>();
        QVERIFY(view);
        auto *bar = m_window->statusBar();

        // Exercise the bridge boundary in addition to the rendered DOM tests.
        auto *bridge = m_window->findChild<EditorBridge *>();
        QVERIFY(bridge);
        bridge->setHoveredLink(url);
        QCOMPARE(bar->currentMessage(), url);
        const QString nextUrl = "https://example.org/next";
        bridge->setHoveredLink(nextUrl);
        QCOMPARE(bar->currentMessage(), nextUrl);
        bridge->setHoveredLink(QString());
        QVERIFY(bar->currentMessage().isEmpty());
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
    }

    void editorLinkLeavePreservesOtherStatusMessages()
    {
        auto *view = m_window->findChild<QWebEngineView *>();
        QVERIFY(view);
        auto *bar = m_window->statusBar();
        const QString status = "Downloading update...";
        auto *bridge = m_window->findChild<EditorBridge *>();
        QVERIFY(bridge);
        bar->showMessage(status);
        bridge->setHoveredLink(QString());
        QCOMPARE(bar->currentMessage(), status);

        bridge->setHoveredLink("https://example.com");
        bar->showMessage(status);
        bridge->setHoveredLink(QString());
        QCOMPARE(bar->currentMessage(), status);
    }

    void editorLinkPreviewClearsWhenChangingNotes()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        QVERIFY(editor);
        auto *view = editor->findChild<QWebEngineView *>();
        QVERIFY(view);
        auto *bar = m_window->statusBar();
        auto *bridge = editor->findChild<EditorBridge *>();
        QVERIFY(bridge);

        bridge->setHoveredLink("https://example.com/task");
        editor->loadTask(m_taskId);
        QVERIFY(bar->currentMessage().isEmpty());
        bridge->setHoveredLink("https://example.com/subtask");
        editor->loadSubTask(m_subtaskId);
        QVERIFY(bar->currentMessage().isEmpty());
        bridge->setHoveredLink("https://example.com/clear");
        editor->clear();
        QVERIFY(bar->currentMessage().isEmpty());
        bridge->setHoveredLink("https://example.com/reload");
        emit view->page()->loadStarted();
        QVERIFY(bar->currentMessage().isEmpty());
    }

    void updateButtonIsHiddenUntilAnUpdateIsAvailable()
    {
        auto *button = updateButton();
        QVERIFY(button);
        QVERIFY(button->isHidden());
        triggerManualCheck();
        m_network->reply->finish(releaseData("v1.0.7"));
        QVERIFY(button->isHidden());
        QCOMPARE(m_network->requests.size(), 1);
    }

    void updateButtonSurvivesDeferral_data()
    {
        QTest::addColumn<bool>("manual");
        QTest::addColumn<bool>("snoozed");
        QTest::newRow("manual-not-now") << true << false;
        QTest::newRow("automatic-not-now") << false << false;
        QTest::newRow("automatic-snoozed") << false << true;
    }

    void updateButtonSurvivesDeferral()
    {
        QFETCH(bool, manual);
        QFETCH(bool, snoozed);
        auto *button = updateButton();
        QVERIFY(button);
        if (snoozed) {
            DatabaseManager::instance().setSetting("update_reminder_after",
                QDateTime::currentDateTimeUtc().addMonths(1).toString(Qt::ISODateWithMs));
        }
        answerUpdate(false);
        checkRelease(manual);
        QCOMPARE(m_updateAnswers, snoozed ? 0 : 1);
        QVERIFY(!button->isHidden());
        QVERIFY(button->isEnabled());
        QCOMPARE(button->text(), QString("Update"));
        QVERIFY(button->toolTip().contains("v1.0.8"));
        QVERIFY(!button->accessibleName().isEmpty());
        QCOMPARE(m_network->requests.size(), 1);
        QVERIFY(!downloadDialog());
        QVERIFY(m_window->isHidden());
    }

    void updateButtonAppearsAtTopRight()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(false);
        checkRelease(true);
        m_window->show();
        QCoreApplication::processEvents();
        auto *search = m_window->findChild<SearchBar *>();
        QVERIFY(search);
        QVERIFY(button->isVisible());
        const QPoint position = button->mapTo(m_window.get(), QPoint());
#ifdef Q_OS_WIN
        auto *minimize = m_window->findChild<QToolButton *>("windowMinimizeButton");
        QVERIFY(minimize);
        const QPoint minimizePosition = minimize->mapTo(m_window.get(), QPoint());
        QVERIFY(position.x() + button->width() <= minimizePosition.x());
        QVERIFY(qAbs(position.y() + button->height() / 2
                     - minimizePosition.y() - minimize->height() / 2) <= 1);
        QVERIFY(position.y() + button->height() <= search->mapTo(m_window.get(), QPoint()).y());
#else
        const QPoint searchPosition = search->mapTo(m_window.get(), QPoint());
        QVERIFY(position.x() >= searchPosition.x() + search->width());
        QVERIFY(m_window->width() - position.x() - button->width() <= 24);
        QVERIFY(position.y() >= searchPosition.y());
        QVERIFY(position.y() + button->height() <= searchPosition.y() + search->height());
#endif
        captureDialog(m_window.get(), "update-button-available.png");
    }

    void windowCaptionControlsMinimizeMaximizeAndRestore()
    {
#ifdef Q_OS_WIN
        auto *caption = m_window->findChild<QWidget *>("windowTitleBar");
        auto *minimize = m_window->findChild<QToolButton *>("windowMinimizeButton");
        auto *maximize = m_window->findChild<QToolButton *>("windowMaximizeButton");
        auto *close = m_window->findChild<QToolButton *>("windowCloseButton");
        QVERIFY(caption);
        QVERIFY(minimize);
        QVERIFY(maximize);
        QVERIFY(close);
        QVERIFY(m_window->windowFlags().testFlag(Qt::FramelessWindowHint));
        m_window->show();
        maximize->click();
        QTRY_VERIFY(m_window->isMaximized());
        QCOMPARE(maximize->toolTip(), QString("Restore"));
        maximize->click();
        QTRY_VERIFY(!m_window->isMaximized());
        QCOMPARE(maximize->toolTip(), QString("Maximize"));
        minimize->click();
        QTRY_VERIFY(m_window->isMinimized());
        m_window->showNormal();
        QTRY_VERIFY(!m_window->isMinimized());
        QVERIFY(caption->isVisible());
#else
        QSKIP("Other platforms retain their native window captions.");
#endif
    }

    void windowCaptionTracksTitleAndDoubleClick()
    {
#ifdef Q_OS_WIN
        auto *caption = m_window->findChild<QWidget *>("windowTitleBar");
        auto *title = m_window->findChild<QLabel *>("windowTitleLabel");
        QVERIFY(caption);
        QVERIFY(title);
        m_window->setWindowTitle("Nexus <preview> & title");
        QCOMPARE(title->text(), m_window->windowTitle());
        QCOMPARE(title->textFormat(), Qt::PlainText);
        m_window->show();
        QTest::mouseDClick(caption, Qt::LeftButton);
        QTRY_VERIFY(m_window->isMaximized());
        QTest::mouseDClick(caption, Qt::LeftButton);
        QTRY_VERIFY(!m_window->isMaximized());
#else
        QSKIP("Other platforms retain their native window captions.");
#endif
    }

    void nativeWindowCaptionHitTestsAndMaximize()
    {
#ifdef Q_OS_WIN
        auto *caption = m_window->findChild<QWidget *>("windowTitleBar");
        auto *maximize = m_window->findChild<QToolButton *>("windowMaximizeButton");
        auto *title = m_window->findChild<QLabel *>("windowTitleLabel");
        QVERIFY(caption);
        QVERIFY(maximize);
        QVERIFY(title);
        if (QGuiApplication::platformName() != "windows")
            QSKIP("Native hit testing requires the Windows desktop plugin.");
        answerUpdate(false);
        checkRelease(true);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        const auto handle = reinterpret_cast<HWND>(m_window->winId());
        const LONG_PTR style = GetWindowLongPtr(handle, GWL_STYLE);
        QVERIFY(style & WS_THICKFRAME);
        QVERIFY(style & WS_SYSMENU);
        QVERIFY(style & WS_MAXIMIZEBOX);
        POINT origin{};
        QVERIFY(ClientToScreen(handle, &origin));
        const qreal scale = m_window->devicePixelRatioF();
        auto screenPoint = [&](QWidget *widget) {
            const QPoint position = widget->mapTo(m_window.get(), widget->rect().center());
            return QPoint(origin.x + qRound(position.x() * scale),
                          origin.y + qRound(position.y() * scale));
        };
        auto hitTest = [&](const QPoint &point) {
            return SendMessage(handle, WM_NCHITTEST, 0, MAKELPARAM(point.x(), point.y()));
        };
        QCOMPARE(hitTest(screenPoint(updateButton())), LRESULT(HTCLIENT));
        QCOMPARE(hitTest(screenPoint(maximize)), LRESULT(HTMAXBUTTON));
        QCOMPARE(hitTest(screenPoint(title)), LRESULT(HTCAPTION));
        RECT bounds{};
        QVERIFY(GetWindowRect(handle, &bounds));
        const int centerX = (bounds.left + bounds.right) / 2;
        const int centerY = (bounds.top + bounds.bottom) / 2;
        QCOMPARE(hitTest(QPoint(bounds.left + 1, centerY)), LRESULT(HTLEFT));
        QCOMPARE(hitTest(QPoint(bounds.right - 2, centerY)), LRESULT(HTRIGHT));
        QCOMPARE(hitTest(QPoint(centerX, bounds.top + 1)), LRESULT(HTTOP));
        QCOMPARE(hitTest(QPoint(centerX, bounds.bottom - 2)), LRESULT(HTBOTTOM));
        QCOMPARE(hitTest(QPoint(bounds.left + 1, bounds.top + 1)), LRESULT(HTTOPLEFT));
        QCOMPARE(hitTest(QPoint(bounds.right - 2, bounds.top + 1)), LRESULT(HTTOPRIGHT));
        QCOMPARE(hitTest(QPoint(bounds.left + 1, bounds.bottom - 2)), LRESULT(HTBOTTOMLEFT));
        QCOMPARE(hitTest(QPoint(bounds.right - 2, bounds.bottom - 2)), LRESULT(HTBOTTOMRIGHT));
        const QPoint maximizePoint = screenPoint(maximize);
        SendMessage(handle, WM_NCLBUTTONDOWN, HTMAXBUTTON,
                    MAKELPARAM(maximizePoint.x(), maximizePoint.y()));
        SendMessage(handle, WM_NCLBUTTONUP, HTMAXBUTTON,
                    MAKELPARAM(maximizePoint.x(), maximizePoint.y()));
        QTRY_VERIFY(m_window->isMaximized());
        MONITORINFO monitor{sizeof(MONITORINFO)};
        QVERIFY(GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor));
        QVERIFY(GetWindowRect(handle, &bounds));
        QVERIFY(bounds.left >= monitor.rcWork.left);
        QVERIFY(bounds.top >= monitor.rcWork.top);
        QVERIFY(bounds.right <= monitor.rcWork.right);
        QVERIFY(bounds.bottom <= monitor.rcWork.bottom);
        SendMessage(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_VERIFY(!m_window->isMaximized());
        SendMessage(handle, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        QTRY_VERIFY(m_window->isMinimized());
        SendMessage(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_VERIFY(!m_window->isMinimized());
        QVERIFY(!m_window->isMaximized());
        SendMessage(handle, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        QTRY_VERIFY(m_window->isMaximized());
        SendMessage(handle, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        QTRY_VERIFY(m_window->isMinimized());
        SendMessage(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_VERIFY(!m_window->isMinimized());
        QVERIFY(m_window->isMaximized());
        SendMessage(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_VERIFY(!m_window->isMaximized());
#else
        QSKIP("Native caption hit testing is Windows-specific.");
#endif
    }

    void updateButtonStartsDownloadWithoutAnotherPrompt()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(false, 1);
        checkRelease(true);
        QVERIFY(!DatabaseManager::instance().getSetting("update_reminder_after").isEmpty());
        button->click();
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl()));
        QVERIFY(DatabaseManager::instance().getSetting("update_reminder_after").isEmpty());
        QVERIFY(downloadDialog());
        QVERIFY(downloadDialog()->isVisible());
    }

    void updateButtonTracksDownloadAndAllowsRetryAfterCancel()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(true);
        checkRelease(true);
        QVERIFY(!button->isEnabled());
        QVERIFY(button->text().contains("Downloading"));
        emit m_network->reply->downloadProgress(1024, 4096);
        QVERIFY(button->text().contains("25%"));
        button->click();
        triggerManualCheck();
        QCOMPARE(m_network->requests.size(), 2);
        emit m_network->reply->downloadProgress(2048, -1);
        QVERIFY(!button->text().contains("25%"));
        downloadDialog()->close();
        QVERIFY(button->isEnabled());
        QCOMPARE(button->text(), QString("Update"));
        QVERIFY(!button->isHidden());
        button->click();
        QCOMPARE(m_network->requests.size(), 3);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl()));
        QVERIFY(downloadDialog());
    }

    void updateButtonRetriesFailedDownload_data()
    {
        QTest::addColumn<bool>("networkFailure");
        QTest::newRow("network-error") << true;
        QTest::newRow("empty-installer") << false;
    }

    void updateButtonRetriesFailedDownload()
    {
        QFETCH(bool, networkFailure);
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(true);
        checkRelease(true);
        QTest::ignoreMessage(QtWarningMsg, networkFailure
            ? "Update: Download failed: simulated failure" : "Update: Downloaded installer is empty");
        answerDialogs({"OK"});
        m_network->reply->finish({}, networkFailure
            ? QNetworkReply::TimeoutError : QNetworkReply::NoError);
        QVERIFY(button->isEnabled());
        QVERIFY(!button->isHidden());
        QCOMPARE(button->text(), QString("Update"));
        button->click();
        QCOMPARE(m_network->requests.size(), 3);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl()));
        QVERIFY(downloadDialog());
    }

    void updateButtonReopensDownloadedUpdate()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(true);
        checkRelease(true);
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        QVERIFY(button->isEnabled());
        QVERIFY(!button->isHidden());
        QCOMPARE(button->text(), updateButtonText());
        answerDialogs({"Later"});
        button->click();
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(m_downloads.size(), 1);
        QVERIFY(!m_updater->isBusy());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
    }

    void updateButtonRedownloadsMissingInstaller()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(true);
        checkRelease(true);
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        QCOMPARE(m_downloads.size(), 1);
        QVERIFY(QFile::remove(m_downloads.first()));
        button->click();
        QCOMPARE(m_network->requests.size(), 3);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl()));
        QVERIFY(downloadDialog());
    }

    void updateButtonUsesNewestDetectedRelease()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(true);
        checkRelease(true);
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        DatabaseManager::instance().setSetting("update_reminder_after",
            QDateTime::currentDateTimeUtc().addMonths(1).toString(Qt::ISODateWithMs));
        checkRelease(false, "v1.0.9");
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(button->text(), QString("Update"));
        QVERIFY(button->toolTip().contains("v1.0.9"));
        button->click();
        QCOMPARE(m_network->requests.size(), 4);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl("v1.0.9")));
        QVERIFY(downloadDialog());
    }

    void updateButtonDoesNotOverlapAnUpdateCheck()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(false);
        checkRelease(true);
        triggerManualCheck();
        QCOMPARE(m_network->requests.size(), 2);
        button->click();
        QCOMPARE(m_network->requests.size(), 2);
        QVERIFY(!downloadDialog());
        QVERIFY(m_window->statusBar()->currentMessage().contains("already in progress"));
        answerUpdate(false);
        checkRelease(true, "v1.0.9");
        button->click();
        QCOMPARE(m_network->requests.size(), 3);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl("v1.0.9")));
    }

    void updateButtonHidesWhenCheckReportsUpToDate()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(false);
        checkRelease(true);
        QVERIFY(!button->isHidden());
        triggerManualCheck();
        m_network->reply->finish(releaseData("v1.0.7"));
        QVERIFY(button->isHidden());
        QVERIFY(!downloadDialog());
        QCOMPARE(m_network->requests.size(), 2);
    }

    void updateButtonReportsReminderSaveFailure()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(false);
        checkRelease(true);
        const QString saved = DatabaseManager::instance().getSetting("update_reminder_after");
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to save setting"));
        answerDialogs({"OK"});
        button->click();
        QCOMPARE(m_network->requests.size(), 1);
        QVERIFY(!downloadDialog());
        QVERIFY(button->isEnabled());
        QCOMPARE(DatabaseManager::instance().getSetting("update_reminder_after"), saved);
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        button->click();
        QCOMPARE(m_network->requests.size(), 2);
        QVERIFY(downloadDialog());
    }

    void updateButtonPreservesSaveBeforeInstallProtection()
    {
        auto *button = updateButton();
        QVERIFY(button);
        answerUpdate(true);
        checkRelease(true);
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadSubTask(m_subtaskId);
        editor->findChild<EditorBridge *>()->setContent("<p>Unsaved button update edit</p>");
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        QTest::ignoreMessage(QtWarningMsg, qPrintable(
            QString("Failed to save subtask content: %1").arg(m_subtaskId)));
        answerDialogs({updateButtonText(), "OK"});
        button->click();
        QCOMPARE(quitting.count(), 0);
        QCOMPARE(m_network->requests.size(), 2);
        QVERIFY(button->isEnabled());
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_subtaskId).content,
                 QString("<p>Unsaved button update edit</p>"));
    }

    void deferringNeverDownloadsAndPersistsDeadline_data()
    {
        QTest::addColumn<int>("months");
        QTest::addColumn<bool>("close");
        QTest::newRow("six-hours") << 0 << false;
        QTest::newRow("one-month") << 1 << false;
        QTest::newRow("two-months") << 2 << false;
        QTest::newRow("three-months") << 3 << false;
        QTest::newRow("window-close") << 0 << true;
    }

    void deferringNeverDownloadsAndPersistsDeadline()
    {
        QFETCH(int, months);
        QFETCH(bool, close);
        const auto before = QDateTime::currentDateTimeUtc();
        answerUpdate(false, months, close);
        checkRelease(true);
        const auto after = QDateTime::currentDateTimeUtc();
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 1);
        QVERIFY(!downloadDialog());
        const auto deadline = QDateTime::fromString(
            DatabaseManager::instance().getSetting("update_reminder_after"), Qt::ISODateWithMs);
        QVERIFY(deadline.isValid());
        QVERIFY(deadline >= (months ? before.addMonths(months) : before.addSecs(6 * 3600)));
        QVERIFY(deadline <= (months ? after.addMonths(months) : after.addSecs(6 * 3600)));
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
    }

    void snoozeSurvivesReopenAndSuppressesAllAutomaticVersions()
    {
        answerUpdate(false, 2);
        checkRelease(true);
        const QString saved = DatabaseManager::instance().getSetting("update_reminder_after");
        m_window.reset();
        m_updater.reset();
        m_network.reset();
        {
            auto connection = QSqlDatabase::database();
            connection.close();
            QVERIFY(connection.open());
        }
        createWindow();
        m_updateAnswers = 0;
        for (const auto &version : {"v1.0.8", "v1.0.9"}) {
            answerUpdate(false);
            checkRelease(false, version);
            QCOMPARE(m_updateAnswers, 0);
            QVERIFY(!downloadDialog());
        }
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(DatabaseManager::instance().getSetting("update_reminder_after"), saved);
        QVERIFY(m_window->isHidden());
    }

    void manualCheckBypassesSnoozeWithoutShorteningItOnNotNow()
    {
        auto &db = DatabaseManager::instance();
        const QString saved = QDateTime::currentDateTimeUtc().addMonths(3).toString(Qt::ISODateWithMs);
        db.setSetting("update_reminder_after", saved);
        answerUpdate(false);
        triggerManualCheck();
        checkRelease(true);
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 1);
        QCOMPARE(db.getSetting("update_reminder_after"), saved);
    }

    void expiredSnoozeRemindsAboutTheSameRelease()
    {
        auto &db = DatabaseManager::instance();
        db.setSetting("update_reminder_after",
                      QDateTime::currentDateTimeUtc().addMonths(1).toString(Qt::ISODateWithMs));
        answerUpdate(false);
        checkRelease(false);
        QCOMPARE(m_updateAnswers, 0);
        db.setSetting("update_reminder_after",
                      QDateTime::currentDateTimeUtc().addSecs(-1).toString(Qt::ISODateWithMs));
        answerUpdate(false);
        checkRelease(false);
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 2);
    }

    void invalidSnoozeIsReportedAndDoesNotBlockReminders()
    {
        DatabaseManager::instance().setSetting("update_reminder_after", "invalid-date");
        QTest::ignoreMessage(QtWarningMsg, "Invalid update reminder date; reminders will resume.");
        answerUpdate(false);
        checkRelease(false);
        QCOMPARE(m_updateAnswers, 1);
    }

    void failedSnoozeSaveIsReported()
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to save setting"));
        answerUpdate(false, 1);
        answerDialogs({"OK"});
        checkRelease(true);
        QVERIFY(DatabaseManager::instance().getSetting("update_reminder_after").isEmpty());
        QCOMPARE(m_network->requests.size(), 1);
    }

    void yesShowsProgressAndClearsSnooze()
    {
        auto &db = DatabaseManager::instance();
        db.setSetting("update_reminder_after",
                      QDateTime::currentDateTimeUtc().addMonths(1).toString(Qt::ISODateWithMs));
        answerUpdate(true);
        checkRelease(true);
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl()));
        QVERIFY(db.getSetting("update_reminder_after").isEmpty());
        QPointer<QProgressDialog> dialog = downloadDialog();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QCOMPARE(dialog->maximum(), 0);
        emit m_network->reply->downloadProgress(1024, 4096);
        QCOMPARE(dialog->maximum(), 100);
        QCOMPARE(dialog->value(), 25);
        QVERIFY(dialog->labelText().contains("25%"));
        QVERIFY(dialog->labelText().contains("/"));
        captureDialog(dialog, "download-progress.png");
        triggerManualCheck();
        QCOMPARE(m_network->requests.size(), 2);
        emit m_network->reply->downloadProgress(2048, -1);
        QCOMPARE(dialog->maximum(), 0);
        QVERIFY(dialog->labelText().contains("unknown"));
        QVERIFY(!dialog->findChild<QProgressBar *>()->isTextVisible());
        captureDialog(dialog, "download-unknown-size.png");
        const auto largeSize = std::numeric_limits<qint64>::max();
        emit m_network->reply->downloadProgress(largeSize / 2, largeSize);
        QVERIFY(dialog->value() >= 49 && dialog->value() <= 50);
        emit m_network->reply->downloadProgress(4096, 4096);
        QCOMPARE(dialog->value(), 100);
        QVERIFY(dialog->isVisible());
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        QVERIFY(!dialog || !dialog->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        QVERIFY(!m_updater->isBusy());
        QCOMPARE(m_downloads.size(), 1);
        answerDialogs({"Later"});
        triggerManualCheck();
        QCOMPARE(m_network->requests.size(), 2);
    }

    void cachedDownloadClosesProgressWithoutAnotherRequest()
    {
        answerUpdate(true);
        checkRelease(true);
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        answerUpdate(true);
        answerDialogs({"Later"});
        checkRelease(true);
        QCOMPARE(m_network->requests.size(), 3);
        QCOMPARE(m_downloads.size(), 2);
        QCOMPARE(m_downloads.first(), m_downloads.last());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
    }

    void closingProgressAbortsDownloadAndAllowsRetry()
    {
        answerUpdate(true);
        checkRelease(true);
        QSignalSpy cancelled(m_updater.get(), &UpdateService::downloadCanceled);
        QSignalSpy errors(m_updater.get(), &UpdateService::error);
        auto *dialog = downloadDialog();
        QVERIFY(dialog);
        dialog->close();
        QCOMPARE(cancelled.count(), 1);
        QCOMPARE(errors.count(), 0);
        QVERIFY(!m_updater->isBusy());
        QVERIFY(m_downloads.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        answerUpdate(true);
        checkRelease(true);
        QCOMPARE(m_network->requests.size(), 4);
        QVERIFY(downloadDialog()->isVisible());
        auto *cancel = downloadDialog()->findChild<QPushButton *>();
        QVERIFY(cancel);
        cancel->click();
        QCOMPARE(cancelled.count(), 2);
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
    }

    void downloadFailureClosesProgressAndShowsError_data()
    {
        QTest::addColumn<bool>("networkFailure");
        QTest::newRow("network-error") << true;
        QTest::newRow("empty-installer") << false;
    }

    void downloadFailureClosesProgressAndShowsError()
    {
        QFETCH(bool, networkFailure);
        answerUpdate(true);
        checkRelease(true);
        QVERIFY(downloadDialog());
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        QTest::ignoreMessage(QtWarningMsg, networkFailure
            ? "Update: Download failed: simulated failure" : "Update: Downloaded installer is empty");
        answerDialogs({"OK"});
        m_network->reply->finish({}, networkFailure
            ? QNetworkReply::TimeoutError : QNetworkReply::NoError);
        QVERIFY(!m_updater->isBusy());
        QVERIFY(m_downloads.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        QCOMPARE(m_downloadPageHandler.requests,
                 QList<QUrl>{QUrl("https://www.jelllove.com/products/nexus.html")});
        QVERIFY(m_lastMessageText.contains(m_downloadPageHandler.requests.first().toString()));
        QVERIFY(m_lastMessageFlags.testFlag(Qt::TextSelectableByMouse));
        QCOMPARE(quitting.count(), 0);
    }

    void backgroundCheckFailureDoesNotOpenDownloadPage()
    {
        m_updater->setAutomaticChecksEnabled(true);
        QTRY_COMPARE(m_network->requests.size(), 1);
        QTest::ignoreMessage(QtWarningMsg, "Update: Update check failed: simulated failure");
        m_network->reply->finish({}, QNetworkReply::TimeoutError);
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
        QVERIFY(!m_updater->isBusy());
        QVERIFY(!downloadDialog());
    }

    void browserFailureKeepsManualDownloadUrlVisible()
    {
        answerUpdate(true);
        checkRelease(true);
        QDesktopServices::setUrlHandler("https", &m_downloadPageHandler, "missingSlot");
        QTest::ignoreMessage(QtWarningMsg, "Update: Download failed: simulated failure");
        QTest::ignoreMessage(QtWarningMsg,
                            QRegularExpression("^QMetaObject::invokeMethod: No such method"));
        QTest::ignoreMessage(QtWarningMsg,
                            QRegularExpression("^Update: Failed to open manual download page:"));
        answerDialogs({"OK"});
        m_network->reply->finish({}, QNetworkReply::TimeoutError);
        QVERIFY(m_lastMessageText.contains("Could not open your browser"));
        QVERIFY(m_lastMessageText.contains("https://www.jelllove.com/products/nexus.html"));
        QVERIFY(m_lastMessageText.contains("Download failed: simulated failure"));
        QVERIFY(m_lastMessageFlags.testFlag(Qt::TextSelectableByMouse));
        QVERIFY(!m_updater->isBusy());
    }

    void invalidDownloadUrlClosesProgressAndShowsError()
    {
        answerUpdate(true);
        answerDialogs({"OK"});
        QTest::ignoreMessage(QtWarningMsg, "Update: Invalid Nexus installer download URL");
        emit m_updater->updateAvailable("v1.0.8", "http://example.com/setup.exe", {}, true);
        QVERIFY(m_network->requests.isEmpty());
        QVERIFY(m_downloads.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        QCOMPARE(m_downloadPageHandler.requests,
                 QList<QUrl>{QUrl("https://www.jelllove.com/products/nexus.html")});
        m_answerTimer->deleteLater();
        m_answerTimer.clear();
    }

    void savesTaskAndSubtaskWithoutWaitingForAutosave()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        QVERIFY(editor);
        auto *bridge = editor->findChild<EditorBridge *>();
        editor->loadTask(m_taskId);
        bridge->setContent("<p>Pending task edit</p>");
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).content, "<p>Pending task edit</p>");
        editor->loadSubTask(m_subtaskId);
        bridge->setContent("<p>Pending subtask edit</p>");
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_subtaskId).content, "<p>Pending subtask edit</p>");
    }

    void titleMessageCannotOverwriteNoteOnUpdate()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadTask(m_taskId);
        auto *bridge = editor->findChild<EditorBridge *>();
        bridge->setContent("<p>Keep this body</p>");
        bridge->setContent("__TITLE__:Renamed task");
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).content, "<p>Keep this body</p>");
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).title, "Renamed task");
    }

    void laterDoesNotCloseApplicationAndManualActionReopensPrompt()
    {
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        QTemporaryFile installer(m_data.filePath("deferred-XXXXXX.exe"));
        QVERIFY(installer.open());
        const QString path = installer.fileName();
        installer.close();
        answerDialogs({"Later"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, path)));
        QCOMPARE(quitting.count(), 0);
        bool triggered = false;
        for (auto *action : m_window->findChildren<QAction *>()) {
            if (action->text() == "Check for &Updates...") {
                answerDialogs({"Later"});
                action->trigger();
                triggered = true;
                break;
            }
        }
        QVERIFY(triggered);
        QCOMPARE(quitting.count(), 0);
        QVERIFY(!m_updater->isBusy());
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
    }

    void installIsBlockedWhenNoteCannotBeSaved()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadSubTask(m_subtaskId);
        editor->findChild<EditorBridge *>()->setContent("<p>Unsaved edit</p>");
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        QTest::ignoreMessage(QtWarningMsg, qPrintable(
            QString("Failed to save subtask content: %1").arg(m_subtaskId)));
        answerDialogs({updateButtonText(), "OK"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, m_data.filePath("missing-installer.exe"))));
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QCOMPARE(quitting.count(), 0);
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_subtaskId).content, "<p>Unsaved edit</p>");
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
    }

    void failedInstallerLaunchKeepsApplicationOpen()
    {
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        answerDialogs({updateButtonText(), "OK"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, m_data.filePath("missing-installer.exe"))));
        QCOMPARE(quitting.count(), 0);
        QCOMPARE(m_downloadPageHandler.requests,
                 QList<QUrl>{QUrl("https://www.jelllove.com/products/nexus.html")});
        QVERIFY(m_lastMessageText.contains("missing-installer.exe"));
    }

    void closingWithoutTraySavesAndSaveFailureBlocksClose()
    {
        auto *tray = m_window->findChild<QSystemTrayIcon *>();
        QVERIFY(tray);
        tray->hide();
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadTask(m_taskId);
        editor->findChild<EditorBridge *>()->setContent("<p>Save before closing</p>");
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QTest::ignoreMessage(QtWarningMsg, qPrintable(
            QString("Failed to save task content: %1").arg(m_taskId)));
        answerDialogs({"OK"});
        QCloseEvent blocked;
        QApplication::sendEvent(m_window.get(), &blocked);
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QVERIFY(!blocked.isAccepted());
        QCloseEvent allowed;
        QApplication::sendEvent(m_window.get(), &allowed);
        QVERIFY(allowed.isAccepted());
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).content,
                 "<p>Save before closing</p>");
    }

    void nonWindowsUpdateOpensPackageWithoutQuitting()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows uses an executable installer, not desktop URL handling.");
#else
        TestUrlHandler handler;
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadTask(m_taskId);
        editor->findChild<EditorBridge *>()->setContent("<p>Save before opening update</p>");
        QTemporaryFile package(m_data.filePath("update-XXXXXX"));
        QVERIFY(package.open());
        const QString path = package.fileName();
        package.close();
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        answerDialogs({"Open update"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, path)));
#ifdef Q_OS_MACOS
        QCOMPARE(handler.opened, QUrl::fromLocalFile(path));
#else
        QCOMPARE(handler.opened, QUrl::fromLocalFile(m_data.path()));
#endif
        QCOMPARE(quitting.count(), 0);
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).content,
                 "<p>Save before opening update</p>");
        QVERIFY(m_downloadPageHandler.requests.isEmpty());
#endif
    }

    void offscreenDesktopReportsHotkeyUnavailable()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("This test requires the offscreen desktop used by CTest.");
        auto &hotkey = GlobalHotkey::instance();
        QVERIFY(!hotkey.registerHotkey());
        QVERIFY(!hotkey.errorString().isEmpty());
        QVERIFY(!GlobalHotkey::availabilityMessage().isEmpty());
    }

    void settingsDescribeSixHoursAndRequireDownloadConsent()
    {
        SettingsDialog settings;
        bool foundCheck = false;
        bool foundDownload = false;
        for (auto *box : settings.findChildren<QCheckBox *>()) {
            if (box->text() == "Check for updates on startup and every 6 hours") {
                foundCheck = true;
                box->setChecked(true);
            }
            if (box->text() == "Automatically download updates (ask before installing)") {
                foundDownload = true;
                box->setChecked(false);
            }
        }
        QVERIFY(foundCheck);
        QVERIFY(!foundDownload);
        QVERIFY(QMetaObject::invokeMethod(&settings, "onSave"));
        QCOMPARE(DatabaseManager::instance().getSetting("check_updates"), "true");
        DatabaseManager::instance().setSetting("check_updates", "false");
    }

    void settingsShowSavedReminderDate()
    {
        const auto deadline = QDateTime::currentDateTimeUtc().addMonths(2);
        DatabaseManager::instance().setSetting("update_reminder_after", deadline.toString(Qt::ISODateWithMs));
        SettingsDialog settings;
        bool found = false;
        for (auto *label : settings.findChildren<QLabel *>()) {
            if (label->text().startsWith("Automatic reminders paused until ")) {
                found = true;
                QVERIFY(label->text().contains(QLocale().toString(deadline.toLocalTime(), QLocale::ShortFormat)));
            }
        }
        QVERIFY(found);
    }

    void settingsApplyWithoutRestartWhileWindowIsHidden()
    {
        auto *timer = m_updater->findChild<QTimer *>("updateCheckTimer");
        for (bool enabled : {true, false}) {
            QTimer::singleShot(0, this, [this, enabled]() {
                auto *dialog = m_window->findChild<SettingsDialog *>();
                QVERIFY(dialog);
                for (auto *box : dialog->findChildren<QCheckBox *>()) {
                    if (box->text() == "Check for updates on startup and every 6 hours")
                        box->setChecked(enabled);
                }
                QVERIFY(QMetaObject::invokeMethod(dialog, "onSave"));
            });
            QVERIFY(QMetaObject::invokeMethod(m_window.get(), "showSettings"));
            QCOMPARE(timer->isActive(), enabled);
            QVERIFY(m_window->isHidden());
            // Stop before processing the queued startup request: this test never accesses GitHub.
            if (enabled) m_updater->setAutomaticChecksEnabled(false);
        }
    }

    void cleanupTestCase()
    {
        m_window.reset();
        QSqlDatabase::database().close();
    }
};

QTEST_MAIN(UpdateUiTest)
#include "UpdateUiTest.moc"
