// DiagramPainter.h - рисование расчетной схемы и эпюр средствами QPainter.
// Одна и та же функция используется и для окна, и для экспорта в PNG.
#pragma once

#include <QRectF>

#include "TorsionModel.h"

class QPainter;

namespace diagram {

// Рисует в прямоугольнике area: расчетную схему стержня и эпюры
// крутящего момента T, касательных напряжений tau и углов поворота phi.
void draw(QPainter& painter, const QRectF& area, const torsion::Problem& problem,
          const torsion::Result& result);

} // namespace diagram
