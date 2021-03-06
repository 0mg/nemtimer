#include <windows.h>
#include <shobjidl.h>

// NONE DEPENDENCY
int popError(HWND hwnd = NULL, UINT mbstyle = MB_OK) {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  DWORD code = GetLastError();
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, str, len, NULL);
  LPTSTR title;
  if (code != NO_ERROR) {
    title = NULL;
    mbstyle |= MB_ICONERROR;
  } else {
    title = TEXT("OK");
    mbstyle |= MB_ICONINFORMATION;
  }
  return MessageBox(hwnd, str, title, mbstyle);
}

void setTBProgress(HWND hwnd, int now, int max) {
  ITaskbarList3* itl;
  CoInitialize(NULL);
  if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&itl)))) {
    itl->SetProgressValue(hwnd, now, max);
    itl->Release();
  }
  CoUninitialize();
}
