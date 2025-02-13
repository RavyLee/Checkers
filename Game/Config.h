#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h" // Подключаем путь к файлу настроек

class Config
{
public:
    // Конструктор класса, автоматически загружает настройки при создании объекта
    Config()
    {
        reload();
    }

    // Функция загружает данные из файла settings.json в переменную config
    void reload()
    {
        std::ifstream fin(project_path + "settings.json"); // Открываем файл настроек
        fin >> config;  // Считываем JSON-данные в переменную config
        fin.close();    // Закрываем файл
    }

    // Перегруженный оператор () для удобного доступа к настройкам
    // Пример использования: config("WindowSize", "Width");
    auto operator()(const string& setting_dir, const string& setting_name) const
    {
        return config[setting_dir][setting_name]; // Возвращает значение настройки
    }

private:
    json config; // Хранит загруженные настройки в формате JSON
};
