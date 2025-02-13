#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    // Конструктор игры
    Game() 
        : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), 
          hand(&board), 
          logic(&board, &config)
    {
        // Очищаем лог-файл при старте игры
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Главная функция игры (основной игровой цикл)
    int play()
    {
        auto start = chrono::steady_clock::now(); // Засекаем время начала игры

        if (is_replay)
        {
            logic = Logic(&board, &config); // Пересоздаём объект логики
            config.reload(); // Перезагружаем настройки
            board.redraw(); // Перерисовываем игровое поле
        }
        else
        {
            board.start_draw(); // Отображаем начальное состояние доски
        }
        is_replay = false;

        int turn_num = -1; // Номер текущего хода
        bool is_quit = false; // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns"); // Максимальное число ходов

        // Игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;
            logic.find_turns(turn_num % 2); // Поиск возможных ходов

            if (logic.turns.empty()) // Если ходов нет, игра завершается
                break;

            // Определяем уровень сложности бота
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Если ход делает игрок, обрабатываем его ход
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT) // Игрок вышел
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY) // Игрок хочет переиграть
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK) // Игрок хочет отменить ход
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2); // Если ход делает бот
        }

        // Записываем время игры в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если нужно переиграть
        if (is_replay)
            return play();
        if (is_quit)
            return 0;

        // Определяем победителя (0 - ничья, 1 - победа белых, 2 - победа черных)
        int res = 2;
        if (turn_num == Max_turns)
        {
            res = 0; // Ничья
        }
        else if (turn_num % 2)
        {
            res = 1; // Победа белых
        }

        // Показываем финальный экран
        board.show_final(res);
        auto resp = hand.wait();

        if (resp == Response::REPLAY) // Если игрок хочет переиграть
        {
            is_replay = true;
            return play();
        }

        return res;
    }

  private:
    // Функция, выполняющая ход бота
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");

        // Создаем поток для задержки перед ходом
        thread th(SDL_Delay, delay_ms);
        auto turns = logic.find_best_turns(color);
        th.join();
        bool is_first = true;

        // Выполняем все ходы
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // Функция, обрабатывающая ход игрока
    Response player_turn(const bool color)
    {
        // Выделяем доступные ходы на доске
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells); // Подсветка доступных клеток

        move_pos pos = { -1, -1, -1, -1 };
        POS_T x = -1, y = -1;

        // Ожидаем, пока игрок сделает ход
        while (true)
        {
            auto resp = hand.get_cell(); // Получаем ячейку, на которую кликнул игрок

            // Если игрок отменил ход или вышел
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);

            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

            // Проверяем, выбрана ли корректная клетка
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
            }

            // Если клетка некорректная, ждём новый выбор
            if (!is_correct)
                continue;

            // Игрок выбрал клетку с фигурой
            x = cell.first;
            y = cell.second;
            board.clear_highlight(); // Убираем старую подсветку
            board.set_active(x, y);  // Подсвечиваем выбранную клетку
        }

        // Завершаем ход, двигаем фигуру
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);

        return Response::OK;
    }

  private:
    Config config;  // Конфигурация игры
    Board board;    // Игровое поле
    Hand hand;      // Взаимодействие с игроком
    Logic logic;    // Логика игры
    int beat_series; // Количество последовательных ударов
    bool is_replay = false; // Флаг для переигровки
};
