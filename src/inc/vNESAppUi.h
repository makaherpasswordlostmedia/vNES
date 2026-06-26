/*
 * vNESAppUi.h — Аналог точки входа (vNES.java / AppletUI.java)
 * Symbian CEikAppUi: управляет загрузкой ROM и жизненным циклом.
 */

#ifndef VNESAPPUI_H
#define VNESAPPUI_H

#include <aknappui.h>
#include "NESTypes.h"

class CVNESAppView;
class CNESSystem;

class CVNESAppUi : public CAknAppUi
{
public:
    void ConstructL();
    ~CVNESAppUi();

    // CEikAppUi
    void HandleCommandL(TInt aCommand) override;
    void HandleForegroundEventL(TBool aForeground) override;

private:
    void LoadRomL(const TDesC& aPath);
    void ShowRomBrowserL();   // выбор ROM через CAknFileSelectionDialog

    CVNESAppView* iView;
    CNESSystem*   iNes;
};

// ---------------------------------------------------------------------------
// vNESApp — CAknApplication
// ---------------------------------------------------------------------------
#include <aknapp.h>

class CVNESApp : public CAknApplication
{
public:
    static CApaApplication* NewApplication();

private:
    CApaDocument* CreateDocumentL() override;
    TUid          AppDllUid() const override;
};

// ---------------------------------------------------------------------------
// vNESDocument — CEikDocument (минимальная реализация)
// ---------------------------------------------------------------------------
#include <akndoc.h>

class CVNESDocument : public CAknDocument
{
public:
    static CVNESDocument* NewL(CEikApplication& aApp);
    CEikAppUi* CreateAppUiL() override;
private:
    CVNESDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
};

// UID приложения (нужно зарегистрировать в .pkg / Symbian Signed)
const TUid KVNESAppUid = { 0xE0000001 };

#endif // VNESAPPUI_H
