#pragma once
#include <stdlib.h>

typedef int8_t POS_T; // Определяем тип для хранения координат (целые числа от -128 до 127)

// Структура, представляющая ход в игре
struct move_pos
{
    POS_T x, y;             // Начальная позиция фигуры (откуда)
    POS_T x2, y2;           // Конечная позиция фигуры (куда)
    POS_T xb = -1, yb = -1; // Координаты побитой фигуры (если был удар)

    // Конструктор для обычного хода (без побитых фигур)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
        : x(x), y(y), x2(x2), y2(y2) {
    }

    // Конструктор для хода с побитой фигурой
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb) {
    }

    // Оператор сравнения "равно" (используется для проверки одинаковых ходов)
    bool operator==(const move_pos& other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }

    // Оператор сравнения "не равно"
    bool operator!=(const move_pos& other) const
    {
        return !(*this == other);
    }
};
