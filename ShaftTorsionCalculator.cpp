#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Section {
    std::string shape;  // "circle", "rectangle" или "tube"
    double L;           // длина, м
    double G;           // модуль сдвига, Па
    double M0;          // момент на начале участка, Н·м
    double m; 
    double M_end;

    // Параметры сечения:
    double d;  // для круга (диаметр) и трубы (внутренний диаметр)
    double D;  // для трубы (внешний диаметр)
    double a;  // для прямоугольника (ширина)
    double b;  // для прямоугольника (высота)

    // момент инерции J (м^4)
    double inertiaMoment() const {
        if (shape == "circle") {
            return M_PI * std::pow(d, 4) / 32.0;
        }
        else if (shape == "rectangle") {
            // приближённая формула J = a b^3 [1/3 - 0.21 (b/a)(1 - b^4/(12 a^4))]
            double ratio = b / a;
            double beta = (1.0 / 3.0) - 0.21 * ratio * (1 - std::pow(ratio, 4) / 12.0);
            return a * std::pow(b, 3) * beta;
        }
        else if (shape == "tube") {
            // J = π (D^4 - d^4) / 32
            return M_PI * (std::pow(D, 4) - std::pow(d, 4)) / 32.0;
        }
        return 0.0;
    }

    // Наружный радиус r для расчёта W и τ
    double outerRadius() const {
        if (shape == "circle") {
            return d / 2.0;
        }
        else if (shape == "rectangle") {
            // для прямоугольника r = b/2, 
            return 0.5 * std::sqrt(a * a + b * b);
        }
        else if (shape == "tube") {
            return D / 2.0;
        }
        return 0.0;
    }

    // Момент в точке x вдоль участка
    double momentAt(double x) const {
        return M0 - m * x;
    }

    // Модуль сопротивления сечения W = J / r (м^3)
    double sectionModulus() const {
        double J = inertiaMoment();
        double r = outerRadius();
        return J / r;
    }

    // Угол закручивания φ = M_end·L/(G·J)
    double twistAngle() const {
        double J = inertiaMoment();
        return (M_end * L) / (G*pow(10, 6) * (J * 1e8 )*pow(10,-8));
    }
};

int main() {
    std::cout << "=== Shaft Torsion Calculator ===\n";

    int n;
    std::cout << "Enter number of sections: ";
    std::cin >> n;
    std::cin.ignore(); // сброс остатка строки

    std::vector<Section> sections;
    sections.reserve(n);

    for (int i = 0; i < n; ++i) {
        Section s;
        std::cout << "\n--- Section #" << (i + 1) << " ---\n";

        std::cout << "Enter shape (circle, rectangle, tube): ";
        std::getline(std::cin, s.shape);

        if (s.shape == "circle") {
            double d_cm;
            std::cout << "Enter diameter d (cm): ";
            std::cin >> d_cm;
            s.d = d_cm / 100.0;
        }
        else if (s.shape == "rectangle") {
            double b_cm, h_over_b;
            std::cout << "Enter small side b (cm): ";
            std::cin >> b_cm;
            std::cout << "Enter ratio h/b: ";
            std::cin >> h_over_b;
            s.b = b_cm / 100.0;
            s.a = h_over_b * s.b;  // a == h
        }
        else if (s.shape == "tube") {
            double D_cm, d_over_D;
            std::cout << "Enter outer diameter D (cm): ";
            std::cin >> D_cm;
            std::cout << "Enter ratio d/D: ";
            std::cin >> d_over_D;
            s.D = D_cm / 100.0;
            s.d = d_over_D * s.D;
        }
        if (s.shape != "circle" && s.shape != "rectangle" && s.shape != "tube") {
            std::cout << "Incorrect data entry\n";
            return 1;  
        }

        std::cout << "Enter length L (m): ";
        std::cin >> s.L;

        std::cout << "Enter shear modulus G (Pa): ";
        std::cin >> s.G;

        std::cout << "Enter start moment M0 (N·m): ";
        std::cin >> s.M0;

        std::cout << "Enter end moment M_L (N·m) (use negative for 'from yourself'): ";
        std::cin >> s.M_end;

        s.m = (s.M0 - s.M_end) / s.L;
        std::cin.ignore();

        sections.push_back(s);
    }

    std::cout << "\n=== Results ===\n";
    std::cout << std::fixed << std::setprecision(6);

    double totalTwist = 0.0;
    for (int i = 0; i < n; ++i) {
        const auto& s = sections[i];
        double J = s.inertiaMoment();
        double W = s.sectionModulus();
        double M_end = s.momentAt(s.L);
        double phi = s.twistAngle();

        std::cout << "\nSection #" << (i + 1) << ":\n";
        std::cout << "  Shape                : " << s.shape << "\n";
        std::cout << "  Interia moment J     = " << J*1e8 << " m^4\n";
        std::cout << "  Section modulus W    = " << W*1e6 << " m^3\n";
        std::cout << "  Moment at start M0   = " << s.M0 << " N·m\n";
        std::cout << "  Moment at end M(L)   = " << M_end << " N·m\n";
        std::cout << "  Twist angle          = " << phi << " rad\n";
    }

    return 0;
}
