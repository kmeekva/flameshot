#include "flameshotdaemon.h"

#include "dbusutils.h"
#include "pinwidget.h"
#include "screenshotsaver.h"
#include "systemnotification.h"
#include <QApplication>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QPixmap>
#include <QRect>

// TODO handle if daemon can't be contacted via dbus

void FlameshotDaemon::start()
{
    if (!m_instance) {
        m_instance = new FlameshotDaemon();
        connect(QApplication::clipboard(),
                &QClipboard::dataChanged,
                instance(),
                []() {
                    instance()->m_hostingClipboard = false;
                    instance()->quitIfIdle();
                });
    }
}

void FlameshotDaemon::createPin(QPixmap capture, QRect geometry)
{
    if (instance()) {
        instance()->attachPin(capture, geometry);
        return;
    }

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << capture;
    stream << geometry;
    QDBusMessage m = createMethodCall(QStringLiteral("attachPin"));
    m << data;
    call(m);
}

void FlameshotDaemon::copyToClipboard(QPixmap capture)
{
    if (instance()) {
        instance()->attachScreenshotToClipboard(capture);
        return;
    }

    QDBusMessage m =
      createMethodCall(QStringLiteral("attachScreenshotToClipboard"));

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << capture;

    m << data;
    call(m);
}

void FlameshotDaemon::copyToClipboard(QString text)
{
    auto m = createMethodCall(QStringLiteral("attachTextToClipboard"));

    m << text;

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    DBusUtils().checkDBusConnection(sessionBus);
    sessionBus.call(m);
}

FlameshotDaemon::FlameshotDaemon()
  : m_persist(false)
  , m_hostingClipboard(false)
{
    // TODO on windows/(Mac maybe?), always persist so hotkeys can be used
    // TODO consider which config options could influence this
}

FlameshotDaemon* FlameshotDaemon::instance()
{
    return m_instance;
}

/**
 * @brief Quit the daemon if it has nothing to do and the 'persist' flag is not
 * set.
 */
void FlameshotDaemon::quitIfIdle()
{
    if (m_persist) {
        return;
    }
    if (!m_hostingClipboard && m_widgets.isEmpty()) {
        qApp->exit(0);
    }
}

// SERVICE METHODS

void FlameshotDaemon::attachPin(QPixmap pixmap, QRect geometry)
{
    PinWidget* pinWidget = new PinWidget(pixmap, geometry);
    m_widgets.append(pinWidget);
    connect(pinWidget, &QObject::destroyed, this, [=]() {
        m_widgets.removeOne(pinWidget);
        quitIfIdle();
    });

    pinWidget->show();
    pinWidget->activateWindow();
}

void FlameshotDaemon::attachScreenshotToClipboard(QPixmap pixmap)
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->blockSignals(true);
    ScreenshotSaver().saveToClipboard(pixmap);
    clipboard->blockSignals(false);
}

// D-BUS ADAPTER METHODS

void FlameshotDaemon::attachPin(const QByteArray& data)
{
    QDataStream stream(data);
    QPixmap pixmap;
    QRect geometry;

    stream >> pixmap;
    stream >> geometry;

    attachPin(pixmap, geometry);
}

void FlameshotDaemon::attachScreenshotToClipboard(const QByteArray& screenshot)
{
    m_hostingClipboard = true;

    QDataStream stream(screenshot);
    QPixmap p;
    stream >> p;

    attachScreenshotToClipboard(p);
}

void FlameshotDaemon::attachTextToClipboard(QString text)
{
    m_hostingClipboard = true;
    QClipboard* clipboard = QApplication::clipboard();

    clipboard->blockSignals(true);
    QApplication::clipboard()->setText(text);
    clipboard->blockSignals(false);
}

QDBusMessage FlameshotDaemon::createMethodCall(QString method)
{
    QDBusMessage m =
      QDBusMessage::createMethodCall(QStringLiteral("org.flameshot.Flameshot"),
                                     QStringLiteral("/"),
                                     QLatin1String(""),
                                     method);
    return m;
}

void FlameshotDaemon::call(const QDBusMessage& m)
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    DBusUtils().checkDBusConnection(sessionBus);
    sessionBus.call(m);
}

// STATIC ATTRIBUTES
FlameshotDaemon* FlameshotDaemon::m_instance = nullptr;
