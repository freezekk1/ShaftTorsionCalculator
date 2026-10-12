// DiagramPainter.h - рисование расчетной схемы и эпюр средствами QPainter.
#pragma once

#include <QRectF>

#include "TorsionModel.h"

class QPainter;

namespace diagram {

void draw(QPainter& painter, const QRectF& area, const torsion::Problem& problem,
          const torsion::Result& result);

} // namespace diagram
