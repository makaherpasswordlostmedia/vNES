/*
 * main.cpp — точка входа Qt-приложения для Symbian^3 (Nokia C7)
 *
 * Запуск: приложение открывается, сразу показывает диалог выбора файла,
 * затем запускает эмуляцию.
 */

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include "NESWidget.h"

// Symbian требует TRAP harness вокруг кода, использующего Leave
#ifdef Q_OS_SYMBIAN
#include <e32base.h>
static void runAppL()
{
    QApplication app(qApp->argc(), qApp->argv());

    NESWidget* w = new NESWidget();

    // Диалог выбора ROM
    QString romPath = QFileDialog::getOpenFileName(
        w,
        QString::fromLatin1("Select NES ROM"),
        QString::fromLatin1("C:/Data/NES"),
        QString::fromLatin1("NES ROMs (*.nes);;All files (*)")
    );

    if (romPath.isEmpty())
    {
        delete w;
        return;
    }

    if (!w->loadROM(romPath))
    {
        QMessageBox::critical(w, "vNES", "Failed to load ROM.\nCheck mapper support.");
        delete w;
        return;
    }

    app.exec();
    delete w;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    CTrapCleanup* cleanup = CTrapCleanup::New();
    TRAPD(err, runAppL());
    delete cleanup;
    return err;
}

#else
// Desktop-версия для разработки / отладки на ПК
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    NESWidget* w = new NESWidget();

    QString romPath = QFileDialog::getOpenFileName(
        w, "Select NES ROM", "",
        "NES ROMs (*.nes);;All files (*)");

    if (romPath.isEmpty()) return 0;

    if (!w->loadROM(romPath))
    {
        QMessageBox::critical(w, "vNES", "Failed to load ROM.");
        return 1;
    }

    return app.exec();
}
#endif
