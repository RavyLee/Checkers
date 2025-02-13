#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand отвечает за обработку действий игрока (клики, выход, перезапуск)
class Hand
{
public:
    // Конструктор принимает указатель на игровое поле (Board)
    Hand(Board* board) : board(board)
    {
    }

    // Функция определяет, на какую клетку кликнул игрок, и возвращает ответ (Response)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        int x = -1, y = -1;
        int xc = -1, yc = -1;

        while (true) // Ожидаем действий игрока
        {
            if (SDL_PollEvent(&windowEvent)) // Проверяем события в окне
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT: // Игрок закрыл окно
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN: // Игрок кликнул мышкой
                    x = windowEvent.motion.x; // Координата X клика
                    y = windowEvent.motion.y; // Координата Y клика

                    // Определяем клетку на игровом поле
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK; // Игрок нажал "Назад"
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY; // Игрок нажал "Перезапуск"
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL; // Игрок выбрал клетку на доске
                    }
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT: // Обработка событий окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size(); // Обновляем размер окна при изменении
                        break;
                    }
                }

                if (resp != Response::OK) // Если событие произошло, выходим из цикла
                    break;
            }
        }
        return { resp, xc, yc }; // Возвращаем тип события и координаты клетки
    }

    // Функция ожидания действий игрока (например, выход или перезапуск)
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true) // Ожидаем события
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT: // Игрок закрыл окно
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: // Игрок изменил размер окна
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: { // Игрок кликнул мышкой
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;

                    // Определяем, кликнул ли он в область "Перезапуск"
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY; // Игрок выбрал "Перезапуск"
                }
                                        break;
                }

                if (resp != Response::OK) // Если действие выполнено, выходим из цикла
                    break;
            }
        }
        return resp; // Возвращаем тип действия
    }

private:
    Board* board; // Указатель на игровое поле
};
