#pragma once
#include <random>
#include <vector>
#include <algorithm>
#include <ctime>
#include <string>
#include <utility> // Для swap
using namespace std;

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

// Константа, представляющая очень большое число (используется для оценки крайне невыгодных позиций)
const int INF = 1e9;

class Logic
{
public:
    // Конструктор класса Logic принимает указатели на объекты Board и Config.
    Logic(Board* board, Config* config) : board(board), config(config)
    {
        // Инициализация генератора случайных чисел:
        // Если в настройках бота не включён режим "NoRandom", используем текущее время в качестве seed.
        // Иначе seed равен 0 (для воспроизводимости).
        rand_eng = std::default_random_engine((!((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0));

        // Устанавливаем режим оценки ходов бота (например, "NumberAndPotential" или иной) из настроек.
        scoring_mode = (*config)("Bot", "BotScoringType");

        // Устанавливаем уровень оптимизации (например, использование альфа-бета отсечений).
        optimization = (*config)("Bot", "Optimization");
    }

    // Функция, возвращающая лучшую последовательность ходов (цепочку ударов, если это необходимо)
    // для фигур заданного цвета.
    vector<move_pos> find_best_turns(const bool color)
    {
        // Очищаем векторы, хранящие индексы для восстановления последовательности ходов.
        next_best_state.clear();
        next_move.clear();

        // Запускаем рекурсивный поиск лучшего хода, начиная с текущей конфигурации доски.
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        // Собираем последовательность ходов, начиная с нулевого состояния.
        int cur_state = 0;
        vector<move_pos> res;
        do
        {
            res.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        } while (cur_state != -1 && next_move[cur_state].x != -1);
        return res;
    }

private:
    // Функция make_turn создаёт новую копию доски и выполняет на ней указанный ход.
    // Если в ходе происходит взятие фигуры, то соответствующая клетка очищается.
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Если ход включает взятие (xb != -1), удаляем фигуру соперника.
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        // Если пешка достигает противоположной стороны, она становится дамкой.
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Перемещаем фигуру в новую позицию, а старую клетку очищаем.
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // Функция calc_score оценивает текущую позицию на доске.
    // Чем ниже значение, тем выгоднее позиция для бота.
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // Инициализируем счетчики для фигур:
        // w  - количество обычных белых шашек,
        // wq - количество белых дамок,
        // b  - количество обычных черных шашек,
        // bq - количество черных дамок.
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                // Если выбран режим "NumberAndPotential", добавляем бонусы за продвижение пешек.
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        // Если бот не играет за белых, меняем показатели, чтобы оценка проводилась с точки зрения бота.
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Если у бота отсутствуют фигуры, позиция крайне невыгодна.
        if (w + wq == 0)
            return INF;
        // Если у противника отсутствуют фигуры, позиция максимально выгодна.
        if (b + bq == 0)
            return 0;
        // Коэффициент ценности дамки: по умолчанию 4, а при режиме "NumberAndPotential" – 5.
        int q_coef = (scoring_mode == "NumberAndPotential") ? 5 : 4;
        // Возвращаем отношение суммарной ценности фигур противника к ценности фигур бота.
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Функция find_first_best_turn ищет лучший ход среди последовательных ударов для конкретной фигуры.
    // Она используется для реализации цепочки ударов, когда после первого удара возможны последующие.
    //
    // Аргументы:
    // - mtx: текущая конфигурация доски.
    // - color: цвет текущего игрока.
    // - x, y: координаты фигуры (если начинается цепочка ударов, иначе -1).
    // - state: индекс текущего состояния в последовательности ходов.
    // - alpha: текущий параметр альфа для отсечения в алгоритме минимакс.
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1)
    {
        // Регистрируем новое состояние: добавляем фиктивное значение -1.
        next_best_state.push_back(-1);
        // Регистрируем ход с невалидными координатами по умолчанию.
        next_move.emplace_back(-1, -1, -1, -1);

        // Изначально лучший найденный счет равен -1 (для поиска максимального значения).
        double best_score = -1;

        // Если state не равен 0, ищем ходы для конкретной фигуры на позиции (x, y).
        if (state != 0)
            find_turns(x, y, mtx);

        // Сохраняем текущий набор возможных ходов и флаг наличия ударов.
        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Если ударов нет и мы находимся не в начале цепочки, переключаемся на стандартный минимакс.
        if (!have_beats_now && state != 0)
        {
            // Переключаем сторону, так как цепочка ударов завершена.
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        // Перебираем все возможные ходы для данной фигуры.
        for (auto turn : turns_now)
        {
            // Определяем индекс следующего состояния (для восстановления последовательности ходов).
            size_t next_state = next_move.size();
            double score;

            // Если возможен удар, продолжаем цепочку ударов (игрок не переключается).
            if (have_beats_now)
            {
                // Выполняем ход и рекурсивно ищем лучший последующий удар.
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else
            {
                // Если ударов нет, выполняем обычный ход и переключаем игрока.
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Если полученный счет лучше текущего лучшего, обновляем лучший счет и запоминаем ход.
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = (have_beats_now ? int(next_state) : -1);
                next_move[state] = turn;
            }
        }
        // Возвращаем лучший найденный счет для данной цепочки ходов.
        return best_score;
    }

    // Функция find_best_turns_rec реализует рекурсивный поиск лучшего хода по алгоритму минимакс
    // с отсечениями альфа-бета. Здесь происходит чередование между максимизирующим и минимизирующим игроками.
    //
    // Аргументы:
    // - mtx: текущая конфигурация доски.
    // - color: цвет текущего игрока.
    // - depth: текущая глубина рекурсии.
    // - alpha: значение альфа для отсечения.
    // - beta: значение бета для отсечения.
    // - x, y: если заданы, поиск ведётся для конкретной фигуры (цепочка ударов).
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Базовый случай: если достигнута максимальная глубина поиска, возвращаем оценку позиции.
        if (depth == Max_depth)
        {
            return calc_score(mtx, (depth % 2 == color));
        }

        // Если переданы координаты (x, y), ищем ходы для конкретной фигуры (для цепочки ударов).
        if (x != -1)
        {
            find_turns(x, y, mtx);
        }
        else
        {
            // Иначе ищем ходы для всех фигур текущего игрока.
            find_turns(color, mtx);
        }
        auto turns_now = turns;
        bool have_beats_now = have_beats; // Флаг наличия ударов.

        // Если в цепочке ударов удары закончились (флаг false) и координаты заданы,
        // переключаем игрока и увеличиваем глубину рекурсии.
        if (!have_beats_now && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если нет вообще возможных ходов, считаем, что состояние терминальное.
        if (turns.empty())
            return (depth % 2 ? 0 : INF);

        // Инициализируем переменные для хранения минимальной и максимальной оценки.
        double min_score = INF + 1;
        double max_score = -1;

        // Перебираем все найденные ходы.
        for (auto turn : turns_now)
        {
            double score = 0.0;
            if (!have_beats_now && x == -1)
            {
                // Если это обычный ход (без последовательных ударов), выполняем ход и переключаем игрока.
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else
            {
                // Если продолжается цепочка ударов, не переключаем игрока, а передаём новые координаты.
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }

            // Обновляем минимальное и максимальное значения оценки.
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Обновляем параметры альфа и бета для отсечения невыгодных ветвей.
            // Предполагается, что на четных глубинах ходит максимизирующий игрок,
            // а на нечетных — минимизирующий.
            if (depth % 2)
                alpha = max(alpha, max_score);
            else
                beta = min(beta, min_score);

            // Если обнаружено условие отсечения (alpha >= beta), прекращаем перебор ветвей.
            if (optimization != "O0" && alpha >= beta)
                return (depth % 2 ? max_score + 1 : min_score - 1);
        }
        // Возвращаем лучшую оценку в зависимости от типа игрока:
        // - Если максимизирующий (глубина нечетная), возвращаем max_score,
        // - Если минимизирующий (глубина четная), возвращаем min_score.
        return (depth % 2 ? max_score : min_score);
    }

public:
    // Функции для поиска возможных ходов.
    // find_turns(color) ищет ходы для всех фигур заданного цвета.
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // find_turns(x, y) ищет ходы для фигуры, находящейся в клетке (x, y).
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Поиск ходов для всех фигур заданного цвета по текущей конфигурации доски.
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        // Проходим по всем клеткам доски (8x8).
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Если клетка содержит фигуру, принадлежащую текущему игроку.
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    // Если найдены удары и ранее ударов не было, очищаем результирующий вектор.
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    // Если уже были удары или ударов ещё не было, добавляем найденные ходы.
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        // Перемешиваем найденные ходы для разнообразия (используем генератор случайных чисел).
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Поиск ходов для фигуры, находящейся в клетке (x, y), по заданной конфигурации доски.
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // Если фигура является пешкой (тип 1 или 2), проверяем возможность ударов.
        switch (type)
        {
        case 1:
        case 2:
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    // Если целевая клетка занята или клетка для взятия не содержит вражеской фигуры, пропускаем ход.
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // Если фигура является дамкой, проверяем возможность ударов по диагоналям.
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 < 8 && j2 < 8 && i2 >= 0 && j2 >= 0; i2 += i, j2 += j)
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
        // Если найдены удары, устанавливаем флаг и завершаем поиск.
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        // Если ударов нет, проверяем обычные ходы для пешек или дамок.
        switch (type)
        {
        case 1:
        case 2:
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
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 < 8 && j2 < 8 && i2 >= 0 && j2 >= 0; i2 += i, j2 += j)
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
    // Вектор для хранения найденных ходов.
    vector<move_pos> turns;
    // Флаг наличия ударов (capture moves) среди найденных ходов.
    bool have_beats;
    // Максимальная глубина рекурсии для алгоритма минимакс.
    int Max_depth;

private:
    // Генератор случайных чисел для перемешивания ходов.
    default_random_engine rand_eng;
    // Режим оценки ходов (например, "NumberAndPotential").
    string scoring_mode;
    // Уровень оптимизации (например, "O0" или иное).
    string optimization;
    // Вектор для хранения следующего хода в последовательности (используется для восстановления цепочки ударов).
    vector<move_pos> next_move;
    // Вектор для хранения индексов следующих состояний (для восстановления цепочки ходов).
    vector<int> next_best_state;
    // Указатель на объект Board.
    Board* board;
    // Указатель на объект Config.
    Config* config;
};
