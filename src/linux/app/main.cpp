#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QStyleFactory>
#include <QTimer>

#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("calc-qt"));
    QApplication::setApplicationName(QStringLiteral("calc-qt"));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("calc-qt"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFontDatabase::addApplicationFont(QStringLiteral(":/icons/CalculatorIcons.ttf"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Calculator with the Windows Calculator engine"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption typeOpt(QStringLiteral("type"), QStringLiteral("Type <keys> once the window is up."), QStringLiteral("keys"));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Save the window to <file> and quit."), QStringLiteral("file"));
    parser.addOption(typeOpt);
    parser.addOption(shotOpt);
    parser.process(app);

    MainWindow window;
    window.show();

    if (parser.isSet(typeOpt) || parser.isSet(shotOpt))
    {
        QTimer::singleShot(200, &window, [&] {
            for (QChar ch : parser.value(typeOpt))
            {
                const int key = ch == '\n' ? int(Qt::Key_Return) : int(ch.toUpper().unicode());
                QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, ch == '\n' ? QString() : QString(ch));
                QApplication::sendEvent(&window, &press);
            }
            if (parser.isSet(shotOpt))
            {
                QTimer::singleShot(200, &window, [&] {
                    window.grab().save(parser.value(shotOpt));
                    QApplication::quit();
                });
            }
        });
    }
    return app.exec();
}
