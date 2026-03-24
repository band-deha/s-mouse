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
#include <QDateTime>
#include <QNetworkInterface>
#include <QScrollBar>

namespace smouse {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("S-Mouse");
    setMinimumSize(700, 500);
    setup_ui();
    create_menus();

    // Thread-safe log signal
    QObject::connect(this, &MainWindow::log_message_received,
                     this, &MainWindow::append_log, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::client_list_changed,
                     this, &MainWindow::refresh_screen_editor, Qt::QueuedConnection);
}

MainWindow::~MainWindow() {
    if (server_) server_->stop();
    if (client_) client_->disconnect();
}

QString MainWindow::get_lan_ip() {
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            const auto entries = iface.addressEntries();
            for (const auto& entry : entries) {
                auto addr = entry.ip();
                if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
                    !addr.isLoopback()) {
                    return addr.toString();
                }
            }
        }
    }
    return "127.0.0.1";
}

void MainWindow::setup_ui() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* main_layout = new QVBoxLayout(central);

    // Connection group
    auto* conn_group = new QGroupBox("Connection", this);
    auto* conn_layout = new QVBoxLayout(conn_group);

    auto* conn_row1 = new QHBoxLayout();

    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("Server");
    mode_combo_->addItem("Client");
    conn_row1->addWidget(mode_combo_);

    host_edit_ = new QLineEdit("127.0.0.1", this);
    host_edit_->setPlaceholderText("Server IP address");
    conn_row1->addWidget(host_edit_);

    port_spin_ = new QSpinBox(this);
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(DEFAULT_TCP_PORT);
    conn_row1->addWidget(port_spin_);

    connect_btn_ = new QPushButton("Start", this);
    connect(connect_btn_, &QPushButton::clicked, this, [this]() {
        if (mode_combo_->currentIndex() == 0) {
            on_start_server();
        } else {
            on_connect_client();
        }
    });
    conn_row1->addWidget(connect_btn_);

    disconnect_btn_ = new QPushButton("Stop", this);
    disconnect_btn_->setEnabled(false);
    connect(disconnect_btn_, &QPushButton::clicked, this, &MainWindow::on_disconnect);
    conn_row1->addWidget(disconnect_btn_);

    conn_layout->addLayout(conn_row1);

    // LAN IP display row
    auto* ip_row = new QHBoxLayout();
    ip_label_ = new QLabel(this);
    ip_label_->setStyleSheet("color: #888; font-size: 11px;");
    ip_label_->setText("LAN IP: " + get_lan_ip());
    ip_row->addWidget(ip_label_);
    ip_row->addStretch();
    conn_layout->addLayout(ip_row);

    main_layout->addWidget(conn_group);

    // Mode toggle
    connect(mode_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        host_edit_->setEnabled(index == 1);
        connect_btn_->setText(index == 0 ? "Start Server" : "Connect");
    });
    host_edit_->setEnabled(false);
    connect_btn_->setText("Start Server");

    // Tabbed area: Screen Layout + Log
    tab_widget_ = new QTabWidget(this);

    // Screen layout editor tab
    screen_editor_ = new ScreenEditor(this);
    tab_widget_->addTab(screen_editor_, "Screen Arrangement");

    // When user drags a client screen, update the server layout edge
    QObject::connect(screen_editor_, &ScreenEditor::client_edge_changed,
                     this, [this](const QString& client_id, int edge) {
        if (server_) {
            server_->layout().set_client_edge(
                client_id.toStdString(),
                static_cast<smouse::Edge>(edge));
            QString edgeName = ScreenEditor::edge_name(edge);
            emit log_message_received(
                QString("[GUI] Client %1 moved to %2 edge of server")
                    .arg(client_id, edgeName));
        }
    });

    // Log tab
    log_view_ = new QTextEdit(this);
    log_view_->setReadOnly(true);
    log_view_->setFont(QFont("Consolas", 9));
    log_view_->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"
    );
    tab_widget_->addTab(log_view_, "Event Log");

    main_layout->addWidget(tab_widget_, 1);

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

void MainWindow::append_log(const QString& msg) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString colored;

    // Color-code log messages
    if (msg.contains("[Server]")) {
        colored = QString("<span style='color:#569cd6'>%1</span> <span style='color:#4ec9b0'>%2</span>")
                      .arg(timestamp, msg.toHtmlEscaped());
    } else if (msg.contains("[Client]")) {
        colored = QString("<span style='color:#569cd6'>%1</span> <span style='color:#dcdcaa'>%2</span>")
                      .arg(timestamp, msg.toHtmlEscaped());
    } else {
        colored = QString("<span style='color:#569cd6'>%1</span> <span style='color:#d4d4d4'>%2</span>")
                      .arg(timestamp, msg.toHtmlEscaped());
    }

    log_view_->append(colored);

    // Auto-scroll to bottom
    auto* sb = log_view_->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::refresh_screen_editor() {
    if (!server_) return;

    screen_editor_->clear();

    auto clients = server_->get_clients();
    for (const auto& [id, name] : clients) {
        auto info = server_->layout().get_client(id);
        if (info) {
            screen_editor_->add_client_screen(
                QString::fromStdString(name.empty() ? id : name),
                QString::fromStdString(id),
                static_cast<int>(info->rect.width),
                static_cast<int>(info->rect.height));
        }
    }

    // Switch to screen arrangement tab when clients connect
    if (!clients.empty()) {
        tab_widget_->setCurrentIndex(0);
    }
}

void MainWindow::on_start_server() {
    server_ = std::make_unique<Server>();

    // Set up log callback
    server_->set_log_callback([this](const std::string& msg) {
        emit log_message_received(QString::fromStdString(msg));
    });

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

        // Refresh screen editor when client list changes
        emit client_list_changed();
    });

    uint16_t port = static_cast<uint16_t>(port_spin_->value());
    if (server_->start(port)) {
        is_server_ = true;
        connect_btn_->setEnabled(false);
        disconnect_btn_->setEnabled(true);
        mode_combo_->setEnabled(false);

        QString lan_ip = get_lan_ip();
        ip_label_->setText(QString("Listening on %1:%2").arg(lan_ip).arg(port));
        ip_label_->setStyleSheet("color: #4ec9b0; font-size: 11px; font-weight: bold;");
        update_status(QString("Server listening on %1:%2").arg(lan_ip).arg(port));

        // Switch to log tab to show startup events
        tab_widget_->setCurrentIndex(1);
    } else {
        QMessageBox::critical(this, "Error",
            "Failed to start server.\n"
            "Make sure the port is not in use and\n"
            "Accessibility permission is enabled.");
        server_.reset();
    }
}

void MainWindow::on_connect_client() {
    client_ = std::make_unique<Client>();
    client_->set_name("s-mouse-gui");
    client_->set_auto_reconnect(true);

    // Set up log callback
    client_->set_log_callback([this](const std::string& msg) {
        emit log_message_received(QString::fromStdString(msg));
    });

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

        // Switch to log tab
        tab_widget_->setCurrentIndex(1);
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
    ip_label_->setText("LAN IP: " + get_lan_ip());
    ip_label_->setStyleSheet("color: #888; font-size: 11px;");
    update_status("Disconnected");
    screen_editor_->clear();
}

void MainWindow::on_settings() {
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::update_status(const QString& status) {
    status_label_->setText(status);
}

} // namespace smouse
