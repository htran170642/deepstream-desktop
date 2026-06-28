#include "pages/PlaceholderPage.hpp"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace dsd {

PlaceholderPage::PlaceholderPage(const QString& title, QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* label = new QLabel(title, this);
    label->setAlignment(Qt::AlignCenter);

    // Make the placeholder title visually prominent.
    QFont font = label->font();
    font.setPointSize(24);
    label->setFont(font);

    layout->addWidget(label);
}

}  // namespace dsd
