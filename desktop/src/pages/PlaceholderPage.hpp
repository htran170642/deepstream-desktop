#pragma once

#include <QString>
#include <QWidget>

namespace dsd {

// Base class for skeleton pages: shows a single centered title.
// Feature phases replace this placeholder content with real UI.
class PlaceholderPage : public QWidget {
    Q_OBJECT

public:
    explicit PlaceholderPage(const QString& title, QWidget* parent = nullptr);
};

}  // namespace dsd
