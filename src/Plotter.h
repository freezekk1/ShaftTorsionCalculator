// Plotter.h - интерфейс рисования эпюр.
// Реализация зависит от наличия Qt: PlotterQt.cpp (QPainter) или PlotterStub.cpp (без Qt).
#pragma once

#include <string>

#include "TorsionModel.h"

namespace plotter {

// true, если программа собрана с Qt и умеет рисовать эпюры.
bool available();

// Открывает окно с расчетной схемой и эпюрами T, tau, phi. Блокирует выполнение,
// пока окно не закрыто. При неудаче возвращает false и заполняет error.
bool showDiagrams(const torsion::Problem& problem, const torsion::Result& result,
                  std::string& error);

// Рисует те же эпюры в PNG-файл (QPainter на QImage).
bool saveDiagrams(const torsion::Problem& problem, const torsion::Result& result,
                  const std::string& path, std::string& error);

} // namespace plotter
