#pragma once

#include <QPlainTextDocumentLayout>

class FixedHeightLayout : public QPlainTextDocumentLayout
{
public:
    explicit FixedHeightLayout(QTextDocument *document)
        : QPlainTextDocumentLayout(document) {}

    // Устанавливаем желаемую высоту строки
    void setFixedLineHeight(qreal height) { m_fixedHeight = height; }

    QRectF blockBoundingRect(const QTextBlock &block) const override
    {
        QRectF rect = QPlainTextDocumentLayout::blockBoundingRect(block);
        if (m_fixedHeight > 0.0)
            rect.setHeight(m_fixedHeight);
        return rect;
    }

private:
    qreal m_fixedHeight = 0.0;
};
