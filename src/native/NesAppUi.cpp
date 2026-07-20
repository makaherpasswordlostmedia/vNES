#include "NesAppUi.h"
#include "NesAppView.h"
#include "NESSystem.h"
#include <avkon.hrh>
#include <eikmenup.h>

void CNesAppUi::ConstructL()
{
    BaseConstructL(EAknEnableSkin);

    // Движок эмулятора — не менялся, используем готовый API NewLC/LoadRomL
    iNesSystem = CNESSystem::NewLC();
    CleanupStack::Pop(iNesSystem);

    // ROM жёстко задан (см. KDefaultRomPath в .h). Если файла нет,
    // LoadRomL уйдёт в Leave — приложение покажет стандартный экран
    // ошибки Symbian вместо аварийного завершения без сообщения.
    iNesSystem->LoadRomL(KDefaultRomPath);
    iNesSystem->StartEmulation();

    iAppView = CNesAppView::NewL(ClientRect(), *iNesSystem);
    AddToStackL(iAppView);
}

CNesAppUi::~CNesAppUi()
{
    if (iAppView)
    {
        RemoveFromStack(iAppView);
        delete iAppView;
    }
    delete iNesSystem;
}

void CNesAppUi::HandleCommandL(TInt aCommand)
{
    switch (aCommand)
    {
    case EAknSoftkeyExit:
    case EEikCmdExit:
        Exit();
        break;
    default:
        break;
    }
}
