#include "ConsoleInterface.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "Plotter.h"

using torsion::Problem;
using torsion::Result;
using torsion::Section;
using torsion::Shape;

namespace {

// Разбор числа: допускает запятую как десятичный разделитель.
bool parseNumber(std::string token, double& value) {
    for (char& c : token)
        if (c == ',') c = '.';
    if (token.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(token.c_str(), &end);
    if (end == token.c_str() || *end != '\0' || !std::isfinite(v)) return false;
    value = v;
    return true;
}

std::vector<std::string> split(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string t;
    while (iss >> t) tokens.push_back(t);
    return tokens;
}

// Формат числа без хвоста нулей.
std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

bool parseShape(const std::string& w, Shape& shape) {
    if (w == "circle" || w == "c" || w == "круг") { shape = Shape::Circle; return true; }
    if (w == "rect" || w == "rectangle" || w == "r" || w == "прямоугольник") {
        shape = Shape::Rectangle;
        return true;
    }
    if (w == "tube" || w == "t" || w == "труба") { shape = Shape::Tube; return true; }
    return false;
}

const char* shapeKeyword(Shape s) {
    switch (s) {
    case Shape::Circle: return "circle";
    case Shape::Rectangle: return "rect";
    case Shape::Tube: return "tube";
    }
    return "circle";
}

// Разбор участка: <форма> <L> <размер> [<h/b или d/D>] [M] [m].
//   circle L d [M [m]]        rect L b h/b [M [m]]        tube L D d/D [M [m]]
bool parseSection(const std::vector<std::string>& args, size_t first, Section& s,
                  std::ostream& out) {
    static const char* usage =
        "Форматы (L - м, размеры - см, M - Н*м, m - Н*м/м; M и m можно не указывать):\n"
        "  add circle <L> <d> [M] [m]            круг, d - диаметр\n"
        "  add rect   <L> <b> <h/b> [M] [m]      прямоугольник, b - меньшая сторона\n"
        "  add tube   <L> <D> <d/D> [M] [m]      труба, D - наружный диаметр, d/D от 0 до 1\n";
    if (args.size() <= first) { out << usage; return false; }

    Shape shape;
    if (!parseShape(args[first], shape)) {
        out << "Неизвестная форма сечения \"" << args[first] << "\" (circle, rect или tube).\n" << usage;
        return false;
    }

    std::vector<double> v;
    for (size_t i = first + 1; i < args.size(); ++i) {
        double x = 0.0;
        if (!parseNumber(args[i], x)) {
            out << "Не удалось разобрать число: \"" << args[i] << "\"\n";
            return false;
        }
        v.push_back(x);
    }

    const size_t needed = (shape == Shape::Circle) ? 2 : 3; // обязательные числа
    if (v.size() < needed || v.size() > needed + 2) {
        out << "Неверное число параметров.\n" << usage;
        return false;
    }

    Section r;
    r.shape = shape;
    r.length = v[0];
    r.size = v[1];
    r.ratio = (shape == Shape::Circle) ? 0.0 : v[2];
    r.moment = (v.size() > needed) ? v[needed] : 0.0;
    r.distributed = (v.size() > needed + 1) ? v[needed + 1] : 0.0;

    if (r.length <= 0.0) { out << "Длина участка должна быть больше нуля.\n"; return false; }
    if (r.size <= 0.0) { out << "Размер сечения должен быть больше нуля.\n"; return false; }
    if (shape == Shape::Rectangle && r.ratio < 1.0) {
        out << "Отношение h/b должно быть не меньше 1.\n";
        return false;
    }
    if (shape == Shape::Tube && (r.ratio < 0.0 || r.ratio >= 1.0)) {
        out << "Отношение d/D должно быть от 0 до 1 (1 не включается).\n";
        return false;
    }
    s = r;
    return true;
}

const char* fixName(const Problem& p) {
    if (p.fixedStart && p.fixedEnd) return "обе заделки (начало и конец)";
    if (p.fixedStart) return "заделка в начале (x = 0)";
    if (p.fixedEnd) return "заделка в конце (x = L)";
    return "без заделок";
}

// "a" или "a / b" для значений в начале и в конце участка.
std::string pair(double a, double b) {
    char buf[96];
    if (std::fabs(a - b) <= 1e-9 * std::fmax(1.0, std::fmax(std::fabs(a), std::fabs(b))))
        std::snprintf(buf, sizeof buf, "%.5g", a);
    else
        std::snprintf(buf, sizeof buf, "%.5g / %.5g", a, b);
    return buf;
}

} // namespace

ConsoleInterface::ConsoleInterface() : in_(&std::cin) {}

void ConsoleInterface::printHelp(std::ostream& out) const {
    out <<
        "Команды (числа можно вводить с точкой или запятой):\n"
        "  wizard                     пошаговый ввод всех данных с расчетом и эпюрами\n"
        "  fix start|end|both|none    граничные условия: заделка в начале / в конце / обе / нет\n"
        "  g <МПа>                    модуль сдвига G\n"
        "  mend <Н*м>                 момент в конце вала\n"
        "  add circle <L> <d> [M] [m]         добавить участок-круг\n"
        "  add rect <L> <b> <h/b> [M] [m]     прямоугольник (b - меньшая сторона)\n"
        "  add tube <L> <D> <d/D> [M] [m]     труба (D - наружный диаметр)\n"
        "       L - длина, м; размеры - см; M - момент в НАЧАЛЕ участка, Н*м;\n"
        "       m - распределенный момент, Н*м/м (M и m можно не указывать)\n"
        "  edit <n> <форма> ...       заменить участок номер n (параметры как в add)\n"
        "  del <n>                    удалить участок номер n\n"
        "  clear                      удалить все участки\n"
        "  show                       показать введенные данные\n"
        "  calc                       выполнить расчет\n"
        "  plot                       показать эпюры в окне (Qt)\n"
        "  png <файл.png>             сохранить эпюры в PNG (Qt)\n"
        "  save <файл> / load <файл>  сохранить / загрузить данные (текст из этих же команд)\n"
        "  example                    загрузить пример\n"
        "  help                       эта справка\n"
        "  quit                       выход\n"
        "\n"
        "Знаки: момент, направленный вдоль оси x (\"на себя\" при взгляде с конца вала),\n"
        "положительный; моменты \"от себя\" вводятся со знаком минус.\n";
}

void ConsoleInterface::printProblem(std::ostream& out) const {
    char buf[240];
    out << "Граничные условия: " << fixName(problem_) << "\n";
    std::snprintf(buf, sizeof buf, "Модуль сдвига G = %s МПа\n", num(problem_.shearModulus).c_str());
    out << buf;
    if (problem_.sections.empty()) {
        out << "Участков пока нет. Добавьте их командой add.\n";
    } else {
        out << "Участки (слева направо):\n";
        out << "  №  форма          L, м  размер, см  h/b или d/D  M в начале, Н*м  m, Н*м/м\n";
        for (size_t i = 0; i < problem_.sections.size(); ++i) {
            const Section& s = problem_.sections[i];
            const std::string ratio = (s.shape == Shape::Circle) ? "-" : num(s.ratio);
            // Кириллица в printf считается по байтам, поэтому форму выводим отдельно.
            std::snprintf(buf, sizeof buf, "  %-3zu", i + 1);
            out << buf << torsion::shapeName(s.shape);
            const size_t shapeCols = (s.shape == Shape::Rectangle) ? 13 : (s.shape == Shape::Circle ? 4 : 5);
            out << std::string(15 - shapeCols, ' ');
            std::snprintf(buf, sizeof buf, "%-6s %-11s %-12s %-16s %s\n", num(s.length).c_str(),
                          num(s.size).c_str(), ratio.c_str(), num(s.moment).c_str(),
                          num(s.distributed).c_str());
            out << buf;
        }
    }
    std::snprintf(buf, sizeof buf, "Момент в конце вала: %s Н*м\n", num(problem_.endMoment).c_str());
    out << buf;
}

void ConsoleInterface::printResult(const Result& r, std::ostream& out) const {
    char buf[320];
    out << "\n=== Результаты расчета ===\n";
    if (!r.note.empty()) out << r.note << "\n";

    out << "Реакции опор:\n";
    if (problem_.fixedStart) {
        std::snprintf(buf, sizeof buf, "  заделка в начале (x = 0): M = %.6g Н*м\n", r.reactionStart);
        out << buf;
    }
    if (problem_.fixedEnd) {
        std::snprintf(buf, sizeof buf, "  заделка в конце  (x = L): M = %.6g Н*м\n", r.reactionEnd);
        out << buf;
    }
    if (!problem_.fixedStart && !problem_.fixedEnd) out << "  опор нет\n";

    out << "\nУчастки (если значение меняется по участку, показано: в начале / в конце):\n";
    for (size_t i = 0; i < r.sections.size(); ++i) {
        const auto& s = r.sections[i];
        std::snprintf(buf, sizeof buf,
                      "  Участок %zu, x = %.4g..%.4g м (%s)\n"
                      "    T   = %s Н*м\n"
                      "    tau = %s МПа\n"
                      "    угол закручивания участка = %.5g рад, угол поворота сечения в конце = %.5g рад\n"
                      "    Jk = %.5g см^4, Wk = %.5g см^3\n",
                      i + 1, s.x0, s.x1, torsion::shapeName(problem_.sections[i].shape),
                      pair(s.torqueStart, s.torqueEnd).c_str(),
                      pair(s.tauStart, s.tauEnd).c_str(), s.twist, s.phi1,
                      s.torsionInertia / 1e4, s.torsionResistance / 1e3);
        out << buf;
    }

    out << "\nИтог:\n";
    std::snprintf(buf, sizeof buf, "  max |T|   = %.6g Н*м\n", r.maxAbsTorque);
    out << buf;
    std::snprintf(buf, sizeof buf, "  max |tau| = %.6g МПа (участок %d)\n", r.maxAbsTau,
                  r.maxTauSection + 1);
    out << buf;
    std::snprintf(buf, sizeof buf, "  max |phi| = %.6g рад = %.6g град\n", r.maxAbsPhi,
                  torsion::toDegrees(r.maxAbsPhi));
    out << buf;
    out << "(знак T и tau: плюс - момент направлен по внешней нормали сечения)\n";
}

bool ConsoleInterface::calculate(std::ostream& out) {
    const Result r = torsion::solve(problem_);
    if (!r.ok) {
        out << "Ошибка: " << r.error << "\n";
        return false;
    }
    printResult(r, out);
    return true;
}

bool ConsoleInterface::savePng(const std::string& path, std::ostream& out) {
    const Result r = torsion::solve(problem_);
    if (!r.ok) {
        out << "Ошибка: " << r.error << "\n";
        return false;
    }
    std::string err;
    if (!plotter::saveDiagrams(problem_, r, path, err)) {
        out << "Ошибка: " << err << "\n";
        return false;
    }
    out << "Эпюры сохранены: " << path << "\n";
    return true;
}

bool ConsoleInterface::saveFile(const std::string& path, std::ostream& out) const {
    std::ofstream f(path);
    if (!f) {
        out << "Не удалось открыть файл для записи: " << path << "\n";
        return false;
    }
    f << "# ShaftTorsionCalculator: исходные данные\n";
    f << "fix " << (problem_.fixedStart && problem_.fixedEnd ? "both"
                    : problem_.fixedStart                    ? "start"
                    : problem_.fixedEnd                      ? "end"
                                                             : "none")
      << "\n";
    f << "g " << num(problem_.shearModulus) << "\n";
    f << "mend " << num(problem_.endMoment) << "\n";
    f << "clear\n";
    for (const Section& s : problem_.sections) {
        f << "add " << shapeKeyword(s.shape) << " " << num(s.length) << " " << num(s.size);
        if (s.shape != Shape::Circle) f << " " << num(s.ratio);
        f << " " << num(s.moment) << " " << num(s.distributed) << "\n";
    }
    out << "Данные сохранены: " << path << "\n";
    return true;
}

bool ConsoleInterface::loadFile(const std::string& path, std::ostream& out) {
    std::ifstream f(path);
    if (!f) {
        out << "Не удалось открыть файл: " << path << "\n";
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (!execute(line, out)) break;
    }
    return true;
}

void ConsoleInterface::loadExample() {
    // Пример с sopromat.xyz: заделка слева, G = 78000 МПа,
    // круг 4 см / прямоугольник 2 см, h/b = 2 / труба 6 см, d/D = 0.6, момент в конце вала -30.
    problem_ = Problem{};
    problem_.fixedStart = true;
    problem_.fixedEnd = false;
    problem_.shearModulus = 78000.0;
    problem_.endMoment = -30.0;
    Section a;
    a.length = 1.2; a.shape = Shape::Circle; a.size = 4.0;
    Section b;
    b.length = 2.0; b.shape = Shape::Rectangle; b.size = 2.0; b.ratio = 2.0; b.moment = 50.0;
    Section c;
    c.length = 0.8; c.shape = Shape::Tube; c.size = 6.0; c.ratio = 0.6; c.moment = 20.0;
    problem_.sections = {a, b, c};
}

bool ConsoleInterface::execute(const std::string& rawLine, std::ostream& out) {
    // Комментарии и пустые строки.
    std::string line = rawLine.substr(0, rawLine.find('#'));
    const auto args = split(line);
    if (args.empty()) return true;

    const std::string& cmd = args[0];

    if (cmd == "quit" || cmd == "exit" || cmd == "q") {
        return false;
    } else if (cmd == "help" || cmd == "?") {
        printHelp(out);
    } else if (cmd == "wizard") {
        if (!runWizard(out)) out << "Ввод прерван.\n";
    } else if (cmd == "show") {
        printProblem(out);
    } else if (cmd == "fix") {
        if (args.size() != 2) { out << "Использование: fix start|end|both|none\n"; return true; }
        const std::string& m = args[1];
        if (m == "start") { problem_.fixedStart = true;  problem_.fixedEnd = false; }
        else if (m == "end") { problem_.fixedStart = false; problem_.fixedEnd = true; }
        else if (m == "both") { problem_.fixedStart = true;  problem_.fixedEnd = true; }
        else if (m == "none") { problem_.fixedStart = false; problem_.fixedEnd = false; }
        else { out << "Неизвестный режим \"" << m << "\". Допустимо: start, end, both, none.\n";
               return true; }
        out << "Граничные условия: " << fixName(problem_) << "\n";
    } else if (cmd == "g") {
        double v = 0.0;
        if (args.size() != 2 || !parseNumber(args[1], v) || v <= 0.0) {
            out << "Использование: g <положительное число, МПа>\n";
            return true;
        }
        problem_.shearModulus = v;
        out << "G = " << num(v) << " МПа\n";
    } else if (cmd == "mend") {
        double v = 0.0;
        if (args.size() != 2 || !parseNumber(args[1], v)) {
            out << "Использование: mend <момент в конце вала, Н*м>\n";
            return true;
        }
        problem_.endMoment = v;
        out << "Момент в конце вала = " << num(v) << " Н*м\n";
    } else if (cmd == "add") {
        Section s;
        if (parseSection(args, 1, s, out)) {
            problem_.sections.push_back(s);
            out << "Добавлен участок " << problem_.sections.size() << "\n";
        }
    } else if (cmd == "edit") {
        double idx = 0.0;
        if (args.size() < 3 || !parseNumber(args[1], idx) || idx < 1 ||
            idx > static_cast<double>(problem_.sections.size()) || idx != std::floor(idx)) {
            out << "Использование: edit <n> <форма> <параметры> (n - номер существующего участка)\n";
            return true;
        }
        Section s;
        if (parseSection(args, 2, s, out)) {
            problem_.sections[static_cast<size_t>(idx) - 1] = s;
            out << "Участок " << static_cast<size_t>(idx) << " изменен\n";
        }
    } else if (cmd == "del") {
        double idx = 0.0;
        if (args.size() != 2 || !parseNumber(args[1], idx) || idx < 1 ||
            idx > static_cast<double>(problem_.sections.size()) || idx != std::floor(idx)) {
            out << "Использование: del <n> (n - номер существующего участка)\n";
            return true;
        }
        problem_.sections.erase(problem_.sections.begin() + (static_cast<long>(idx) - 1));
        out << "Участок " << static_cast<size_t>(idx) << " удален\n";
    } else if (cmd == "clear") {
        problem_.sections.clear();
        out << "Все участки удалены\n";
    } else if (cmd == "calc") {
        calculate(out);
    } else if (cmd == "plot") {
        const Result r = torsion::solve(problem_);
        if (!r.ok) { out << "Ошибка: " << r.error << "\n"; return true; }
        std::string err;
        if (!plotter::showDiagrams(problem_, r, err)) out << "Ошибка: " << err << "\n";
    } else if (cmd == "png") {
        if (args.size() != 2) { out << "Использование: png <файл.png>\n"; return true; }
        savePng(args[1], out);
    } else if (cmd == "save") {
        if (args.size() != 2) { out << "Использование: save <файл>\n"; return true; }
        saveFile(args[1], out);
    } else if (cmd == "load") {
        if (args.size() != 2) { out << "Использование: load <файл>\n"; return true; }
        loadFile(args[1], out);
    } else if (cmd == "example") {
        loadExample();
        out << "Загружен пример (схема с sopromat.xyz):\n";
        printProblem(out);
    } else {
        out << "Неизвестная команда \"" << cmd << "\". Введите help для справки.\n";
    }
    return true;
}

bool ConsoleInterface::runWizard(std::ostream& out) {
    if (!in_) return false;

    // Читает строку; false - конец ввода.
    auto readLine = [&](std::string& s) {
        if (!std::getline(*in_, s)) return false;
        const auto b = s.find_first_not_of(" \t\r\n");
        const auto e = s.find_last_not_of(" \t\r\n");
        s = (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
        return true;
    };

    // Спрашивает число, пока не получит корректное. Пустая строка - значение по умолчанию.
    auto askNumber = [&](const std::string& question, double def, double& value,
                         bool (*valid)(double), const char* hint) {
        while (true) {
            out << question << " [" << num(def) << "]: " << std::flush;
            std::string s;
            if (!readLine(s)) return false;
            if (s.empty()) { value = def; return true; }
            double v = 0.0;
            if (!parseNumber(s, v)) { out << "  Нужно число. Попробуйте еще раз.\n"; continue; }
            if (!valid(v)) { out << "  " << hint << "\n"; continue; }
            value = v;
            return true;
        }
    };

    auto any = [](double) { return true; };
    auto positive = [](double x) { return x > 0.0; };
    auto choice3 = [](double x) { return x == 1 || x == 2 || x == 3; };
    auto choice4 = [](double x) { return x == 1 || x == 2 || x == 3 || x == 4; };
    auto count = [](double x) { return x >= 1 && x <= 50 && x == std::floor(x); };
    auto ratioRect = [](double x) { return x >= 1.0; };
    auto ratioTube = [](double x) { return x >= 0.0 && x < 1.0; };

    Problem p;
    out << "\n--- Пошаговый ввод данных ---\n"
           "Enter без ввода - взять значение в квадратных скобках.\n"
           "Моменты, направленные \"от себя\", вводите со знаком минус.\n\n";

    double v = 0.0;
    if (!askNumber("Где заделка? 1 - в начале, 2 - в конце, 3 - с обеих сторон, 4 - нет", 1, v,
                   +choice4, "Введите число от 1 до 4."))
        return false;
    p.fixedStart = (v == 1 || v == 3);
    p.fixedEnd = (v == 2 || v == 3);

    if (!askNumber("Модуль сдвига G, МПа (сталь около 80000)", 80000, p.shearModulus, +positive,
                   "G должно быть больше нуля."))
        return false;

    double n = 1;
    if (!askNumber("Сколько участков (от 1 до 50)", 1, n, +count, "Введите целое число от 1 до 50."))
        return false;

    for (int i = 1; i <= static_cast<int>(n); ++i) {
        out << "\nУчасток " << i << " (слева направо):\n";
        Section s;
        double shape = 1;
        if (!askNumber("  Форма сечения: 1 - круг, 2 - прямоугольник, 3 - труба", 1, shape,
                       +choice3, "Введите 1, 2 или 3."))
            return false;
        if (!askNumber("  Длина, м", 1, s.length, +positive, "Длина должна быть больше нуля."))
            return false;

        if (shape == 1) {
            s.shape = Shape::Circle;
            if (!askNumber("  Диаметр d, см", 4, s.size, +positive, "Размер должен быть больше нуля."))
                return false;
        } else if (shape == 2) {
            s.shape = Shape::Rectangle;
            if (!askNumber("  Меньшая сторона b, см", 2, s.size, +positive,
                           "Размер должен быть больше нуля."))
                return false;
            if (!askNumber("  Отношение сторон h/b (не меньше 1)", 2, s.ratio, +ratioRect,
                           "h/b должно быть не меньше 1."))
                return false;
        } else {
            s.shape = Shape::Tube;
            if (!askNumber("  Наружный диаметр D, см", 6, s.size, +positive,
                           "Размер должен быть больше нуля."))
                return false;
            if (!askNumber("  Отношение d/D (от 0 до 1, единица не включается)", 0.6, s.ratio,
                           +ratioTube, "d/D должно быть от 0 до 1 (1 не включается)."))
                return false;
        }
        if (!askNumber("  Момент M в начале участка, Н*м", 0, s.moment, +any, "")) return false;
        if (!askNumber("  Распределенный момент m по длине участка, Н*м/м", 0, s.distributed, +any, ""))
            return false;
        p.sections.push_back(s);
    }

    out << "\n";
    if (!askNumber("Момент в конце вала, Н*м", 0, p.endMoment, +any, "")) return false;

    problem_ = p;
    out << "\n";
    printProblem(out);
    if (calculate(out)) {
        if (plotter::available()) {
            out << "\nОткрываю эпюры. Закройте окно, чтобы вернуться в консоль.\n";
            execute("plot", out);
        } else {
            out << "\n(Эпюры недоступны: программа собрана без Qt.)\n";
        }
    }
    out << "\nДальше можно ввести команду: wizard - новый ввод, edit/add/del - правка участков,\n"
           "calc - пересчитать, plot - эпюры, png <файл> - сохранить картинку, help, quit.\n";
    return true;
}

void ConsoleInterface::run(std::istream& in, std::ostream& out, bool showPrompt,
                           bool startWithWizard) {
    in_ = &in;
    out << "ShaftTorsionCalculator - расчет стержня на кручение\n";
    if (startWithWizard) {
        if (!runWizard(out)) {
            out << "\nВвод прерван.\n";
            return;
        }
    } else {
        out << "Введите help для списка команд, example - для готового примера.\n";
    }
    std::string line;
    while (true) {
        if (showPrompt) out << "> " << std::flush;
        if (!std::getline(in, line)) break;
        if (!execute(line, out)) break;
    }
}
