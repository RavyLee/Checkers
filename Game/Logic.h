#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
    // Конструктор класса Logic, принимает указатели на Board и Config
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        // Инициализация генератора случайных чисел в зависимости от настроек
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        // Установка режима оценки ходов бота
        scoring_mode = (*config)("Bot", "BotScoringType");
        // Установка уровня оптимизации
        optimization = (*config)("Bot", "Optimization");
    }
  
  
private:
    // Функция, выполняющая ход на переданной копии игровой доски и возвращающая новую доску
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1) // Если шашка была съедена, удаляем её
            mtx[turn.xb][turn.yb] = 0;
        // Если пешка дошла до конца доски, она становится дамкой
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Выполняем перемещение фигуры
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }
// Функция calc_score оценивает текущую позицию на доске и возвращает числовое значение,
// отражающее соотношение силы фигур противника и фигур бота.
// Чем меньше возвращаемое значение, тем лучше позиция для бота (0 – выигрышная позиция).
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
            // Инициализация переменных для подсчёта фигур:
    // w  - количество обычных шашек "белого" цвета,
    // wq - количество дамок "белого" цвета,
    // b  - количество обычных шашек "чёрного" цвета,
    // bq - количество дамок "чёрного" цвета.
        double w = 0, wq = 0, b = 0, bq = 0; // Счётчики фигур на доске
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Если в клетке находится обычная белая шашка (код 1), увеличиваем счетчик w
                w += (mtx[i][j] == 1);
                // Если в клетке находится белая дамка (код 3), увеличиваем счетчик wq
                wq += (mtx[i][j] == 3);
                // Если в клетке находится обычная чёрная шашка (код 2), увеличиваем счетчик b
                b += (mtx[i][j] == 2);
                // Если в клетке находится чёрная дамка (код 4), увеличиваем счетчик bq
                bq += (mtx[i][j] == 4);
                // Если выбран режим оценки "NumberAndPotential",
                // то к обычным шашкам добавляется бонус, зависящий от их продвижения по доске.
                // Для белых шашек бонус пропорционален расстоянию от последней строки (7 - i),
                // для чёрных – расстоянию от первой строки (i).
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        // Если бот не играет за белых (first_bot_color == false),
        // меняем местами показатели фигур, чтобы оценка всегда проводилась
        // с точки зрения максимизирующего игрока (бота).
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Если у бота (считаем, что его фигуры теперь соответствуют "белым") нет ни обычных шашек, ни дамок,
        // возвращаем значение INF – крайне невыгодное положение.
        if (w + wq == 0)
            return INF;
        // Если у противника (фигуры "чёрных") нет ни обычных шашек, ни дамок,
        // возвращаем 0 – позиция, максимально благоприятная для бота.
        if (b + bq == 0)
            return 0;
        // Коэффициент, определяющий ценность дамки по отношению к обычной шашке.
        // По умолчанию дамки ценятся в 4 раза выше обычных шашек.
        // Если включен режим "NumberAndPotential", коэффициент повышается до 5.
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        // Рассчитывается отношение суммарной ценности фигур противника к суммарной ценности фигур бота.
        // Формула: (количество обычных фигур противника + дамки противника * q_coef) / 
        //          (количество обычных фигур бота + дамки бота * q_coef)
        // Чем ниже результат, тем выгоднее позиция бота.
        return (b + bq * q_coef) / (w + wq * q_coef);
    }


public:
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        // Проходим по всем клеткам доски (8 строк по 8 клеток)
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns;
    bool have_beats;
    int Max_depth;

  private:
    default_random_engine rand_eng;
    string scoring_mode;
    string optimization;
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board;
    Config *config;
};
