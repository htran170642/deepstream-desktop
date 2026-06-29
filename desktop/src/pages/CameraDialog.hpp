#pragma once

#include <cstdint>

#include <QDialog>

#include "client/CameraClient.hpp"  // for CameraInfo

class QCheckBox;
class QLineEdit;

namespace dsd {

// Modal form for adding or editing a camera. Pure UI: it only collects fields
// into a CameraInfo. The caller (CameraPage) performs the gRPC call.
class CameraDialog : public QDialog {
    Q_OBJECT

public:
    // For "Add", pass a default-constructed CameraInfo. 
    //For "Edit", pass the
    // existing one — its id is preserved and returned unchanged.
    explicit CameraDialog(const CameraInfo& initial, QWidget* parent = nullptr);

    // The camera as edited in the form. Valid after exec() returns Accepted.
    CameraInfo camera() const;

protected:
    // Rejects empty name / RTSP URL before closing, with a warning popup.
    void accept() override;

private:
    std::int64_t id_ = 0;  // preserved from the initial camera (0 when adding)
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* rtsp_edit_ = nullptr;
    QCheckBox* enabled_check_ = nullptr;
};

}  // namespace dsd
