#include <windows.h>
#include "random.h"
#include "mydll.h"

EXPORT
BOOL WINAPI
DllMain (HANDLE hDll, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            /* Code to run when the DLL is loaded */
            random_init();
            break;

        case DLL_PROCESS_DETACH:
            /* Code to run when the DLL is freed */
            random_final();
            break;

        case DLL_THREAD_ATTACH:
            /* Code to run when a thread is created during the DLL's lifetime */
            break;

        case DLL_THREAD_DETACH:
            /* Code to run when a thread ends normally. */
            break;
    }
    return TRUE;
}
