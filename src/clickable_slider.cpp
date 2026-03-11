#include "clickable_slider.h"

#include <QMouseEvent>
#include <QStyle>

ClickableSlider::ClickableSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
}

void ClickableSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int handleLength = style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
        int value = minimum();

        if (orientation() == Qt::Horizontal) {
            const int availableWidth = qMax(1, width() - handleLength);
            const int x = qBound(0, event->position().toPoint().x() - handleLength / 2, availableWidth);
            value = QStyle::sliderValueFromPosition(minimum(), maximum(), x, availableWidth);
        } else {
            const int availableHeight = qMax(1, height() - handleLength);
            const int y = qBound(0, event->position().toPoint().y() - handleLength / 2, availableHeight);
            value = QStyle::sliderValueFromPosition(minimum(), maximum(), availableHeight - y, availableHeight);
        }

        setValue(value);
    }

    QSlider::mousePressEvent(event);
}
