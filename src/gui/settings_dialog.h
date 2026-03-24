#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>

namespace smouse {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void on_save();

private:
    QSpinBox* port_spin_ = nullptr;
    QSpinBox* dead_zone_spin_ = nullptr;
    QCheckBox* auto_reconnect_check_ = nullptr;
    QCheckBox* clipboard_check_ = nullptr;
    QSpinBox* keepalive_spin_ = nullptr;
};

} // namespace smouse
