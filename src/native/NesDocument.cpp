#include "NesDocument.h"
#include "NesAppUi.h"

CNesDocument::CNesDocument(CEikApplication& aApp)
    : CAknDocument(aApp)
{
}

CNesDocument::~CNesDocument()
{
}

CNesDocument* CNesDocument::NewL(CEikApplication& aApp)
{
    CNesDocument* self = new (ELeave) CNesDocument(aApp);
    return self;
}

CEikAppUi* CNesDocument::CreateAppUiL()
{
    return new (ELeave) CNesAppUi;
}
