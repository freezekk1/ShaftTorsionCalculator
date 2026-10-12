// ConsoleInterface.h - командный интерфейс ввода секций и граничных условий.
#pragma once

#include <iosfwd>
#include <string>

#include "TorsionModel.h"

class ConsoleInterface {
public:
    ConsoleInterface();

    void run(std::istream& in, std::ostream& out, bool showPrompt, bool startWithWizard = false);

    bool runWizard(std::ostream& out);

    bool execute(const std::string& line, std::ostream& out);

    bool loadFile(const std::string& path, std::ostream& out);

    bool calculate(std::ostream& out);

    bool savePng(const std::string& path, std::ostream& out);

    const torsion::Problem& problem() const { return problem_; }

private:
    torsion::Problem problem_;
    std::istream* in_;

    void printHelp(std::ostream& out) const;
    void printProblem(std::ostream& out) const;
    void printResult(const torsion::Result& r, std::ostream& out) const;
    bool saveFile(const std::string& path, std::ostream& out) const;
    void loadExample();
};
