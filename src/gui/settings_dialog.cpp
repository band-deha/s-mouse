#include "settings_dialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QSettings>

namespace smouse {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumWidth(350);

    auto* layout = new QVBoxLayout(this);

    // Network settings
    auto* net_group = new QGroupBox("Network", this);
    auto* net_form = new QFormLayout(net_group);

    port_spin_ = new QSpinBox(this);
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(24800);
    net_form->addRow("Port:", port_spin_);

    keepalive_spin_ = new QSpinBox(this);
    keepalive_spin_->setRange(1, 30);
    keepalive_spin_->setValue(3);
    keepalive_spin_->setSuffix(" seconds");
    net_form->addRow("Keepalive timeout:", keepalive_spin_);

    layout->addWidget(net_group);

    // Behavior settings
    auto* behavior_group = new QGroupBox("Behavior", this);
    auto* behavior_form = new QFormLayout(behavior_group);

    dead_zone_spin_ = new QSpinBox(this);
    dead_zone_spin_->setRange(0, 50);
    dead_zone_spin_->setValue(5);
    dead_zone_spin_->setSuffix(" pixels");
    behavior_form->addRow("Corner dead zone:", dead_zone_spin_);

    auto_reconnect_check_ = new QCheckBox("Auto-reconnect on disconnect", this);
    auto_reconnect_check_->setChecked(true);
    behavior_form->addRow(auto_reconnect_check_);

    clipboard_check_ = new QCheckBox("Share clipboard on screen switch", this);
    clipboard_check_->setChecked(true);
    behavior_form->addRow(clipboard_check_);

    layout->addWidget(behavior_group);

    // Buttons
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::on_save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Load current settings
    QSettings settings("s-mouse", "s-mouse");
    port_spin_->setValue(settings.value("port", 24800).toInt());
    dead_zone_spin_->setValue(settings.value("dead_zone", 5).toInt());
    auto_reconnect_check_->setChecked(settings.value("auto_reconnect", true).toBool());
    clipboard_check_->setChecked(settings.value("clipboard_sharing", true).toBool());
    keepalive_spin_->setValue(settings.value("keepalive_timeout", 3).toInt());
}

void SettingsDialog::on_save() {
    QSettings settings("s-mouse", "s-mouse");
    settings.setValue("port", port_spin_->value());
    settings.setValue("dead_zone", dead_zone_spin_->value());
    settings.setValue("auto_reconnect", auto_reconnect_check_->isChecked());
    settings.setValue("clipboard_sharing", clipboard_check_->isChecked());
    settings.setValue("keepalive_timeout", keepalive_spin_->value());
    accept();
}

} // namespace smouse
