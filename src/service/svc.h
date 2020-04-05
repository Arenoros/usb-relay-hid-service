#pragma once
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include <stdio.h>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

#include "msg.h"


void SvcInstall(void);
void __stdcall SvcUninstall();
void WINAPI SvcCtrlHandler(DWORD);
void WINAPI SvcMain(DWORD, LPTSTR*);

void ReportSvcStatus(DWORD, DWORD, DWORD);
void SvcInit(DWORD, LPTSTR*);
void SvcReportEvent(LPCTSTR);

//
//void __stdcall DisplayUsage(void);
//
//void __stdcall DoStartSvc(void);
//void __stdcall DoUpdateSvcDacl(void);
//void __stdcall DoStopSvc(void);
//
//BOOL __stdcall StopDependentServices(void);
