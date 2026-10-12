#include "TorsionModel.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace torsion {

namespace {

struct Geometry {
    double Jk; // мм^4
    double Wk; // мм^3
};

Geometry geometry(const Section& s) {
    const double a = s.size * 10.0; // см -> мм
    switch (s.shape) {
    case Shape::Circle:
        return {kPi * std::pow(a, 4) / 32.0, kPi * std::pow(a, 3) / 16.0};
    case Shape::Tube: {
        const double k = 1.0 - std::pow(s.ratio, 4);
        return {kPi * std::pow(a, 4) * k / 32.0, kPi * std::pow(a, 3) * k / 16.0};
    }
    case Shape::Rectangle: {
        const double b = a;
        const double h = s.ratio * b;
        double alpha = 0.0, beta = 0.0;
        rectangleCoefficients(s.ratio, alpha, beta);
        return {beta * h * std::pow(b, 3), alpha * h * b * b};
    }
    }
    return {1.0, 1.0};
}

} // namespace

const char* shapeName(Shape shape) {
    switch (shape) {
    case Shape::Circle: return "круг";
    case Shape::Rectangle: return "прямоугольник";
    case Shape::Tube: return "труба";
    }
    return "?";
}

void rectangleCoefficients(double r, double& alpha, double& beta) {
    // Таблица коэффициентов для прямоугольного сечения (h/b, alpha, beta).
    static const double table[][3] = {
        {1.0, 0.208, 0.141}, {1.5, 0.231, 0.196}, {1.75, 0.239, 0.214}, {2.0, 0.246, 0.229},
        {2.5, 0.258, 0.249}, {3.0, 0.267, 0.263}, {4.0, 0.282, 0.281}, {6.0, 0.299, 0.299},
        {8.0, 0.307, 0.307}, {10.0, 0.313, 0.313},
    };
    constexpr size_t n = sizeof(table) / sizeof(table[0]);
    if (r <= table[0][0]) {
        alpha = table[0][1];
        beta = table[0][2];
        return;
    }
    if (r >= table[n - 1][0]) {
        // Узкая полоса: alpha ~ beta ~ (1 - 0.63*b/h)/3.
        alpha = beta = (1.0 - 0.63 / r) / 3.0;
        return;
    }
    for (size_t i = 0; i + 1 < n; ++i) {
        if (r <= table[i + 1][0]) {
            const double t = (r - table[i][0]) / (table[i + 1][0] - table[i][0]);
            alpha = table[i][1] + t * (table[i + 1][1] - table[i][1]);
            beta = table[i][2] + t * (table[i + 1][2] - table[i][2]);
            return;
        }
    }
}

double torqueAt(const SectionResult& s, double sMeters) {
    const double len = s.x1 - s.x0;
    if (len <= 0.0) return s.torqueStart;
    return s.torqueStart + (s.torqueEnd - s.torqueStart) * (sMeters / len);
}

double phiAt(const SectionResult& s, double sMeters) {
    return s.phi0 + s.phiLinear * sMeters + s.phiQuad * sMeters * sMeters;
}

std::string validate(const Problem& p) {
    std::ostringstream err;
    if (p.sections.empty()) return "Не задано ни одного участка стержня.";
    if (!std::isfinite(p.shearModulus) || p.shearModulus <= 0.0)
        return "Модуль сдвига G должен быть положительным числом.";
    if (!std::isfinite(p.endMoment)) return "Момент в конце вала задан некорректно.";

    for (size_t i = 0; i < p.sections.size(); ++i) {
        const Section& s = p.sections[i];
        err.str("");
        err << "Участок " << (i + 1) << ": ";
        if (!std::isfinite(s.length) || s.length <= 0.0)
            return err.str() + "длина должна быть больше нуля.";
        if (!std::isfinite(s.size) || s.size <= 0.0)
            return err.str() + "размер сечения должен быть больше нуля.";
        if (!std::isfinite(s.moment) || !std::isfinite(s.distributed))
            return err.str() + "момент задан некорректно.";
        if (s.shape == Shape::Rectangle && (!std::isfinite(s.ratio) || s.ratio < 1.0))
            return err.str() + "отношение h/b должно быть не меньше 1.";
        if (s.shape == Shape::Tube && (!std::isfinite(s.ratio) || s.ratio < 0.0 || s.ratio >= 1.0))
            return err.str() + "отношение d/D должно быть от 0 до 1 (1 не включается).";
    }
    return {};
}

Result solve(const Problem& p) {
    Result r;
    r.error = validate(p);
    if (!r.error.empty()) return r;

    const size_t n = p.sections.size();

    // Сосредоточенные моменты в узлах 0..n: узел k - начало участка k, узел n - конец вала.
    std::vector<double> nodeMoment(n + 1);
    for (size_t k = 0; k < n; ++k) nodeMoment[k] = p.sections[k].moment;
    nodeMoment[n] = p.endMoment;

    std::vector<Geometry> geo(n);
    std::vector<double> q(n); // суммарный распределенный момент участка, Н*м
    for (size_t i = 0; i < n; ++i) {
        geo[i] = geometry(p.sections[i]);
        q[i] = p.sections[i].distributed * p.sections[i].length;
        r.totalLength += p.sections[i].length;
    }

    // Метод сечений по правой отсеченной части (опора на конце пока отброшена):
    // tEnd0[i] - момент в конце участка i, tStart0[i] - в начале.
    std::vector<double> tStart0(n), tEnd0(n);
    {
        double acc = 0.0; // момент в начале следующего участка
        for (size_t i = n; i-- > 0;) {
            tEnd0[i] = acc + nodeMoment[i + 1];
            tStart0[i] = tEnd0[i] + q[i];
            acc = tStart0[i];
        }
    }
    const double totalMoment = nodeMoment[0] + tStart0[0]; // сумма всех внешних моментов

    double scale = 0.0;
    for (double m : nodeMoment) scale = std::max(scale, std::fabs(m));
    for (double v : q) scale = std::max(scale, std::fabs(v));
    if (scale == 0.0) scale = 1.0;

    double X = 0.0; // реактивный момент в заделке на конце, входит во все T
    if (p.fixedStart && p.fixedEnd) {
        // Статически неопределимая система: полный угол закручивания равен нулю.
        r.staticallyIndeterminate = true;
        double num = 0.0, den = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double c = p.sections[i].length / geo[i].Jk; // L/(G*Jk), G сокращается
            num += 0.5 * (tStart0[i] + tEnd0[i]) * c;
            den += c;
        }
        X = -num / den;
        r.reactionEnd = X;
        r.reactionStart = -(totalMoment + X);
        r.note = "Система статически неопределима (заделки с обеих сторон): "
                 "реакции найдены из условия равенства нулю полного угла закручивания.";
    } else if (p.fixedStart) {
        r.reactionStart = -totalMoment;
    } else if (p.fixedEnd) {
        X = -totalMoment;
        r.reactionEnd = X;
    } else {
        if (std::fabs(totalMoment) > 1e-9 * scale) {
            r.error = "Заделок нет, а сумма внешних моментов не равна нулю "
                      "(сумма = " + std::to_string(totalMoment) +
                      " Н*м): стержень не находится в равновесии.";
            return r;
        }
        r.note = "Заделок нет: углы поворота отсчитываются от начала стержня.";
    }

    // Напряжения и углы поворота.
    r.sections.resize(n);
    const double G = p.shearModulus;
    double x = 0.0;
    double phi = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Section& s = p.sections[i];
        SectionResult& sr = r.sections[i];
        sr.x0 = x;
        sr.x1 = x + s.length;
        sr.torqueStart = tStart0[i] + X;
        sr.torqueEnd = tEnd0[i] + X;
        sr.torsionInertia = geo[i].Jk;
        sr.torsionResistance = geo[i].Wk;
        // T [Н*м] -> [Н*мм] (x1000); Wk в мм^3 -> МПа.
        sr.tauStart = sr.torqueStart * 1000.0 / geo[i].Wk;
        sr.tauEnd = sr.torqueEnd * 1000.0 / geo[i].Wk;
        // phi(s) = int T ds / (G*Jk); Н*м*м -> Н*мм*мм (x1e6).
        const double k = 1e6 / (G * geo[i].Jk);
        sr.phiLinear = sr.torqueStart * k;
        sr.phiQuad = -0.5 * s.distributed * k;
        sr.twist = 0.5 * (sr.torqueStart + sr.torqueEnd) * s.length * k;
        sr.phi0 = phi;
        phi += sr.twist;
        sr.phi1 = phi;
        x = sr.x1;
    }

    // Если закреплен только конец, нулевой угол - на конце.
    if (!p.fixedStart && p.fixedEnd) {
        const double shift = -r.sections.back().phi1;
        for (SectionResult& sr : r.sections) {
            sr.phi0 += shift;
            sr.phi1 += shift;
        }
    }

    // Нули с точностью до округления (например, угол на заделке) обнуляем.
    {
        double phiScale = 0.0;
        for (const SectionResult& sr : r.sections)
            phiScale = std::max({phiScale, std::fabs(sr.phi0), std::fabs(sr.phi1)});
        const double eps = 1e-12 * phiScale;
        for (SectionResult& sr : r.sections) {
            if (std::fabs(sr.phi0) < eps) sr.phi0 = 0.0;
            if (std::fabs(sr.phi1) < eps) sr.phi1 = 0.0;
        }
    }

    // Сводные величины (T и tau линейны по участку - максимум на краях; phi - парабола).
    for (size_t i = 0; i < n; ++i) {
        const SectionResult& sr = r.sections[i];
        r.maxAbsTorque = std::max({r.maxAbsTorque, std::fabs(sr.torqueStart), std::fabs(sr.torqueEnd)});
        const double tauMax = std::max(std::fabs(sr.tauStart), std::fabs(sr.tauEnd));
        if (tauMax > r.maxAbsTau || r.maxTauSection < 0) {
            r.maxAbsTau = tauMax;
            r.maxTauSection = static_cast<int>(i);
        }
        r.maxAbsPhi = std::max({r.maxAbsPhi, std::fabs(sr.phi0), std::fabs(sr.phi1)});
        if (sr.phiQuad != 0.0) {
            const double sv = -sr.phiLinear / (2.0 * sr.phiQuad); // вершина параболы
            if (sv > 0.0 && sv < sr.x1 - sr.x0)
                r.maxAbsPhi = std::max(r.maxAbsPhi, std::fabs(phiAt(sr, sv)));
        }
    }

    r.ok = true;
    return r;
}

} // namespace torsion
