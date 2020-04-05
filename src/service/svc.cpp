#include "svc.h"

#pragma comment(lib, "advapi32.lib")

SERVICE_STATUS gSvcStatus;
SERVICE_STATUS_HANDLE gSvcStatusHandle;
HANDLE ghSvcStopEvent = NULL;

TCHAR szCommand[10];
//TCHAR szSvcName[80];
TCHAR ServiceName[]=  _T("UsbRelayHidService");
SC_HANDLE schSCManager;
SC_HANDLE schService;

void __stdcall DisplayUsage() {
    printf("Usage:\n");
    printf("\tsvc install\n\n");
}

//
// Purpose:
//   Entry point for the process
//
// Parameters:
//   None
//
// Return value:
//   None
//
int __cdecl _tmain(int argc, TCHAR* argv[]) {

    if(argc == 2) {
        StringCchCopy(szCommand, 10, argv[1]);
        if(lstrcmpi(szCommand, TEXT("install")) == 0) {
            SvcInstall();
            return 0;
        }
        if(lstrcmpi(szCommand, TEXT("uninstall")) == 0) {
            SvcUninstall();
            return 0;
        }
        printf("ERROR: Incorrect command\n\n");
        DisplayUsage();
        /* else if(lstrcmpi(szCommand, TEXT("help")) == 0) {
            DisplayUsage();
            return 0;
        }*/
        /*}  else if(argc == 3) {
       StringCchCopy(szCommand, 10, argv[1]);
        StringCchCopy(szSvcName, 80, argv[2]);

        if(lstrcmpi(szCommand, TEXT("start")) == 0)
            DoStartSvc();
        else if(lstrcmpi(szCommand, TEXT("dacl")) == 0)
            DoUpdateSvcDacl();
        else if(lstrcmpi(szCommand, TEXT("stop")) == 0)
            DoStopSvc();
        else {
            _tprintf(TEXT("Unknown command (%s)\n\n"), szCommand);
            DisplayUsage();
        }
        return 0;*/
    } else if (argc > 2){
        printf("ERROR: Incorrect number of arguments\n\n");
        DisplayUsage();
        return 0;
    }

    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] = {{ServiceName, (LPSERVICE_MAIN_FUNCTION)SvcMain}, {NULL, NULL}};

    // This call returns when the service has stopped.
    // The process should simply terminate when the call returns.

    if(!StartServiceCtrlDispatcher(DispatchTable)) { SvcReportEvent(_T("StartServiceCtrlDispatcher")); }
}

//
// Purpose:
//   Installs a service in the SCM database
//
// Parameters:
//   None
//
// Return value:
//   None
//
void SvcInstall() {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];
    HMODULE wtf = nullptr;
    if(!GetModuleFileName(wtf, szPath, MAX_PATH)) {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database.

    schSCManager = OpenSCManager(NULL,                    // local computer
                                 NULL,                    // ServicesActive database
                                 SC_MANAGER_ALL_ACCESS);  // full access rights

    if(NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Create the service

    schService = CreateService(schSCManager,               // SCM database
                               ServiceName,                // name of service
                               ServiceName,                // service name to display
                               SERVICE_ALL_ACCESS,         // desired access
                               SERVICE_WIN32_OWN_PROCESS,  // service type
                               SERVICE_DEMAND_START,       // start type
                               SERVICE_ERROR_NORMAL,       // error control type
                               szPath,                     // path to service's binary
                               NULL,                       // no load ordering group
                               NULL,                       // no tag identifier
                               NULL,                       // no dependencies
                               NULL,                       // LocalSystem account
                               NULL);                      // no password

    if(schService == NULL) {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    } else
        printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}
//
// Purpose:
//   Deletes a service from the SCM database
//
// Parameters:
//   None
//
// Return value:
//   None
//
void __stdcall SvcUninstall() {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS ssStatus;

    // Get a handle to the SCM database.

    schSCManager = OpenSCManager(NULL,                    // local computer
                                 NULL,                    // ServicesActive database
                                 SC_MANAGER_ALL_ACCESS);  // full access rights

    if(NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the service.

    schService = OpenService(schSCManager,  // SCM database
                             ServiceName,     // name of service
                             DELETE);       // need delete access

    if(schService == NULL) {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    // Delete the service.

    if(!DeleteService(schService)) {
        printf("DeleteService failed (%d)\n", GetLastError());
    } else
        printf("Service deleted successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose:
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
//
// Return value:
//   None.
//
void WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv) {
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler(ServiceName, SvcCtrlHandler);

    if(!gSvcStatusHandle) {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}

//
// Purpose:
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
//
// Return value:
//   None
//
void SvcInit(DWORD dwArgc, LPTSTR* lpszArgv) {
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEvent(NULL,   // default security attributes
                                 TRUE,   // manual reset event
                                 FALSE,  // not signaled
                                 NULL);  // no name

    if(ghSvcStopEvent == NULL) {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.

    while(1) {
        // Check whether to stop the service.

        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
}

//
// Purpose:
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation,
//     in milliseconds
//
// Return value:
//   None
//
void ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if(dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else
        gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else
        gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

//
// Purpose:
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
//
// Return value:
//   None
//
void WINAPI SvcCtrlHandler(DWORD dwCtrl) {
    // Handle the requested control code.

    switch(dwCtrl) {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }
}

//
// Purpose:
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
//
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
void SvcReportEvent(LPCTSTR szFunction) {
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, ServiceName);

    if(NULL != hEventSource) {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = ServiceName;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,         // event log handle
                    EVENTLOG_ERROR_TYPE,  // event type
                    0,                    // event category
                    SVC_ERROR,            // event identifier
                    NULL,                 // no security identifier
                    2,                    // size of lpszStrings array
                    0,                    // no binary data
                    lpszStrings,          // array of strings
                    NULL);                // no binary data

        DeregisterEventSource(hEventSource);
    }
}
