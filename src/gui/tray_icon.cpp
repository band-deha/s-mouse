#include "tray_icon.h"
#include <QApplication>
#include <QPixmap>
#include <QPainter>

namespace smouse {

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent) {
    tray_icon_ = new QSystemTrayIcon(this);
    tray_menu_ = new QMenu();

    auto* show_action = tray_menu_->addAction("Show S-Mouse");
    connect(show_action, &QAction::triggered, this, &TrayIcon::show_window_requested);

    tray_menu_->addSeparator();

    auto* quit_action = tray_menu_->addAction("Quit");
    connect(quit_action, &QAction::triggered, this, &TrayIcon::quit_requested);

    tray_icon_->setContextMenu(tray_menu_);
    create_icon(Status::DISCONNECTED);
}

void TrayIcon::show() {
    tray_icon_->show();
}

void TrayIcon::set_status(Status status) {
    if (status == current_status_) return;
    current_status_ = status;
    create_icon(status);

    QString tooltip;
    switch (status) {
    case Status::DISCONNECTED: tooltip = "S-Mouse: Disconnected"; break;
    case Status::CONNECTED: tooltip = "S-Mouse: Connected"; break;
    case Status::RECONNECTING: tooltip = "S-Mouse: Reconnecting..."; break;
    }
    tray_icon_->setToolTip(tooltip);
}

void TrayIcon::create_icon(Status status) {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color;
    switch (status) {
    case Status::DISCONNECTED: color = QColor(200, 60, 60); break;
    case Status::CONNECTED: color = QColor(60, 180, 60); break;
    case Status::RECONNECTING: color = QColor(220, 180, 40); break;
    }

    painter.setBrush(color);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawEllipse(3, 3, 16, 16);

    // Draw "S" letter
    painter.setPen(QPen(Qt::white, 2));
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "S");

    tray_icon_->setIcon(QIcon(pixmap));
}

} // namespace smouse
