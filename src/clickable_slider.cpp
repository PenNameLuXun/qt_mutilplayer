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
        const int handleWidth = style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
        const int availableWidth = qMax(1, width() - handleWidth);
        const int x = qBound(0, event->position().toPoint().x() - handleWidth / 2, availableWidth);
        const int value = QStyle::sliderValueFromPosition(minimum(), maximum(), x, availableWidth);
        setValue(value);
    }

    QSlider::mousePressEvent(event);
}
