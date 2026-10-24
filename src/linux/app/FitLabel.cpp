#include "FitLabel.h"

#include <QFontMetrics>

FitLabel::FitLabel(QWidget* parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setMinimumWidth(1);
}

void FitLabel::setFitText(const QString& text)
{
    setText(text);
    refit();
}

void FitLabel::setSizeRange(int minPx, int maxPx)
{
    m_minPx = minPx;
    m_maxPx = maxPx;
    refit();
}

void FitLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    refit();
}

void FitLabel::refit()
{
    const int avail = contentsRect().width() - 4;
    QFont f = font();
    int px = m_maxPx;
    for (; px > m_minPx; px -= 2)
    {
        f.setPixelSize(px);
        if (QFontMetrics(f).horizontalAdvance(text()) <= avail)
            break;
    }
    f.setPixelSize(px);
    setFont(f);
}
