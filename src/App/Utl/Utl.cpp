#include "Blinker.h"
#include "VersionStr.h"
#include "Timeline.h"

using namespace std;

#include "StrictMode.h"


void UtlSetupShell()
{
    Timeline::Global().Event("UtlSetupShell");

    Blinker::SetupShell();
    Version::SetupShell();
}

