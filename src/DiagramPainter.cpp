#include "DiagramPainter.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QString>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace torsion;

namespace diagram {
namespace {

const QColor kTorqueColor(0x2b, 0x6c, 0xb0);
const QColor kTauColor(0xc0, 0x56, 0x21);
const QColor kPhiColor(0x2f, 0x85, 0x5a);
const QColor kReactionColor(0xc5, 0x30, 0x30);
const QColor kInk(0x1a, 0x20, 0x2c);
const QColor kGuide(0xa0, 0xae, 0xc0);

enum class Align { Center, Left, Right };

// Соответствие координаты x (м) координате на экране + размер шрифта.
struct Layout {
    double xL = 0, xR = 0; // экранные координаты x = 0 и x = L
    double total = 1;      // длина стержня, м
    double fontPx = 12;
    double X(double x) const { return xL + (xR - xL) * x / total; }
};

QString fmt(double v) {
    if (std::fabs(v) < 1e-12) return QStringLiteral("0");
    return QString::number(v, 'g', 4);
}

QString utf8(const char* s) { return QString::fromUtf8(s); }

void setFontPx(QPainter& p, double px, bool bold = false) {
    QFont f = p.font();
    f.setPixelSize(static_cast<int>(std::lround(px)));
    f.setBold(bold);
    p.setFont(f);
}

// Подпись с белой полупрозрачной подложкой. x - центр, левый или правый край подписи.
void drawLabel(QPainter& p, double x, double baselineY, const QString& text, const QColor& color,
               Align align = Align::Center) {
    const QFontMetricsF fm(p.font());
    const double w = fm.horizontalAdvance(text);
    const double left = (align == Align::Center) ? x - w / 2 : (align == Align::Left ? x : x - w);
    const QRectF box(left - 2, baselineY - fm.ascent() - 1, w + 4, fm.height() + 2);
    p.fillRect(box, QColor(255, 255, 255, 220));
    p.setPen(color);
    p.drawText(QPointF(left, baselineY), text);
}

// Подпись над точкой (above = true) или под ней.
void drawValueLabel(QPainter& p, double x, double y, bool above, const QString& text,
                    const QColor& color, Align align = Align::Center) {
    const QFontMetricsF fm(p.font());
    const double baseline = above ? y - 3 - fm.descent() : y + 3 + fm.ascent();
    drawLabel(p, x, baseline, text, color, align);
}

void drawNodeGuides(QPainter& p, const Layout& L, const Result& res, double yTop, double yBottom) {
    p.setPen(QPen(kGuide, 1.0, Qt::DashLine));
    p.drawLine(QPointF(L.X(0), yTop), QPointF(L.X(0), yBottom));
    for (const SectionResult& s : res.sections)
        p.drawLine(QPointF(L.X(s.x1), yTop), QPointF(L.X(s.x1), yBottom));
}

// Символ момента: дуга вокруг оси со стрелкой. positive - момент вдоль +x.
void drawTorqueArrow(QPainter& p, double cx, double cy, double a, double b, bool positive,
                     double headSize = 9.0, double lineWidth = 2.0) {
    const QRectF ell(cx - a, cy - b, 2 * a, 2 * b);
    QPainterPath path;
    path.arcMoveTo(ell, 45);
    path.arcTo(ell, 45, 270);
    p.setPen(QPen(kTorqueColor, lineWidth, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Наконечник на конце дуги (положительный) или в ее начале (отрицательный).
    const double theta = (positive ? 315.0 : 45.0) * 3.14159265358979 / 180.0;
    const QPointF tip(cx + a * std::cos(theta), cy - b * std::sin(theta));
    QPointF dir(-a * std::sin(theta), -b * std::cos(theta)); // касательная против часовой стрелки
    if (!positive) dir = -dir;
    dir /= std::hypot(dir.x(), dir.y());
    const QPointF perp(-dir.y(), dir.x());
    QPolygonF head;
    head << tip << (tip - dir * headSize + perp * headSize * 0.45)
         << (tip - dir * headSize - perp * headSize * 0.45);
    p.setBrush(kTorqueColor);
    p.setPen(Qt::NoPen);
    p.drawPolygon(head);
    p.setBrush(Qt::NoBrush);
}

void drawWall(QPainter& p, double x, double cy, double halfHeight, bool hatchLeft) {
    p.setPen(QPen(kInk, 3.0, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(QPointF(x, cy - halfHeight), QPointF(x, cy + halfHeight));
    p.setPen(QPen(kInk, 1.2));
    const double dx = hatchLeft ? -9.0 : 9.0;
    for (double y = cy - halfHeight; y < cy + halfHeight - 1; y += 8.0)
        p.drawLine(QPointF(x, y), QPointF(x + dx, y + 9.0));
}

// Подпись размеров участка; пробуем длинный вариант, потом покороче.
QString sectionCaption(const Section& s, size_t index, double availablePx, const QFontMetricsF& fm) {
    QString size, full, medium;
    const QString len = fmt(s.length);
    switch (s.shape) {
    case Shape::Circle:
        size = utf8("Ø") + fmt(s.size) + utf8(" см");
        break;
    case Shape::Rectangle:
        size = fmt(s.size) + utf8("×") + fmt(s.size * s.ratio) + utf8(" см");
        break;
    case Shape::Tube:
        size = utf8("Ø") + fmt(s.size) + QStringLiteral("/") + fmt(s.size * s.ratio) + utf8(" см");
        break;
    }
    full = QString::number(index + 1) + QStringLiteral(": ") + size + QStringLiteral(", L=") + len + utf8(" м");
    medium = size + QStringLiteral(", ") + len + utf8(" м");
    for (const QString& c : {full, medium, size})
        if (fm.horizontalAdvance(c) <= availablePx) return c;
    return {};
}

// Расчетная схема: стержень с сечениями, заделками, моментами и размерами.
void drawSchematic(QPainter& p, const QRectF& r, const Layout& L, const Problem& prob,
                   const Result& res) {
    const double fp = L.fontPx;
    const double labelH = fp + 8;
    const double captionH = fp * 1.8;
    const double rvPad = 14;
    double shaftMax = r.height() - labelH - captionH - 2 * rvPad;
    shaftMax = std::clamp(shaftMax, 24.0, 90.0);
    const double rvMax = shaftMax / 2 + rvPad;
    const double cy = r.top() + labelH + rvMax;

    double smax = 0.0;
    for (const Section& s : prob.sections) smax = std::max(smax, s.size);
    auto heightOf = [&](double size) { return std::max(shaftMax * 0.4, shaftMax * size / smax); };

    // Ось стержня.
    p.setPen(QPen(kGuide, 1.0, Qt::DashDotLine));
    p.drawLine(QPointF(L.xL - 14, cy), QPointF(L.xR + 14, cy));

    // Участки.
    for (size_t i = 0; i < prob.sections.size(); ++i) {
        const Section& s = prob.sections[i];
        const double x0 = L.X(res.sections[i].x0);
        const double x1 = L.X(res.sections[i].x1);
        const double h = heightOf(s.size);
        const QRectF body(x0, cy - h / 2, x1 - x0, h);
        p.setPen(QPen(kInk, 1.6));
        p.setBrush(QColor(0xe2, 0xe8, 0xf0));
        p.drawRect(body);
        p.setBrush(Qt::NoBrush);
        if (s.shape == Shape::Tube && s.ratio > 0.0) {
            const double hi = h * s.ratio;
            p.setPen(QPen(kInk, 1.0, Qt::DashLine));
            p.setBrush(QColor(255, 255, 255));
            p.drawRect(QRectF(x0, cy - hi / 2, x1 - x0, hi));
            p.setBrush(Qt::NoBrush);
        } else if (s.shape == Shape::Rectangle) {
            // Диагонали - условный знак прямоугольного сечения.
            p.setPen(QPen(kGuide, 1.0));
            p.drawLine(body.topLeft(), body.bottomRight());
            p.drawLine(body.bottomLeft(), body.topRight());
        }
    }

    // Заделки и реакции.
    const double wallHalf = shaftMax / 2 + 10;
    const QFontMetricsF fm(p.font());
    if (prob.fixedStart) {
        drawWall(p, L.xL, cy, wallHalf, true);
        p.setPen(kReactionColor);
        const QString a = QStringLiteral("R = ") + fmt(res.reactionStart);
        const QString b = utf8("Н·м");
        const double xr = L.xL - 16;
        p.drawText(QPointF(xr - fm.horizontalAdvance(a), cy - 2), a);
        p.drawText(QPointF(xr - fm.horizontalAdvance(b), cy - 2 + fm.height()), b);
    }
    if (prob.fixedEnd) {
        drawWall(p, L.xR, cy, wallHalf, false);
        p.setPen(kReactionColor);
        const QString a = QStringLiteral("R = ") + fmt(res.reactionEnd);
        const QString b = utf8("Н·м");
        const double xr = L.xR + 16;
        p.drawText(QPointF(xr, cy - 2), a);
        p.drawText(QPointF(xr, cy - 2 + fm.height()), b);
    }

    // Распределенные моменты: ряд маленьких дуг над участком.
    for (size_t i = 0; i < prob.sections.size(); ++i) {
        const double m = prob.sections[i].distributed;
        if (m == 0.0) continue;
        const double x0 = L.X(res.sections[i].x0);
        const double x1 = L.X(res.sections[i].x1);
        const int count = std::clamp(static_cast<int>((x1 - x0) / 24.0), 2, 14);
        const double bSmall = shaftMax / 2 + 7;
        for (int k = 0; k < count; ++k) {
            const double x = x0 + (x1 - x0) * (k + 0.5) / count;
            drawTorqueArrow(p, x, cy, 3.5, bSmall, m > 0.0, 5.0, 1.4);
        }
        drawValueLabel(p, (x0 + x1) / 2, cy - rvMax, true,
                       QStringLiteral("m = ") + fmt(m) + utf8(" Н·м/м"), kTorqueColor);
    }

    // Сосредоточенные моменты в узлах: начало каждого участка и конец вала.
    auto torqueAt = [&](double x, double value) {
        if (value == 0.0) return;
        drawTorqueArrow(p, x, cy, 7.0, rvMax, value > 0.0);
        drawValueLabel(p, x, cy - rvMax, true, fmt(value) + utf8(" Н·м"), kTorqueColor);
    };
    for (size_t i = 0; i < prob.sections.size(); ++i)
        torqueAt(L.X(res.sections[i].x0), prob.sections[i].moment);
    torqueAt(L.X(res.totalLength), prob.endMoment);

    // Подписи размеров под участками.
    p.setPen(kInk);
    for (size_t i = 0; i < prob.sections.size(); ++i) {
        const double x0 = L.X(res.sections[i].x0);
        const double x1 = L.X(res.sections[i].x1);
        const QString c = sectionCaption(prob.sections[i], i, (x1 - x0) - 4, fm);
        if (!c.isEmpty())
            p.drawText(QPointF((x0 + x1) / 2 - fm.horizontalAdvance(c) / 2, cy + rvMax + fp), c);
    }
}

// Эпюра, линейная по участку (T, tau): значение в начале и в конце участка.
void drawLinearDiagram(QPainter& p, const QRectF& plot, const Layout& L, const Result& res,
                       const std::function<double(const SectionResult&)>& startValue,
                       const std::function<double(const SectionResult&)>& endValue, double maxAbs,
                       const QColor& color) {
    const double mid = plot.center().y();
    const double pad = L.fontPx * 1.8;
    const double amp = std::max(4.0, plot.height() / 2 - pad);
    const double k = maxAbs > 0.0 ? amp / maxAbs : 0.0;

    for (const SectionResult& s : res.sections) {
        const double x0 = L.X(s.x0), x1 = L.X(s.x1);
        const double v0 = startValue(s), v1 = endValue(s);
        const double y0 = mid - v0 * k, y1 = mid - v1 * k;

        QPolygonF poly;
        poly << QPointF(x0, mid) << QPointF(x0, y0) << QPointF(x1, y1) << QPointF(x1, mid);
        QColor fill = color; fill.setAlpha(36);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPolygon(poly);
        p.setBrush(Qt::NoBrush);

        QColor hatch = color; hatch.setAlpha(110);
        p.setPen(QPen(hatch, 1.0));
        for (double x = x0 + 4; x < x1 - 1; x += 6) {
            const double t = (x - x0) / (x1 - x0);
            p.drawLine(QPointF(x, mid), QPointF(x, y0 + (y1 - y0) * t));
        }

        p.setPen(QPen(color, 2.2));
        p.drawLine(QPointF(x0, y0), QPointF(x1, y1));
        p.drawLine(QPointF(x0, mid), QPointF(x0, y0));
        p.drawLine(QPointF(x1, mid), QPointF(x1, y1));

        const bool constant = std::fabs(v0 - v1) <= 1e-9 * std::fmax(1.0, maxAbs);
        if (constant) {
            drawValueLabel(p, (x0 + x1) / 2, y0, v0 >= 0.0, fmt(v0), color);
        } else {
            drawValueLabel(p, x0 + 3, y0, v0 >= 0.0, fmt(v0), color, Align::Left);
            drawValueLabel(p, x1 - 3, y1, v1 >= 0.0, fmt(v1), color, Align::Right);
        }
    }

    p.setPen(QPen(kInk, 1.4));
    p.drawLine(QPointF(L.xL - 14, mid), QPointF(L.xR + 14, mid));
}

// Эпюра углов поворота: на участке с m != 0 - парабола, иначе - прямая.
void drawPhiDiagram(QPainter& p, const QRectF& plot, const Layout& L, const Result& res,
                    double maxAbs, const QColor& color) {
    const double mid = plot.center().y();
    const double pad = L.fontPx * 1.8;
    const double amp = std::max(4.0, plot.height() / 2 - pad);
    const double k = maxAbs > 0.0 ? amp / maxAbs : 0.0;

    for (const SectionResult& s : res.sections) {
        const double x0 = L.X(s.x0), x1 = L.X(s.x1);
        const double len = s.x1 - s.x0;
        const int steps = (s.phiQuad != 0.0) ? 48 : 1;

        auto yAt = [&](double t) { return mid - phiAt(s, len * t) * k; }; // t от 0 до 1

        QPolygonF poly;
        poly << QPointF(x0, mid);
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            poly << QPointF(x0 + (x1 - x0) * t, yAt(t));
        }
        poly << QPointF(x1, mid);

        QColor fill = color; fill.setAlpha(36);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPolygon(poly);
        p.setBrush(Qt::NoBrush);

        QColor hatch = color; hatch.setAlpha(110);
        p.setPen(QPen(hatch, 1.0));
        for (double x = x0 + 4; x < x1 - 1; x += 6) {
            const double t = (x - x0) / (x1 - x0);
            p.drawLine(QPointF(x, mid), QPointF(x, yAt(t)));
        }

        QPolygonF curve;
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            curve << QPointF(x0 + (x1 - x0) * t, yAt(t));
        }
        p.setPen(QPen(color, 2.2));
        p.drawPolyline(curve);

        // Экстремум внутри участка (вершина параболы).
        if (s.phiQuad != 0.0) {
            const double sv = -s.phiLinear / (2.0 * s.phiQuad);
            if (sv > 0.03 * len && sv < 0.97 * len) {
                const double v = phiAt(s, sv);
                const double xv = x0 + (x1 - x0) * sv / len;
                p.setPen(QPen(color, 1.0));
                p.setBrush(color);
                p.drawEllipse(QPointF(xv, mid - v * k), 2.5, 2.5);
                p.setBrush(Qt::NoBrush);
                drawValueLabel(p, xv, mid - v * k, v >= 0.0, fmt(v), color);
            }
        }
    }

    p.setPen(QPen(kInk, 1.4));
    p.drawLine(QPointF(L.xL - 14, mid), QPointF(L.xR + 14, mid));

    // Значения в узлах.
    const auto& first = res.sections.front();
    drawValueLabel(p, L.X(first.x0), mid - first.phi0 * k, first.phi0 >= 0.0, fmt(first.phi0), color);
    for (const SectionResult& s : res.sections)
        drawValueLabel(p, L.X(s.x1), mid - s.phi1 * k, s.phi1 >= 0.0, fmt(s.phi1), color);
}

void drawPanelTitle(QPainter& p, const QRectF& panel, const Layout& L, const QString& title,
                    const QColor& color) {
    setFontPx(p, L.fontPx, true);
    p.setPen(color);
    p.drawText(QPointF(L.xL - 14, panel.top() + L.fontPx + 1), title);
    setFontPx(p, L.fontPx, false);
}

} // namespace

void draw(QPainter& p, const QRectF& area, const Problem& prob, const Result& res) {
    if (!res.ok || res.sections.empty() || res.totalLength <= 0.0) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    Layout L;
    L.fontPx = std::clamp(area.width() / 75.0, 11.0, 20.0);
    setFontPx(p, L.fontPx);
    const double margin = L.fontPx * 8.5;
    L.xL = area.left() + margin;
    L.xR = area.right() - margin;
    L.total = res.totalLength;

    const double xRow = L.fontPx + 10;
    const double schH = std::clamp(area.height() * 0.28, 120.0, 260.0);
    const double panelH = (area.height() - schH - xRow) / 3.0;

    const QRectF schRect(area.left(), area.top(), area.width(), schH);
    const QRectF panelT(area.left(), schRect.bottom(), area.width(), panelH);
    const QRectF panelTau(area.left(), panelT.bottom(), area.width(), panelH);
    const QRectF panelPhi(area.left(), panelTau.bottom(), area.width(), panelH);

    // Вертикальные линии через узлы (под всеми остальными элементами).
    drawNodeGuides(p, L, res, schRect.bottom() - L.fontPx * 1.8, panelPhi.bottom());

    drawSchematic(p, schRect, L, prob, res);

    auto plotRect = [&](const QRectF& panel) {
        const double top = panel.top() + L.fontPx + 6;
        return QRectF(area.left(), top, area.width(), panel.bottom() - top - 2);
    };

    drawPanelTitle(p, panelT, L, utf8("Эпюра T, Н·м"), kTorqueColor);
    drawLinearDiagram(
        p, plotRect(panelT), L, res, [](const SectionResult& s) { return s.torqueStart; },
        [](const SectionResult& s) { return s.torqueEnd; }, res.maxAbsTorque, kTorqueColor);

    drawPanelTitle(p, panelTau, L, utf8("Эпюра τ, МПа"), kTauColor);
    drawLinearDiagram(
        p, plotRect(panelTau), L, res, [](const SectionResult& s) { return s.tauStart; },
        [](const SectionResult& s) { return s.tauEnd; }, res.maxAbsTau, kTauColor);

    drawPanelTitle(p, panelPhi, L, utf8("Эпюра φ, рад"), kPhiColor);
    drawPhiDiagram(p, plotRect(panelPhi), L, res, res.maxAbsPhi, kPhiColor);

    // Ось x: координаты узлов.
    setFontPx(p, L.fontPx);
    const QFontMetricsF fm(p.font());
    p.setPen(kInk);
    const double yText = panelPhi.bottom() + fm.ascent() + 3;
    double lastRight = -1e9;
    auto nodeLabel = [&](double x) {
        const QString t = fmt(x);
        const double w = fm.horizontalAdvance(t);
        const double cx = L.X(x);
        if (cx - w / 2 > lastRight + 4) {
            p.drawText(QPointF(cx - w / 2, yText), t);
            lastRight = cx + w / 2;
        }
    };
    nodeLabel(0.0);
    for (const SectionResult& s : res.sections) nodeLabel(s.x1);
    p.drawText(QPointF(std::max(L.xR + 14, lastRight + 10), yText), utf8("x, м"));

    p.restore();
}

} // namespace diagram
