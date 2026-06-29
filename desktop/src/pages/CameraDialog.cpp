#include "pages/CameraDialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>

namespace dsd {

CameraDialog::CameraDialog(const CameraInfo& initial, QWidget* parent)
    : QDialog(parent), id_(initial.id) {
    setWindowTitle(initial.id == 0 ? "Add Camera" : "Edit Camera");

    name_edit_ = new QLineEdit(QString::fromStdString(initial.name), this);
    rtsp_edit_ = new QLineEdit(QString::fromStdString(initial.rtspUrl), this);
    enabled_check_ = new QCheckBox("Enabled", this);
    enabled_check_->setChecked(initial.enabled);

    auto* form = new QFormLayout(this);
    form->addRow("Name", name_edit_);
    form->addRow("RTSP URL", rtsp_edit_);
    form->addRow(enabled_check_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CameraInfo CameraDialog::camera() const {
    CameraInfo info;
    info.id = id_;
    info.name = name_edit_->text().trimmed().toStdString();
    info.rtspUrl = rtsp_edit_->text().trimmed().toStdString();
    info.enabled = enabled_check_->isChecked();
    return info;
}

void CameraDialog::accept() {
    if (name_edit_->text().trimmed().isEmpty() ||
        rtsp_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Invalid input",
                             "Name and RTSP URL must not be empty.");
        return;  // keep the dialog open
    }
    QDialog::accept();  // valid -> close with Accepted
}

}  // namespace dsd
