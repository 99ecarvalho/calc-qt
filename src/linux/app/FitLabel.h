// Right-aligned label that shrinks its font so the whole text stays visible,
// like the primary display of the Windows app.
#pragma once

#include <QLabel>

class FitLabel final : public QLabel
{
    Q_OBJECT

public:
    explicit FitLabel(QWidget* parent = nullptr);

    void setFitText(const QString& text);
    void setSizeRange(int minPx, int maxPx);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void refit();

    int m_minPx = 16;
    int m_maxPx = 48;
};
