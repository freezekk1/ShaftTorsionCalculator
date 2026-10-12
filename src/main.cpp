// ShaftTorsionCalculator - расчет стержня на кручение.
// Этап 1: консольный интерфейс. Этап 2: эпюры средствами Qt (QPainter).
//
// Запуск:
//   ShaftTorsionCalculator                      пошаговый ввод данных, расчет и эпюры
//   ShaftTorsionCalculator data.txt             загрузить данные и перейти в интерактивный режим
//   ShaftTorsionCalculator data.txt --batch     посчитать, вывести результат и выйти
//   ShaftTorsionCalculator data.txt --batch --png out.png   то же + сохранить эпюры
//   ShaftTorsionCalculator data.txt --gui       посчитать и показать окно с эпюрами
#include <cstring>
#include <iostream>
#include <string>

#include "ConsoleInterface.h"
#include "Plotter.h"

#ifdef _WIN32
#include <windows.h>
#endif

static void printUsage() {
    std::cout << "Использование: ShaftTorsionCalculator [файл] [--batch] [--png файл.png] [--gui]\n";
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::string file, pngPath;
    bool batch = false, gui = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--batch") {
            batch = true;
        } else if (a == "--gui") {
            gui = true;
        } else if (a == "--png" && i + 1 < argc) {
            pngPath = argv[++i];
        } else if (a == "-h" || a == "--help") {
            printUsage();
            return 0;
        } else if (a.rfind("--", 0) == 0) {
            std::cerr << "Неизвестный параметр: " << a << "\n";
            printUsage();
            return 2;
        } else {
            file = a;
        }
    }

    ConsoleInterface console;

    if (!file.empty() && !console.loadFile(file, std::cout))
        return 1;

    if (batch || gui || !pngPath.empty()) {
        if (file.empty()) {
            std::cerr << "Для пакетного режима нужен файл с данными.\n";
            return 2;
        }
        if (!console.calculate(std::cout)) return 1;
        if (!pngPath.empty() && !console.savePng(pngPath, std::cout)) return 1;
        if (gui) console.execute("plot", std::cout);
        return 0;
    }

    // Без файла данных начинаем с пошагового ввода; с файлом - сразу режим команд.
    console.run(std::cin, std::cout, true, /*startWithWizard=*/file.empty());
    return 0;
}
