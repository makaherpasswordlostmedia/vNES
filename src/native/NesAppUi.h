/*
 * NesAppUi.h — CAknAppUi. Владеет экземпляром CNESSystem (движок
 * эмулятора, без изменений) и главным View (NesAppView, отрисовка +
 * ввод). Заменяет NESWidget/main.cpp из Qt-версии.
 *
 * ROM жёстко задан по пути C:\Data\NES\game.nes для первой версии —
 * без файлового диалога (сложнее, планируется отдельно).
 */
#ifndef NESAPPUI_H
#define NESAPPUI_H

#include <aknappui.h>

class CNesAppView;
class CNESSystem;

_LIT(KDefaultRomPath, "C:\\Data\\NES\\game.nes");

class CNesAppUi : public CAknAppUi
{
public:
    void ConstructL();
    ~CNesAppUi();

    CNESSystem* NesSystem() const { return iNesSystem; }

private:
    void HandleCommandL(TInt aCommand);

private:
    CNesAppView* iAppView;
    CNESSystem*  iNesSystem;
};

#endif // NESAPPUI_H
