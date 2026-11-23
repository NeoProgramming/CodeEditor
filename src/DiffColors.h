// DiffColors.h
#pragma once
#include <QColor>

namespace DiffColors
{
    inline QColor diff()    { return QColor(255, 180, 0, 60); }   // неяркий
    inline QColor current() { return QColor(255, 180, 0, 130); }  // яркий
    inline QColor word()    { return QColor(255, 140, 0, 150); }  // пословная
}
