#pragma once

#include <QSystemTrayIcon>
#include <QMenu>

namespace smouse {

class TrayIcon : public QObject {
    Q_OBJECT

public:
    enum class Status {
        DISCONNECTED,   // Red
        CONNECTED,      // Green
        RECONNECTING,   // Yellow
    };

    explicit TrayIcon(QObject* parent = nullptr);

    void show();
    void set_status(Status status);

signals:
    void show_window_requested();
    void quit_requested();

private:
    void create_icon(Status status);

    QSystemTrayIcon* tray_icon_ = nullptr;
    QMenu* tray_menu_ = nullptr;
    Status current_status_ = Status::DISCONNECTED;
};

} // namespace smouse
