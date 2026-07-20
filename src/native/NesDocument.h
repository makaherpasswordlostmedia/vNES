/*
 * NesDocument.h — CApaDocument. Хранит модель (в нашем случае модель —
 * это сам CNESSystem, живущий в AppUi) и создаёт AppUi.
 */
#ifndef NESDOCUMENT_H
#define NESDOCUMENT_H

#include <akndoc.h>

class CEikAppUi;

class CNesDocument : public CAknDocument
{
public:
    static CNesDocument* NewL(CEikApplication& aApp);
    ~CNesDocument();

private:
    CNesDocument(CEikApplication& aApp);
    CEikAppUi* CreateAppUiL();
};

#endif // NESDOCUMENT_H
