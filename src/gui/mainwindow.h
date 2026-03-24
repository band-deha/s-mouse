#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

namespace smouse {

class ScreenEditor;
class Server;
class Client;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_start_server();
    void on_connect_client();
    void on_disconnect();
    void on_settings();
    void update_status(const QString& status);

private:
    void setup_ui();
    void create_menus();

    // UI elements
    QComboBox* mode_combo_ = nullptr;
    QLineEdit* host_edit_ = nullptr;
    QSpinBox* port_spin_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    QPushButton* disconnect_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    ScreenEditor* screen_editor_ = nullptr;

    // Core
    std::unique_ptr<Server> server_;
    std::unique_ptr<Client> client_;
    bool is_server_ = false;
};

} // namespace smouse
