#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_image.h>
#endif

using namespace std;

// Класс Board управляет отрисовкой доски и фигур
class Board
{
public:
    // Конструктор по умолчанию
    Board() = default;

    // Конструктор, принимающий ширину и высоту окна
    Board(const unsigned int W, const unsigned int H) : W(W), H(H) {}

    // Функция инициализации графического интерфейса
    int start_draw()
    {
        // Инициализация SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }

        // Если размеры окна не заданы, используем разрешение экрана
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desktop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }

        // Создаем окно
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }

        // Создаем рендерер
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загружаем текстуры фигур и интерфейса
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());

        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        // Получаем размеры рендерера
        SDL_GetRendererOutputSize(ren, &W, &H);

        // Создаем начальное состояние доски
        make_start_mtx();
        rerender();
        return 0;
    }

    // Функция обновления экрана при перезапуске игры
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();
        clear_active();
        clear_highlight();
    }

    // Функция перемещения фигуры
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        // Если ход сопровождается побитием фигуры, удаляем побитую фигуру
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0;
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Перемещение фигуры на новые координаты
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }

        // Если фигура достигла конца доски, превращаем её в дамку
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;

        // Перемещаем фигуру
        mtx[i2][j2] = mtx[i][j];

        // Очищаем старую позицию
        drop_piece(i, j);

        // Добавляем ход в историю
        add_history(beat_series);
    }

    // Функция удаления фигуры с доски
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();
    }

    // Функция превращения фигуры в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;
        rerender();
    }

    // Функция получения текущего состояния доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Функция подсветки возможных ходов
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    // Функция очистки подсветки
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    // Функция подсветки активной клетки
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    // Функция очистки активной клетки
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    // Проверяет, подсвечена ли клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Функция отката хода
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());
        clear_highlight();
        clear_active();
    }

    // Функция показа результата игры
    void show_final(const int res)
    {
        game_results = res;
        rerender();
    }

    // Функция сброса размеров окна при изменении
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    // Освобождение памяти и завершение программы
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }
    // Деструктор класса, освобождает ресурсы при удалении объекта
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Добавляет текущее состояние доски в историю ходов
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }

    // Создает начальное состояние доски с расстановкой фигур
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0; // Очищаем клетку
                if (i < 3 && (i + j) % 2 == 1) // Черные шашки
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1) // Белые шашки
                    mtx[i][j] = 1;
            }
        }
        add_history(); // Добавляем в историю
    }

    // Перерисовывает доску и фигуры
    void rerender()
    {
        // Очищаем экран и рисуем доску
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Отрисовываем фигуры на доске
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j]) // Если клетка пуста, пропускаем
                    continue;

                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece; // Белая шашка
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece; // Черная шашка
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen; // Белая дамка
                else
                    piece_texture = b_queen; // Черная дамка

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // Подсветка возможных ходов (зеленым)
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // Подсветка выбранной клетки (красным)
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);

        // Кнопка "Назад"
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);

        // Кнопка "Перезапуск"
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Отображение результата игры
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path; // Победа белых
            else if (game_results == 2)
                result_path = black_path; // Победа черных

            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        SDL_RenderPresent(ren);

        // Нужно для macOS
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Функция записи ошибки в лог-файл
    void print_exception(const string& text)
    {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

  public:
      int W = 0; // Ширина окна
      int H = 0; // Высота окна

      // История состояний доски
      vector<vector<vector<POS_T>>> history_mtx;

  private:
      SDL_Window* win = nullptr; // Окно SDL
      SDL_Renderer* ren = nullptr; // Рендерер SDL

      // Текстуры для отображения
      SDL_Texture* board = nullptr;
      SDL_Texture* w_piece = nullptr;
      SDL_Texture* b_piece = nullptr;
      SDL_Texture* w_queen = nullptr;
      SDL_Texture* b_queen = nullptr;
      SDL_Texture* back = nullptr;
      SDL_Texture* replay = nullptr;

      // Пути к файлам текстур
      const string textures_path = project_path + "Textures/";
      const string board_path = textures_path + "board.png";
      const string piece_white_path = textures_path + "piece_white.png";
      const string piece_black_path = textures_path + "piece_black.png";
      const string queen_white_path = textures_path + "queen_white.png";
      const string queen_black_path = textures_path + "queen_black.png";
      const string white_path = textures_path + "white_wins.png";
      const string black_path = textures_path + "black_wins.png";
      const string draw_path = textures_path + "draw.png";
      const string back_path = textures_path + "back.png";
      const string replay_path = textures_path + "replay.png";

      // Координаты активной клетки
      int active_x = -1, active_y = -1;

      // Результат игры (-1 - нет результата, 1 - победа белых, 2 - победа черных, 0 - ничья)
      int game_results = -1;

      // Подсвеченные клетки (возможные ходы)
      vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));

      // Игровое поле (1 - белые, 2 - черные, 3 - белая дамка, 4 - черная дамка)
      vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));

      // История серий ударов
      vector<int> history_beat_series;
};
