// Заглушка для сборки без Qt: консольная часть работает, эпюры не рисуются.
#include "Plotter.h"

namespace plotter {

namespace {
const char* kNoQt =
    "Программа собрана без Qt, рисование эпюр недоступно. "
    "Установите Qt 6 (или Qt 5) и пересоберите проект (см. README).";
}

bool available() { return false; }

bool showDiagrams(const torsion::Problem&, const torsion::Result&, std::string& error) {
    error = kNoQt;
    return false;
}

bool saveDiagrams(const torsion::Problem&, const torsion::Result&, const std::string&,
                  std::string& error) {
    error = kNoQt;
    return false;
}

} // namespace plotter
