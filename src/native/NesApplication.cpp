#include "NesApplication.h"
#include "NesDocument.h"

TUid CNesApplication::AppDllUid() const
{
    return KUidNesApp;
}

CApaDocument* CNesApplication::CreateDocumentL()
{
    return CNesDocument::NewL(*this);
}

// Стандартная фабричная функция, которую ищет E32Dll/EXE entry point
LOCAL_C CApaApplication* NewApplication()
{
    return new CNesApplication;
}

GLDEF_C TInt E32Main()
{
    return EikStart::RunApplication(NewApplication);
}
