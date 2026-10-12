// Реализация Plotter на Qt: эпюры рисуются через QPainter
// (в окне - QWidget::paintEvent, в файле - QPainter на QImage).
#include "Plotter.h"

#include <QApplication>
#include <QEventLoop>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "DiagramPainter.h"

namespace plotter {

namespace {

int gArgc = 1;
char gArg0[] = "ShaftTorsionCalculator";
char* gArgv[] = {gArg0, nullptr};

bool hasDisplay() {
#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
#if !defined(Q_OS_MACOS)
    return qEnvironmentVariableIsSet("DISPLAY") || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
    return true;
#endif
#else
    return true;
#endif
}

// QApplication создается один раз при первом обращении.
bool ensureApp(bool needWindow, std::string& error) {
    if (QCoreApplication::instance()) return true;
    if (!hasDisplay()) {
        if (needWindow) {
            error = "Графическая оболочка не найдена (нет DISPLAY). Используйте png <файл>.";
            return false;
        }
        // Сохранение в файл возможно и без экрана.
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    static QApplication app(gArgc, gArgv);
    return true;
}

constexpr int kImageWidth = 1500;
constexpr int kImageHeight = 1200;

bool renderToFile(const torsion::Problem& problem, const torsion::Result& result,
                  const QString& path) {
    QImage image(kImageWidth, kImageHeight, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    diagram::draw(painter, QRectF(10, 10, kImageWidth - 20, kImageHeight - 20), problem, result);
    painter.end();
    return image.save(path);
}

// Виджет, который рисует эпюры в paintEvent.
class DiagramWidget : public QWidget {
public:
    DiagramWidget(const torsion::Problem& p, const torsion::Result& r, QWidget* parent = nullptr)
        : QWidget(parent), problem_(p), result_(r) {
        setMinimumSize(800, 720);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        diagram::draw(painter, QRectF(rect()).adjusted(8, 8, -8, -8), problem_, result_);
    }

private:
    torsion::Problem problem_;
    torsion::Result result_;
};

} // namespace

bool available() { return true; }

bool showDiagrams(const torsion::Problem& problem, const torsion::Result& result,
                  std::string& error) {
    if (!ensureApp(true, error)) return false;

    auto* window = new QWidget;
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(QString::fromUtf8("Эпюры - ShaftTorsionCalculator"));

    auto* layout = new QVBoxLayout(window);
    layout->addWidget(new DiagramWidget(problem, result, window), 1);

    auto* saveButton = new QPushButton(QString::fromUtf8("Сохранить в PNG..."), window);
    layout->addWidget(saveButton, 0, Qt::AlignRight);
    QObject::connect(saveButton, &QPushButton::clicked, window, [window, problem, result]() {
        const QString path = QFileDialog::getSaveFileName(
            window, QString::fromUtf8("Сохранить эпюры"), QStringLiteral("epures.png"),
            QStringLiteral("PNG (*.png)"));
        if (!path.isEmpty()) renderToFile(problem, result, path);
    });

    // Ждем закрытия окна и возвращаемся в консоль.
    QEventLoop loop;
    QObject::connect(window, &QObject::destroyed, &loop, &QEventLoop::quit);
    window->resize(1000, 860);
    window->show();
    loop.exec();
    return true;
}

bool saveDiagrams(const torsion::Problem& problem, const torsion::Result& result,
                  const std::string& path, std::string& error) {
    if (!ensureApp(false, error)) return false;
    if (!renderToFile(problem, result, QString::fromStdString(path))) {
        error = "Не удалось записать файл " + path;
        return false;
    }
    return true;
}

} // namespace plotter
