/*
 * NesApplication.h — точка входа Symbian-приложения (заменяет main.cpp
 * из Qt-версии). Стандартная тройка Application/Document/AppUi для
 * Avkon-приложений Symbian.
 */
#ifndef NESAPPLICATION_H
#define NESAPPLICATION_H

#include <aknapp.h>

// UID3 приложения — должен совпадать с UID3 в group/vnes.mmp
const TUid KUidNesApp = { 0xE0000002 };

class CNesApplication : public CAknApplication
{
public:
    TUid AppDllUid() const;

protected:
    CApaDocument* CreateDocumentL();
};

#endif // NESAPPLICATION_H
