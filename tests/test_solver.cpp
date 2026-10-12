// Проверка расчета на задачах с известным ответом.
#include <cmath>
#include <cstdio>

#include "TorsionModel.h"

using namespace torsion;

static int failures = 0;

static void check(const char* what, double got, double expected, double relTol = 1e-6) {
    const double tol = relTol * std::fmax(1e-12, std::fabs(expected));
    if (std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s: получено %.9g, ожидалось %.9g\n", what, got, expected);
        ++failures;
    }
}

static void checkAbs(const char* what, double got, double expected, double absTol) {
    if (std::fabs(got - expected) > absTol) {
        std::printf("FAIL: %s: получено %.9g, ожидалось %.9g\n", what, got, expected);
        ++failures;
    }
}

static void checkTrue(const char* what, bool cond) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static Section circle(double L, double d, double M = 0.0, double m = 0.0) {
    Section s;
    s.length = L; s.shape = Shape::Circle; s.size = d; s.moment = M; s.distributed = m;
    return s;
}

static Section rect(double L, double b, double hb, double M = 0.0, double m = 0.0) {
    Section s;
    s.length = L; s.shape = Shape::Rectangle; s.size = b; s.ratio = hb; s.moment = M; s.distributed = m;
    return s;
}

static Section tube(double L, double D, double dD, double M = 0.0, double m = 0.0) {
    Section s;
    s.length = L; s.shape = Shape::Tube; s.size = D; s.ratio = dD; s.moment = M; s.distributed = m;
    return s;
}

int main() {
    // 1. Пример с sopromat.xyz (схема 2613514): заделка слева, G = 78000 МПа,
    //    1) круг 4 см, L = 1.2 м; 2) прямоугольник b = 2 см, h/b = 2, L = 2 м, M = 50;
    //    3) труба D = 6 см, d/D = 0.6, L = 0.8 м, M = 20; момент в конце вала -30.
    //    На сайте: T = 40, -10, -30; tau = 3.17 (3.18), 2.54, 0.813;
    //    phi = 0.00245, -0.00105, -0.00132.
    {
        Problem p;
        p.fixedStart = true;
        p.shearModulus = 78000.0;
        p.endMoment = -30.0;
        p.sections = {circle(1.2, 4.0), rect(2.0, 2.0, 2.0, 50.0), tube(0.8, 6.0, 0.6, 20.0)};
        Result r = solve(p);
        checkTrue("1: ok", r.ok);
        check("1: T1", r.sections[0].torqueStart, 40.0);
        check("1: T1 конец", r.sections[0].torqueEnd, 40.0);
        check("1: T2", r.sections[1].torqueStart, -10.0);
        check("1: T3", r.sections[2].torqueStart, -30.0);
        check("1: R", r.reactionStart, -(0 + 50 + 20 - 30.0));
        checkAbs("1: tau1", r.sections[0].tauStart, 3.18, 0.02);
        checkAbs("1: tau2", r.sections[1].tauStart, -2.54, 0.005);
        checkAbs("1: tau3", r.sections[2].tauStart, -0.813, 0.0005);
        checkAbs("1: phi1", r.sections[0].phi1, 0.00245, 0.000005);
        checkAbs("1: phi2", r.sections[1].phi1, -0.00105, 0.000005);
        checkAbs("1: phi3", r.sections[2].phi1, -0.00133, 0.000006);
    }

    // 2. Один круглый участок: заделка слева, момент 500 Н*м на конце, d = 4 см, L = 1 м.
    {
        Problem p;
        p.endMoment = 500.0;
        p.sections = {circle(1.0, 4.0)};
        Result r = solve(p);
        const double Jp = kPi * std::pow(40.0, 4) / 32.0;
        check("2: Jp", r.sections[0].torsionInertia, Jp);
        check("2: tau", r.sections[0].tauStart, 39.7887357729738, 1e-9);
        check("2: phi", r.sections[0].phi1, 500.0e3 * 1000.0 / (80000.0 * Jp));
        check("2: реакция", r.reactionStart, -500.0);
    }

    // 3. Статически неопределимый вал: две одинаковые секции, момент 1000 в середине.
    {
        Problem p;
        p.fixedStart = p.fixedEnd = true;
        p.sections = {circle(0.5, 3.0), circle(0.5, 3.0, 1000.0)};
        Result r = solve(p);
        checkTrue("3: ok", r.ok);
        checkTrue("3: неопределимость", r.staticallyIndeterminate);
        check("3: T1", r.sections[0].torqueStart, 500.0);
        check("3: T2", r.sections[1].torqueStart, -500.0);
        check("3: R_A", r.reactionStart, -500.0);
        check("3: R_B", r.reactionEnd, -500.0);
        checkAbs("3: phi в конце", r.sections[1].phi1, 0.0, 1e-12);
    }

    // 4. Разные сечения, обе заделки: угол на конце нулевой, реакции уравновешивают моменты.
    {
        Problem p;
        p.fixedStart = p.fixedEnd = true;
        p.endMoment = 100.0;
        p.sections = {circle(0.7, 5.0, 0.0, 30.0), tube(0.4, 3.0, 0.5, 1200.0),
                      rect(0.9, 2.0, 3.0, -400.0, -50.0)};
        Result r = solve(p);
        checkTrue("4: ok", r.ok);
        checkAbs("4: phi в конце", r.sections.back().phi1, 0.0, 1e-12);
        const double total = 100.0 + 1200.0 - 400.0 + 30.0 * 0.7 - 50.0 * 0.9;
        check("4: равновесие", r.reactionStart + r.reactionEnd + total, 0.0 + 0.0, 1e-9);
        // момент между участками непрерывен там, где нет внешнего момента
        checkAbs("4: непрерывность T в конце", r.sections[2].torqueEnd, 100.0 + r.reactionEnd, 1e-9);
    }

    // 5. Заделка только на конце, момент в начале вала (момент первого участка).
    {
        Problem p;
        p.fixedStart = false;
        p.fixedEnd = true;
        p.sections = {circle(1.0, 4.0, 500.0)};
        Result r = solve(p);
        const double Jp = kPi * std::pow(40.0, 4) / 32.0;
        const double twist = 500.0e3 * 1000.0 / (80000.0 * Jp);
        check("5: T", r.sections[0].torqueStart, -500.0);
        checkAbs("5: phi на конце", r.sections[0].phi1, 0.0, 1e-15);
        check("5: phi в начале", r.sections[0].phi0, twist);
    }

    // 6. Распределенный момент: консоль, m = 100 Н*м/м, L = 2 м, d = 4 см.
    //    T(0) = m*L = 200, T(L) = 0, phi(L) = m*L^2/(2*G*Jp).
    {
        Problem p;
        p.sections = {circle(2.0, 4.0, 0.0, 100.0)};
        Result r = solve(p);
        const double Jp = kPi * std::pow(40.0, 4) / 32.0;
        check("6: T начало", r.sections[0].torqueStart, 200.0);
        checkAbs("6: T конец", r.sections[0].torqueEnd, 0.0, 1e-12);
        check("6: реакция", r.sections[0].torqueStart + r.reactionStart, 0.0 + 0.0, 1e-9);
        check("6: phi", r.sections[0].phi1, 100.0 * 4.0 / 2.0 * 1e6 / (80000.0 * Jp));
        check("6: phiAt(L)", phiAt(r.sections[0], 2.0), r.sections[0].phi1, 1e-9);
        check("6: phiAt(1)", phiAt(r.sections[0], 1.0),
              (200.0 * 1.0 - 100.0 * 0.5) * 1e6 / (80000.0 * Jp));
    }

    // 7. Труба и прямоугольник: геометрия.
    {
        Problem p;
        p.sections = {tube(1.0, 6.0, 0.6, 100.0), rect(1.0, 2.0, 2.0)};
        Result r = solve(p);
        const double c4 = 1.0 - std::pow(0.6, 4);
        check("7: Jp трубы", r.sections[0].torsionInertia, kPi * std::pow(60.0, 4) * c4 / 32.0);
        check("7: Wp трубы", r.sections[0].torsionResistance, kPi * std::pow(60.0, 3) * c4 / 16.0);
        check("7: Jk прямоугольника", r.sections[1].torsionInertia, 0.229 * 40.0 * std::pow(20.0, 3));
        check("7: Wk прямоугольника", r.sections[1].torsionResistance, 0.246 * 40.0 * 20.0 * 20.0);
        double a, b;
        rectangleCoefficients(1.25, a, b);
        check("7: интерполяция alpha", a, (0.208 + 0.231) / 2);
        check("7: интерполяция beta", b, (0.141 + 0.196) / 2);
    }

    // 8. Без заделок: равновесный набор проходит, неравновесный - ошибка.
    {
        Problem p;
        p.fixedStart = p.fixedEnd = false;
        p.sections = {circle(1.0, 4.0, -300.0)};
        p.endMoment = 300.0;
        checkTrue("8: равновесие допустимо", solve(p).ok);
        check("8: T", solve(p).sections[0].torqueStart, 300.0);
        p.endMoment = 100.0;
        checkTrue("8: неравновесие -> ошибка", !solve(p).ok);
    }

    // 9. Проверка исходных данных.
    {
        Problem p;
        checkTrue("9: нет участков", !solve(p).ok);
        p.sections = {circle(-1.0, 4.0)};
        checkTrue("9: отрицательная длина", !solve(p).ok);
        p.sections = {tube(1.0, 4.0, 1.0)};
        checkTrue("9: d/D = 1", !solve(p).ok);
        p.sections = {rect(1.0, 2.0, 0.5)};
        checkTrue("9: h/b < 1", !solve(p).ok);
        p.sections = {circle(1.0, 4.0)};
        p.shearModulus = 0.0;
        checkTrue("9: G = 0", !solve(p).ok);
    }

    if (failures == 0) std::printf("Все тесты пройдены\n");
    return failures == 0 ? 0 : 1;
}
