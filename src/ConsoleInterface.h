// ConsoleInterface.h - командный интерфейс ввода секций и граничных условий.
#pragma once

#include <iosfwd>
#include <string>

#include "TorsionModel.h"

class ConsoleInterface {
public:
    ConsoleInterface();

    // Интерактивный цикл: читает команды из in до команды quit или конца ввода.
    // startWithWizard - сначала провести пошаговый ввод данных (и показать результат).
    void run(std::istream& in, std::ostream& out, bool showPrompt, bool startWithWizard = false);

    // Пошаговый ввод: программа задает вопросы, затем считает и показывает эпюры.
    // false - ввод прерван (конец потока), введенные ранее данные не изменены.
    bool runWizard(std::ostream& out);

    // Выполняет одну команду. Возвращает false, если нужно завершить работу.
    bool execute(const std::string& line, std::ostream& out);

    // Читает команды из файла (формат файла = те же команды). false - файл не открыт.
    bool loadFile(const std::string& path, std::ostream& out);

    // Расчет и печать результатов (для пакетного режима).
    bool calculate(std::ostream& out);

    // Сохранение эпюр в PNG (для пакетного режима).
    bool savePng(const std::string& path, std::ostream& out);

    const torsion::Problem& problem() const { return problem_; }

private:
    torsion::Problem problem_;
    std::istream* in_;

    void printHelp(std::ostream& out) const;
    void printProblem(std::ostream& out) const;
    void printResult(const torsion::Result& r, std::ostream& out) const;
    bool saveFile(const std::string& path, std::ostream& out) const;
    void loadExample();
};
