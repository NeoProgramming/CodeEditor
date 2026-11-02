#pragma once

#include <QString>
#include <QFileInfo>

// Состояние «рабочего» редактора вкладки: единственного, который
// отображает реальный файл и участвует в Save / modified / заголовке.
struct PrimaryState
{
    QString filePath;          // пусто = Untitled
    int     untitledNumber = 0;
    bool    modified = false;

    QString displayName() const
    {
        if (!filePath.isEmpty())
            return QFileInfo(filePath).fileName();
        return QString("Untitled %1").arg(untitledNumber);
    }
};
