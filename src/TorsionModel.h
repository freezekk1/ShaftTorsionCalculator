// TorsionModel.h - модель стержня и расчет на кручение (чистый C++17, без Qt).
//
// Входные данные устроены так же, как на https://sopromat.xyz/projects/torsion :
//  * заделка в начале и/или в конце, модуль сдвига G (МПа);
//  * участки слева направо: длина (м), форма сечения (круг / прямоугольник / труба),
//    размер (см), сосредоточенный момент M в НАЧАЛЕ участка (Н*м),
//    распределенный по длине участка момент m (Н*м/м);
//  * момент в конце вала (Н*м).
//
// Соглашения о знаках:
//  * Ось x направлена от начала стержня (x = 0) к концу (x = L).
//  * Внешний момент положителен, если его вектор направлен вдоль +x
//    ("на себя" при взгляде со стороны конца стержня). Моменты "от себя" - отрицательные.
//  * Внутренний крутящий момент T положителен, если его вектор направлен
//    по внешней нормали сечения (правило сечений).
#pragma once

#include <string>
#include <vector>

namespace torsion {

enum class Shape {
    Circle,    // сплошной круг: size = диаметр d
    Rectangle, // прямоугольник: size = меньшая сторона b, ratio = h/b (>= 1)
    Tube       // труба: size = наружный диаметр D, ratio = d/D (0 <= ratio < 1)
};

struct Section {
    double length = 1.0;       // длина участка, м
    Shape shape = Shape::Circle;
    double size = 4.0;         // размер сечения, см (смысл зависит от формы)
    double ratio = 0.0;        // h/b для прямоугольника, d/D для трубы
    double moment = 0.0;       // сосредоточенный момент M в начале участка, Н*м
    double distributed = 0.0;  // распределенный момент m по длине участка, Н*м/м
};

struct Problem {
    bool fixedStart = true;        // заделка в начале стержня (x = 0)
    bool fixedEnd = false;         // заделка в конце стержня (x = L)
    double shearModulus = 80000.0; // модуль сдвига G, МПа
    double endMoment = 0.0;        // момент в конце вала, Н*м
    std::vector<Section> sections;
};

// Результаты расчета по одному участку. Внутри участка T меняется линейно
// (если m != 0), угол поворота phi - по параболе.
struct SectionResult {
    double x0 = 0.0, x1 = 0.0;          // границы участка, м
    double torqueStart = 0.0;           // T в начале участка, Н*м
    double torqueEnd = 0.0;             // T в конце участка, Н*м
    double torsionInertia = 0.0;        // Jk (для круга и трубы - Jp), мм^4
    double torsionResistance = 0.0;     // Wk (для круга и трубы - Wp), мм^3
    double tauStart = 0.0, tauEnd = 0.0;// наибольшие касательные напряжения, МПа
    double twist = 0.0;                 // угол закручивания участка, рад
    double phi0 = 0.0, phi1 = 0.0;      // угол поворота сечения в начале / в конце, рад
    double phiLinear = 0.0;             // phi(s) = phi0 + phiLinear*s + phiQuad*s^2,
    double phiQuad = 0.0;               //   s - расстояние от начала участка, м
};

struct Result {
    bool ok = false;
    std::string error;          // описание ошибки, если ok == false
    std::string note;           // пояснение (например, о статической неопределимости)
    bool staticallyIndeterminate = false;
    double reactionStart = 0.0; // реактивный момент в заделке x = 0, Н*м
    double reactionEnd = 0.0;   // реактивный момент в заделке x = L, Н*м
    std::vector<SectionResult> sections;
    double totalLength = 0.0;   // м
    double maxAbsTorque = 0.0;  // Н*м
    double maxAbsTau = 0.0;     // МПа
    double maxAbsPhi = 0.0;     // рад
    int maxTauSection = -1;     // индекс участка с наибольшим |tau|
};

// Проверка корректности исходных данных. Пустая строка - все хорошо.
std::string validate(const Problem& problem);

// Основной расчет. При ошибке данных возвращает Result с ok == false.
Result solve(const Problem& problem);

// Значения внутри участка (s - расстояние от начала участка, м).
double torqueAt(const SectionResult& s, double sMeters);
double phiAt(const SectionResult& s, double sMeters);

// Коэффициенты кручения прямоугольника: tau_max = T/(alpha*h*b^2), Jk = beta*h*b^3.
void rectangleCoefficients(double hOverB, double& alpha, double& beta);

constexpr double kPi = 3.14159265358979323846;

inline double toDegrees(double rad) { return rad * 180.0 / kPi; }

const char* shapeName(Shape shape); // "круг", "прямоугольник", "труба"

} // namespace torsion
