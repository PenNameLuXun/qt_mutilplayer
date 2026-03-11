#pragma once

#include <QSlider>

class ClickableSlider : public QSlider
{
    Q_OBJECT

public:
    explicit ClickableSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
