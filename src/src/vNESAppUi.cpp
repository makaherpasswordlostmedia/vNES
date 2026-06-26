/*
 * vNESAppUi.cpp — CVNESAppUi, CVNESDocument, CVNESApp
 * Точка входа Symbian-приложения.
 */
#include "vNESAppUi.h"
#include "vNESAppView.h"
#include "NESSystem.h"
#include <avkon.hrh>
#include <aknfileselectdialog.h>
#include <eikenv.h>
#include <f32file.h>

// ---------------------------------------------------------------------------
// CVNESAppUi
// ---------------------------------------------------------------------------
void CVNESAppUi::ConstructL()
{
    BaseConstructL(EAknEnableSkin);

    iNes  = CNESSystem::NewLC(); CleanupStack::Pop();
    iView = CVNESAppView::NewL(ClientRect());

    AddToStackL(iView);
}

CVNESAppUi::~CVNESAppUi()
{
    if (iView)
    {
        RemoveFromStack(iView);
        delete iView;
    }
    delete iNes;
}

void CVNESAppUi::HandleCommandL(TInt aCommand)
{
    switch (aCommand)
    {
    case EAknCmdOpen:
    case EAknSoftkeySelect:
        ShowRomBrowserL();
        break;
    case EAknSoftkeyBack:
    case EEikCmdExit:
        iNes->StopEmulation();
        Exit();
        break;
    default:
        break;
    }
}

void CVNESAppUi::HandleForegroundEventL(TBool aForeground)
{
    CAknAppUi::HandleForegroundEventL(aForeground);
    Globals::focused = aForeground;
    if (aForeground && iNes->IsRunning())
        iView->StartL();
    else
        iView->Stop();
}

void CVNESAppUi::ShowRomBrowserL()
{
    TFileName romPath;
    romPath = _L("C:\\Data\\NES\\");

    // Диалог выбора файла (только .nes)
    CAknFileSelectionDialog* dlg = CAknFileSelectionDialog::NewL(
        ECFDDialogTypeSelect);
    CleanupStack::PushL(dlg);
    dlg->SetTitleL(_L("Select NES ROM"));

    if (dlg->ExecuteL(romPath))
        LoadRomL(romPath);

    CleanupStack::PopAndDestroy(dlg);
}

void CVNESAppUi::LoadRomL(const TDesC& aPath)
{
    iView->Stop();
    iNes->StopEmulation();

    TBool ok = iNes->LoadRomL(aPath);
    if (ok)
    {
        iView->AttachNES(iNes);
        iNes->StartEmulation();
        iView->StartL();
    }
}

// ---------------------------------------------------------------------------
// CVNESDocument
// ---------------------------------------------------------------------------
CVNESDocument* CVNESDocument::NewL(CEikApplication& aApp)
{
    CVNESDocument* self = new (ELeave) CVNESDocument(aApp);
    return self;
}

CEikAppUi* CVNESDocument::CreateAppUiL()
{
    return new (ELeave) CVNESAppUi();
}

// ---------------------------------------------------------------------------
// CVNESApp
// ---------------------------------------------------------------------------
CApaDocument* CVNESApp::CreateDocumentL()
{
    return CVNESDocument::NewL(*this);
}

TUid CVNESApp::AppDllUid() const
{
    return KVNESAppUid;
}

CApaApplication* CVNESApp::NewApplication()
{
    return new CVNESApp();
}

// ---------------------------------------------------------------------------
// DLL entry point (Symbian EXE/DLL)
// ---------------------------------------------------------------------------
#include <eikstart.h>

LOCAL_C CApaApplication* NewApplication()
{
    return CVNESApp::NewApplication();
}

GLDEF_C TInt E32Main()
{
    return EikStart::RunApplication(NewApplication);
}
