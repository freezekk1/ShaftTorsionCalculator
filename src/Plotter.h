// Plotter.h - интерфейс рисования эпюр.
#pragma once

#include <string>

#include "TorsionModel.h"

namespace plotter {

bool available();

bool showDiagrams(const torsion::Problem& problem, const torsion::Result& result,
                  std::string& error);

bool saveDiagrams(const torsion::Problem& problem, const torsion::Result& result,
                  const std::string& path, std::string& error);

} // namespace plotter
