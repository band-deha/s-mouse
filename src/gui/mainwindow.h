#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QTabWidget>

namespace smouse {

class ScreenEditor;
class Server;
class Client;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void log_message_received(const QString& msg);
    void client_list_changed();

private slots:
    void on_start_server();
    void on_connect_client();
    void on_disconnect();
    void on_settings();
    void update_status(const QString& status);
    void append_log(const QString& msg);
    void refresh_screen_editor();

private:
    void setup_ui();
    void create_menus();
    static QString get_lan_ip();

    // UI elements
    QComboBox* mode_combo_ = nullptr;
    QLineEdit* host_edit_ = nullptr;
    QSpinBox* port_spin_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    QPushButton* disconnect_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* ip_label_ = nullptr;
    ScreenEditor* screen_editor_ = nullptr;
    QTextEdit* log_view_ = nullptr;
    QTabWidget* tab_widget_ = nullptr;

    // Core
    std::unique_ptr<Server> server_;
    std::unique_ptr<Client> client_;
    bool is_server_ = false;
};

} // namespace smouse
