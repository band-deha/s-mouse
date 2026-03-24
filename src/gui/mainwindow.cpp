#include "mainwindow.h"
#include "screen_editor.h"
#include "settings_dialog.h"
#include "core/server.h"
#include "core/client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

namespace smouse {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("S-Mouse");
    setMinimumSize(600, 400);
    setup_ui();
    create_menus();
}

MainWindow::~MainWindow() {
    if (server_) server_->stop();
    if (client_) client_->disconnect();
}

void MainWindow::setup_ui() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* main_layout = new QVBoxLayout(central);

    // Connection group
    auto* conn_group = new QGroupBox("Connection", this);
    auto* conn_layout = new QHBoxLayout(conn_group);

    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("Server");
    mode_combo_->addItem("Client");
    conn_layout->addWidget(mode_combo_);

    host_edit_ = new QLineEdit("127.0.0.1", this);
    host_edit_->setPlaceholderText("Server IP address");
    conn_layout->addWidget(host_edit_);

    port_spin_ = new QSpinBox(this);
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(DEFAULT_TCP_PORT);
    conn_layout->addWidget(port_spin_);

    connect_btn_ = new QPushButton("Start", this);
    connect(connect_btn_, &QPushButton::clicked, this, [this]() {
        if (mode_combo_->currentIndex() == 0) {
            on_start_server();
        } else {
            on_connect_client();
        }
    });
    conn_layout->addWidget(connect_btn_);

    disconnect_btn_ = new QPushButton("Stop", this);
    disconnect_btn_->setEnabled(false);
    connect(disconnect_btn_, &QPushButton::clicked, this, &MainWindow::on_disconnect);
    conn_layout->addWidget(disconnect_btn_);

    main_layout->addWidget(conn_group);

    // Mode toggle
    connect(mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        host_edit_->setEnabled(index == 1);  // Only show host for client mode
        connect_btn_->setText(index == 0 ? "Start Server" : "Connect");
    });
    host_edit_->setEnabled(false);
    connect_btn_->setText("Start Server");

    // Screen layout editor
    screen_editor_ = new ScreenEditor(this);
    main_layout->addWidget(screen_editor_, 1);

    // Status bar
    status_label_ = new QLabel("Disconnected", this);
    statusBar()->addWidget(status_label_);
}

void MainWindow::create_menus() {
    auto* file_menu = menuBar()->addMenu("&File");

    auto* settings_action = file_menu->addAction("&Settings...");
    connect(settings_action, &QAction::triggered, this, &MainWindow::on_settings);

    file_menu->addSeparator();

    auto* quit_action = file_menu->addAction("&Quit");
    quit_action->setShortcut(QKeySequence::Quit);
    connect(quit_action, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::on_start_server() {
    server_ = std::make_unique<Server>();
    server_->set_state_callback([this](ServerState state, const std::string& client_id) {
        QString status;
        switch (state) {
        case ServerState::LOCAL_ACTIVE:
            status = "Server: Local active";
            break;
        case ServerState::CLIENT_ACTIVE:
            status = QString("Server: Client active (%1)").arg(QString::fromStdString(client_id));
            break;
        case ServerState::DISCONNECTED:
            status = "Server: Disconnected";
            break;
        }
        QMetaObject::invokeMethod(this, [this, status]() {
            update_status(status);
        }, Qt::QueuedConnection);
    });

    uint16_t port = static_cast<uint16_t>(port_spin_->value());
    if (server_->start(port)) {
        is_server_ = true;
        connect_btn_->setEnabled(false);
        disconnect_btn_->setEnabled(true);
        mode_combo_->setEnabled(false);
        update_status(QString("Server listening on port %1").arg(port));
    } else {
        QMessageBox::critical(this, "Error",
            "Failed to start server.\n"
            "Make sure Accessibility permission is enabled in\n"
            "System Settings > Privacy & Security > Accessibility");
        server_.reset();
    }
}

void MainWindow::on_connect_client() {
    client_ = std::make_unique<Client>();
    client_->set_name("s-mouse-gui");
    client_->set_auto_reconnect(true);
    client_->set_state_callback([this](ClientState state) {
        QString status;
        switch (state) {
        case ClientState::DISCONNECTED: status = "Client: Disconnected"; break;
        case ClientState::CONNECTED: status = "Client: Connected (idle)"; break;
        case ClientState::ACTIVE: status = "Client: Active (receiving input)"; break;
        case ClientState::RECONNECTING: status = "Client: Reconnecting..."; break;
        }
        QMetaObject::invokeMethod(this, [this, status]() {
            update_status(status);
        }, Qt::QueuedConnection);
    });

    std::string host = host_edit_->text().toStdString();
    uint16_t port = static_cast<uint16_t>(port_spin_->value());

    if (client_->connect(host, port)) {
        is_server_ = false;
        connect_btn_->setEnabled(false);
        disconnect_btn_->setEnabled(true);
        mode_combo_->setEnabled(false);
        host_edit_->setEnabled(false);
        update_status(QString("Connected to %1:%2").arg(host_edit_->text()).arg(port));
    } else {
        QMessageBox::critical(this, "Error",
            QString("Failed to connect to %1:%2").arg(host_edit_->text()).arg(port));
        client_.reset();
    }
}

void MainWindow::on_disconnect() {
    if (server_) {
        server_->stop();
        server_.reset();
    }
    if (client_) {
        client_->disconnect();
        client_.reset();
    }

    connect_btn_->setEnabled(true);
    disconnect_btn_->setEnabled(false);
    mode_combo_->setEnabled(true);
    host_edit_->setEnabled(mode_combo_->currentIndex() == 1);
    update_status("Disconnected");
}

void MainWindow::on_settings() {
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::update_status(const QString& status) {
    status_label_->setText(status);
}

} // namespace smouse
