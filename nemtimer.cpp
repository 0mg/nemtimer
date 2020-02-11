#include "nemtimer.h"
#include "kbd.h"
#include "util.h"

#define MS 1000
#define ATIMEOUT_ALT 60
#define ATIMEOUT_DEFAULT 7200
#define WTIMER_ID 1
#define WTIMER_OUT 200
#define WTIMER_LITE_ID 2
#define WTIMER_LITE_OUT 60000
#define OVERTIME -3000
#define CMDTOLANG(cmd) ((cmd & 0xF) << 12)
#define LANGTOCMD(lang) (0xFF0 | (lang >> 12))
#define LANGTOI(lang) (lang >> 12)
#define APPEND(dst, src) SendMessage((dst), CB_ADDSTRING, 0, (LPARAM)(src))

static WORD langtype = C_LANG_DEFAULT;
static BOOL atimeover = FALSE;
static BOOL bootrun = FALSE;

// # cmd options
#define TASK_SLEEP 0
#define TASK_HIBER 1
#define TASK_DISPOFF 2
#define AWAKE_SYS ES_SYSTEM_REQUIRED //1
#define AWAKE_DISP ES_DISPLAY_REQUIRED //2
static int task = TASK_SLEEP;
static int awaken = AWAKE_SYS;
static BOOL deepsleep = FALSE;
static BOOL debugMode = FALSE;
static BOOL liteMode = FALSE;
static BOOL langset = FALSE;

static struct {
  int fixed = ATIMEOUT_DEFAULT;
  int out = fixed * MS;
  int rest = out;
  TCHAR text[99];
} atimer;

static struct {
  LPTSTR
  task[3] = {NULL, L"/h", L"/d"},
  awaken[4] = {NULL, L"/as", L"/ad", L"/a"},
  deep[2] = {NULL, L"/deep"},
  debug[2] = {NULL, L"/debug"},
  lite[2] = {NULL, L"/lite"},
  lang[2] = {L"/en", L"/ja"};
} cmddef;

void __start__(void) {ExitProcess(WinMain(GetModuleHandle(NULL), 0, NULL, 0));}

LPTSTR secToString(LPTSTR buf, int sec) {
  if (sec >= 3600) {
    wsprintf(buf, TEXT("%d:%02d:%02d"), sec / 3600, (sec % 3600) / 60, sec % 60);
  } else if (sec >= 60) {
    wsprintf(buf, TEXT("%d:%02d"), (sec % 3600) / 60, sec % 60);
  } else {
    wsprintf(buf, TEXT("%d"), sec);
  }
  return buf;
}

LPTSTR makeLinkName(LPTSTR buf) {
  getLocaleString(buf, C_STR_TASKFIRST + task, langtype);
  lstrcat(buf, L" ");
  int sec = atimer.out / MS;
  int H = sec / 3600, M = (sec % 3600) / 60, S = sec % 60;
  TCHAR tmp[99];
  if (H) wsprintf(tmp, L"%dh", H), lstrcat(buf, tmp);
  if (M) wsprintf(tmp, L"%dm", M), lstrcat(buf, tmp);
  if (S) wsprintf(tmp, L"%ds", S), lstrcat(buf, tmp);
  return buf;
}

LPTSTR makeCmdOptionString(LPTSTR buf, SIZE_T sz) {
  SecureZeroMemory(buf, sz);
  if (langset) {
    lstrcat(buf, cmddef.lang[LANGTOI(langtype)]);
    lstrcat(buf, L" ");
  }
  if (cmddef.task[task]) {
    lstrcat(buf, cmddef.task[task]); // task = 0,1,2
    lstrcat(buf, L" ");
  }
  if (cmddef.awaken[awaken]) {
    lstrcat(buf, cmddef.awaken[awaken]); // awaken = 0,1,2,3
    lstrcat(buf, L" ");
  }
  if (cmddef.deep[deepsleep]) {
    lstrcat(buf, cmddef.deep[deepsleep]); // deepsleep = 0,1
    lstrcat(buf, L" ");
  }
  TCHAR timestr[99];
  secToString(timestr, atimer.out / MS);
  lstrcat(buf, timestr);
  return buf;
}

int parseTimeString(LPTSTR ts) {
  BOOL correct = FALSE;
  int a = 0, b = 0, c = 0, *p = &a, time = -1, ms = 1000;
  while (*ts) { // alt of swscanf(ts,"%d:%d:%d",&a,&b,&c) in msvcrt.dll
    if (*ts >= '0' && *ts <= '9') {
      correct = TRUE;
      *p = (*p * 10) + (*ts - '0');
    } else if (*ts == ':') { // hh:mm:ss OR mm:ss OR ss | C:\log.txt
      p = p == &a ? &b : &c;
    } else if (*ts >= '0') { // 'C' = 67 | C:\log.txt
      correct = TRUE;
      *p = (*p * 10) + (*ts - '0');
    }
    ts++;
  }
  // every `time` can be less than zero
  if (!correct) {
    time = -1;
  } else if (p == &c) { // hh:mm:ss
    time = (a * 3600 + b * 60 + c) * ms;
  } else if (p == &b) { // mm:ss
    time = (a * 60 + b) * ms;
  } else if (p == &a) { // ss
    time = a * ms;
  } else {
    time = -1;
  }
  return time;
}

HACCEL registerHotkeys() {
  Hotkey.assign(C_CMD_EXIT, VK_ESCAPE, 0);
  Hotkey.assign(C_CMD_DISPOFF, 'B', C_KBD_CTRL);
  Hotkey.assign(C_CMD_NEW, 'N', C_KBD_CTRL);
  Hotkey.assign(C_CMD_SAVEAS, 'E', C_KBD_CTRL);
  Hotkey.assign(C_CMD_AWAKEN, 'A', C_KBD_CTRL);
  return CreateAcceleratorTable(Hotkey.hotkeylist, Hotkey.entries);
}

void modifyMenu(HMENU menu, WORD lang) {
  setMenuText(menu, C_STR_FILE, lang, 1);
  setMenuText(menu, C_STR_CTRL, lang, 2);
  setMenuText(menu, C_STR_TOOL, lang, 3);
  setMenuText(menu, C_STR_HELP, lang, 4);
  setMenuText(menu, C_CMD_EXIT, lang);
  setMenuText(menu, C_CMD_NEW, lang);
  setMenuText(menu, C_CMD_STOP, lang);
  setMenuText(menu, C_CMD_DISPOFF, lang);
  setMenuText(menu, C_CMD_AWAKEN, lang);
  setMenuText(menu, C_CMD_NEW, lang);
  setMenuText(menu, C_CMD_SAVEAS, lang);
  setMenuText(menu, C_CMD_ABOUT, lang);
  setMenuText(menu, C_CMD_MANUAL, lang);
}

void modifyCtrls(HWND hwnd, WORD lang) {
  WORD list[] = {IDOK, IDCANCEL, C_CMD_START, ID_BTN_NEW, ID_BTN_STOP, ID_GP_MAIN, ID_DESC_TIME, ID_CAP_TIME, ID_DESC_TASK, ID_CAP_TASK, ID_GP_AWAKE, ID_DESC_AWAKE, ID_CAP_AWDISP, ID_CAP_AWSYS, ID_DESC_AWSYS, ID_CHK_TASK, 0}, *ids = list;
  TCHAR s[C_MAX_MSGTEXT];
  while (*ids) {
    if (getLocaleString(s, *ids, langtype)) SetDlgItemText(hwnd, *ids, s);
    ids++;
  }
  if (getLocaleString(s, ID_ICO_TASK + task, langtype)) SetDlgItemText(hwnd, ID_ICO_TASK, s);
}

BOOL CALLBACK optProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static HWND times, tasks, awdisp, awsys, deep;
  switch (msg) {
  case WM_INITDIALOG: {
    TCHAR s[C_MAX_MSGTEXT];
    times = GetDlgItem(hwnd, ID_EDIT_TIME);
    tasks = GetDlgItem(hwnd, ID_EDIT_TASK);
    awdisp = GetDlgItem(hwnd, ID_EDIT_AWDISP);
    awsys = GetDlgItem(hwnd, ID_EDIT_AWSYS);
    deep = GetDlgItem(hwnd, ID_CHK_TASK);
    APPEND(times, L"3:00");
    APPEND(times, L"5:00");
    APPEND(times, L"30:00");
    APPEND(times, L"1:00:00");
    APPEND(times, L"1:30:00");
    APPEND(times, L"2:00:00");
    APPEND(times, L"2:30:00");
    APPEND(times, L"3:00:00");
    APPEND(tasks, getLocaleString(s, C_STR_SLEEP, langtype));
    APPEND(tasks, getLocaleString(s, C_STR_HIBER, langtype));
    APPEND(tasks, getLocaleString(s, C_STR_DISPOFF, langtype));
    APPEND(awdisp, getLocaleString(s, C_STR_AUTO, langtype));
    APPEND(awdisp, getLocaleString(s, C_STR_SUPRESS, langtype));
    APPEND(awsys, getLocaleString(s, C_STR_AUTO, langtype));
    APPEND(awsys, getLocaleString(s, C_STR_SUPRESS, langtype));
    SetDlgItemText(hwnd, ID_EDIT_TIME, atimer.text);
    SendMessage(tasks, CB_SETCURSEL, task, 0);
    SendMessage(awdisp, CB_SETCURSEL, awaken & AWAKE_DISP ? 1 : 0, 0);
    SendMessage(awsys, CB_SETCURSEL, awaken & AWAKE_SYS ? 1 : 0, 0);
    SendMessage(deep, BM_SETCHECK, deepsleep, 0);
    EnableWindow(GetDlgItem(hwnd, ID_DESC_AWSYS), SendMessage(awsys, CB_GETCURSEL, 0, 0) == 0);
    modifyCtrls(hwnd, langtype);
    if (getLocaleString(s, ID_DLG_OPTION, langtype)) SetWindowText(hwnd, s);
    return 1;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case ID_EDIT_AWSYS: {
      if (HIWORD(wp) == CBN_SELCHANGE) {
        EnableWindow(GetDlgItem(hwnd, ID_DESC_AWSYS), SendMessage(awsys, CB_GETCURSEL, 0, 0) == 0);
      }
      break;
    }
    case C_CMD_START: {
      TCHAR timestr[20];
      GetDlgItemText(hwnd, ID_EDIT_TIME, timestr, 20);
      int time = parseTimeString(timestr);
      atimer.out = time >= 0 ? time : ATIMEOUT_DEFAULT * MS;
      atimer.rest = atimer.out;
      task = SendMessage(tasks, CB_GETCURSEL, 0, 0);
      awaken = (SendMessage(awdisp, CB_GETCURSEL, 0, 0) == 1 ? AWAKE_DISP : 0) |
               (SendMessage(awsys, CB_GETCURSEL, 0, 0) == 1 ? AWAKE_SYS : 0);
      deepsleep = SendMessage(deep, BM_GETCHECK, 0, 0);
      EndDialog(hwnd, IDOK);
      break;
    }
    case IDCANCEL: EndDialog(hwnd, IDCANCEL); break;
    }
    return 1;
  }
  case WM_NOTIFY: {
    if (wp == ID_SPIN_TIME) {
      TCHAR timestr[20];
      GetDlgItemText(hwnd, ID_EDIT_TIME, timestr, 20);
      int sec = parseTimeString(timestr) / MS;
      int delta = (60 * 5) * -(((NMUPDOWN *)lp)->iDelta);
      sec += delta;
      if (sec < 0) sec = 0;
      secToString(timestr, sec);
      SetDlgItemText(hwnd, ID_EDIT_TIME, timestr);
    }
    return 1;
  }
  }
  return 0;
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static ULONGLONG stime = 0;
  static BOOL counting = FALSE;
  static HWND probar, timeview, taskicon, dispicon, sysicon, exticon;
  static HMENU menubar;
  static HFONT timefont, iconfont;
  // # count down
  if (counting) {
    atimer.rest = atimer.out - (GetTickCount64() - stime);
    // # on time over
    if (atimer.rest <= 0) {
      if (debugMode) {
        TCHAR s[99];
        wsprintf(s, TEXT("timer rest: %d;\n"), atimer.rest);
        OutputDebugString(s);
      }
      if (atimer.rest < OVERTIME) {
        // if timeout after [suspend PC by other app or sys] or freeze
        atimer.out = ATIMEOUT_ALT * MS;
        counting = FALSE;
        PostMessage(hwnd, WM_COMMAND, C_CMD_START, 0);
      } else {
        counting = FALSE;
        atimeover = TRUE;
        KillTimer(hwnd, WTIMER_ID);
        KillTimer(hwnd, WTIMER_LITE_ID);
        atimer.rest = 0;
        ShowWindow(hwnd, FALSE);
        PostMessage(hwnd, WM_CLOSE, 0, 0);
      }
    }
  }
  atimer.fixed = (atimer.rest + 999) / MS;
  // # message handler
  switch (msg) {
  case WM_CREATE: {
    menubar = GetMenu(hwnd);
    timefont = CreateFont(75, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, FIXED_PITCH, L"MS Shell Dlg");
    iconfont = CreateFont(28, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, FIXED_PITCH, L"MS Shell Dlg");
    return 0;
  }
  case WM_INITDIALOG: {
    // # Init vars
    probar = GetDlgItem(hwnd, ID_PB_MAIN);
    timeview = GetDlgItem(hwnd, ID_TXT_TIMER);
    taskicon = GetDlgItem(hwnd, ID_ICO_TASK);
    dispicon = GetDlgItem(hwnd, ID_ICO_AWDISP);
    sysicon = GetDlgItem(hwnd, ID_ICO_AWSYS);
    exticon = GetDlgItem(hwnd, ID_ICO_EXTRA);
    // # Init dialog's layout
    SendMessage(timeview, WM_SETFONT, (WPARAM)timefont, TRUE);
    SendMessage(taskicon, WM_SETFONT, (WPARAM)iconfont, TRUE);
    SendMessage(dispicon, WM_SETFONT, (WPARAM)iconfont, TRUE);
    SendMessage(sysicon, WM_SETFONT, (WPARAM)iconfont, TRUE);
    SendMessage(exticon, WM_SETFONT, (WPARAM)iconfont, TRUE);
    ShowWindow(exticon, debugMode);
    // # Reset menu & dialog
    SendMessage(hwnd, WM_COMMAND, LANGTOCMD(langtype), 1);
    // # Boot
    if (bootrun) {
      SendMessage(hwnd, WM_COMMAND, C_CMD_START, 0);
    } else {
      SendMessage(hwnd, WM_COMMAND, C_CMD_STOP, 0);
    }
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case ID_BTN_NEW: //flow to follow
    case C_CMD_NEW: {
      if (counting) break;
      if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DLG_OPTION), hwnd, optProc) == IDOK) {
        SendMessage(hwnd, WM_COMMAND, C_CMD_START, 0);
      }
      break;
    }
    case C_CMD_START: {
      if (counting) break;
      // # main
      KillTimer(hwnd, WTIMER_ID);
      KillTimer(hwnd, WTIMER_LITE_ID);
      SetTimer(hwnd, WTIMER_ID, WTIMER_OUT, NULL);
      if (liteMode) SetTimer(hwnd, WTIMER_LITE_ID, WTIMER_LITE_OUT, NULL);
      SetThreadExecutionState(
        (awaken & ES_DISPLAY_REQUIRED) |
        (awaken & ES_SYSTEM_REQUIRED) |
        ES_CONTINUOUS);
      stime = GetTickCount64();
      SendMessage(probar, PBM_SETRANGE32, 0, atimer.out);
      // # display flags
      TCHAR s[C_MAX_MSGTEXT];
      if (getLocaleString(s, ID_ICO_TASK + task, langtype)) SetWindowText(taskicon, s);
      ShowWindow(dispicon, !!(awaken & AWAKE_DISP));
      ShowWindow(sysicon, !!(awaken & AWAKE_SYS));
      ShowWindow(exticon, debugMode);
      // # visual style
      EnableWindow(timeview, TRUE);
      EnableWindow(taskicon, TRUE);
      EnableWindow(dispicon, TRUE);
      EnableWindow(sysicon, TRUE);
      EnableWindow(exticon, TRUE);
      EnableWindow(GetDlgItem(hwnd, ID_BTN_NEW), FALSE);
      EnableWindow(GetDlgItem(hwnd, ID_BTN_STOP), TRUE);
      // # supplement
      SetFocus((HWND)GetDlgItem(hwnd, ID_BTN_STOP));
      SendMessage(hwnd, WM_TIMER, 0, 0); // redraw timertext
      // set taskbar progress on bootrun
      PostMessage(hwnd, WM_TIMER, 0, 0);
      // # main
      counting = TRUE; // After redraw timertext
      break;
    }
    case ID_BTN_STOP: //flow to follow
    case IDOK: //flow to follow
    case C_CMD_STOP: {
      // # main
      counting = FALSE;
      KillTimer(hwnd, WTIMER_ID);
      KillTimer(hwnd, WTIMER_LITE_ID);
      SetThreadExecutionState(ES_CONTINUOUS);
      SendMessage(probar, PBM_SETPOS, 0, 0);
      setTBProgress(hwnd, 0, 0);
      SetWindowText(hwnd, C_VAL_APPNAME);
      // # visual style
      EnableWindow(timeview, FALSE);
      EnableWindow(taskicon, FALSE);
      EnableWindow(dispicon, FALSE);
      EnableWindow(sysicon, FALSE);
      EnableWindow(exticon, FALSE);
      EnableWindow(GetDlgItem(hwnd, ID_BTN_NEW), TRUE);
      EnableWindow(GetDlgItem(hwnd, ID_BTN_STOP), FALSE);
      // # supplement
      SetFocus((HWND)GetDlgItem(hwnd, ID_BTN_NEW));
      break;
    }
    case C_CMD_ABOUT: {
      TCHAR v[C_MAX_MSGTEXT], t[C_MAX_MSGTEXT];
      wsprintf(v, C_VAL_APPNAME L" version %d.%d", C_VAL_APPVER);
      MSGBOXPARAMS mbpa;
      SecureZeroMemory(&mbpa, sizeof(MSGBOXPARAMS));
      mbpa.cbSize = sizeof(MSGBOXPARAMS);
      mbpa.hwndOwner = hwnd;
      mbpa.hInstance = GetModuleHandle(NULL);
      mbpa.lpszText = v;
      mbpa.lpszCaption = getLocaleString(t, C_STR_ABOUT_CAP, langtype);
      mbpa.dwStyle = MB_USERICON;
      mbpa.lpszIcon = MAKEINTRESOURCE(ID_ICO_EXE);
      MessageBoxIndirect(&mbpa);
      break;
    }
    case C_CMD_SAVEAS: {
      const SIZE_T pathmax = 0x1000;
      WCHAR pathname[pathmax], exepath[pathmax], argstr[pathmax];
      GetModuleFileName(NULL, exepath, sizeof(exepath));
      makeLinkName(pathname);
      makeCmdOptionString(argstr, sizeof(argstr));
      OPENFILENAMEW ofn;
      SecureZeroMemory(&ofn, sizeof(OPENFILENAME));
      ofn.lStructSize = sizeof(OPENFILENAME);
      ofn.hwndOwner = hwnd;
      ofn.lpstrFilter = L"*.lnk\0*.lnk\0" L"*.*\0*.*\0\0";
      ofn.lpstrFile = pathname;
      ofn.nMaxFile = pathmax;
      ofn.lpstrDefExt = L"lnk";
      ofn.Flags = OFN_OVERWRITEPROMPT;
      if (GetSaveFileNameW(&ofn)) {
        IShellLinkW *isl;
        IPersistFile *ips;
        CoInitialize(NULL);
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&isl)))) {
          isl->SetPath(exepath);
          isl->SetArguments(argstr);
          if (SUCCEEDED(isl->QueryInterface(IID_PPV_ARGS(&ips)))) {
            if (FAILED(ips->Save(pathname, TRUE))) popError(hwnd);
            ips->Release();
          } else popError(hwnd);
          isl->Release();
        } else popError(hwnd);
        CoUninitialize();
      }
      break;
    }
    case IDCANCEL: //flow to follow
    case C_CMD_EXIT: DestroyWindow(hwnd); break;
    case C_CMD_DISPOFF: {
      SendNotifyMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
      break;
    }
    case C_CMD_LANG_JA: //flow to follow
    case C_CMD_LANG_DEFAULT: {
      HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_MNU_MAIN));
      langtype = CMDTOLANG(LOWORD(wp));
      modifyMenu(menu, langtype);
      DestroyMenu(menubar);
      SetMenu(hwnd, menubar = menu);
      modifyCtrls(hwnd, langtype);
      // for command option
      if (!(HIWORD(wp) == 0 && lp != 0)) langset = TRUE;
      break;
    }
    }
    return 0;
  }
  case WM_SIZE: {
    if (counting && liteMode) {
      wp = 0;
      // goto WM_TIMER
    } else return 0;
  }
  case WM_TIMER: {
    if (liteMode) {
      if (wp == 0 || wp == WTIMER_LITE_ID) {
        // draw
      } else return 0;
    }
    // # draw timer
    SendMessage(probar, PBM_SETPOS, atimer.rest, 0);
    setTBProgress(hwnd, atimer.rest, atimer.out);
    secToString(atimer.text, atimer.fixed);
    SetDlgItemText(hwnd, ID_TXT_TIMER, atimer.text);
    TCHAR s[C_MAX_MSGTEXT];
    wsprintf(s, L"%s " C_VAL_APPNAME, atimer.text);
    SetWindowText(hwnd, s);
    return 0;
  }
  case WM_INITMENU: {
    EnableMenuItem(menubar, C_CMD_NEW, counting * MF_GRAYED);
    EnableMenuItem(menubar, C_CMD_STOP, !counting * MF_GRAYED);
    return 0;
  }
  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cl, int cs) {
  // command line option
  {
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    LPWSTR *a = argv, ts = NULL;
    if (argc > 1) {
      task = 0;
      awaken = 0;
      deepsleep = 0;
      debugMode = 0;
      liteMode = 0;
    }
    for (int i = 1; i < argc; i++) {
      a = argv + i;
      if (**a != '/') ts = *a, bootrun = TRUE;
      else if (lstrcmp(*a, cmddef.task[TASK_HIBER]) == 0) task = TASK_HIBER;
      else if (lstrcmp(*a, cmddef.task[TASK_DISPOFF]) == 0) task = TASK_DISPOFF;
      else if (lstrcmp(*a, cmddef.awaken[AWAKE_SYS]) == 0) awaken |= AWAKE_SYS;
      else if (lstrcmp(*a, cmddef.awaken[AWAKE_DISP]) == 0) awaken |= AWAKE_DISP;
      else if (lstrcmp(*a, cmddef.awaken[AWAKE_SYS | AWAKE_DISP]) == 0) awaken = AWAKE_SYS | AWAKE_DISP;
      else if (lstrcmp(*a, cmddef.deep[TRUE]) == 0) deepsleep = TRUE;
      else if (lstrcmp(*a, cmddef.debug[TRUE]) == 0) debugMode = TRUE;
      else if (lstrcmp(*a, cmddef.lite[TRUE]) == 0) liteMode = TRUE;
      // # lang
      else if (lstrcmp(*a, cmddef.lang[LANGTOI(C_LANG_DEFAULT)]) == 0) {
        langtype = C_LANG_DEFAULT, langset = TRUE;
      } else if (lstrcmp(*a, cmddef.lang[LANGTOI(C_LANG_JA)]) == 0) {
        langtype = C_LANG_JA, langset = TRUE;
      }
    }
    if (ts) {
      int time = parseTimeString(ts);
      atimer.rest = atimer.out = time >= 0 ? time : ATIMEOUT_DEFAULT * MS;
      atimer.fixed = atimer.out / MS;
    }
    LocalFree(argv);
  }
  // Init vars
  secToString(atimer.text, atimer.fixed);
  // MENU & LANG & HOTKEY
  HACCEL haccel = registerHotkeys();
  if (!langset) switch (LANGIDFROMLCID(GetUserDefaultLCID())) {
  case 0x0411: langtype = C_LANG_JA; break;
  default: langtype = C_LANG_DEFAULT; break;
  }
  // Main Window: Settings
  WNDCLASSEX wc;
  SecureZeroMemory(&wc, sizeof(WNDCLASSEX));
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.cbWndExtra = DLGWINDOWEXTRA;
  wc.lpfnWndProc = mainWndProc;
  wc.hInstance = hi;
  wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hIcon = (HICON)LoadImage(hi, MAKEINTRESOURCE(ID_ICO_EXE), IMAGE_ICON, 0, 0, 0);
  wc.hIconSm = (HICON)LoadImage(hi, MAKEINTRESOURCE(ID_ICO_EXE), IMAGE_ICON, 0, 0, 0);
  wc.lpszClassName = C_VAL_APPNAME;
  wc.hbrBackground = (HBRUSH)(COLOR_MENU + 1);
  RegisterClassEx(&wc);

  // Main Window: Create, Show
  HWND hwnd = CreateDialog(hi, MAKEINTRESOURCE(ID_DLG_MAIN), 0, (DLGPROC)wc.lpfnWndProc);
  // WinMain() must return 0 before msg loop
  if (hwnd == NULL) { popError(); return 0; }

  // main
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    if (TranslateAccelerator(hwnd, haccel, &msg)) {
    } else if (IsDialogMessage(hwnd, &msg)) {
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  if (debugMode) {
    TCHAR s[C_MAX_MSGTEXT];
    wsprintf(s, L"exit code: %d\nsuspend: %d\ntask: %d\ndeep: %d",
      msg.wParam, atimeover, task, deepsleep);
    MessageBox(0, s, 0, 0);
    return msg.wParam;
  } else if (atimeover) {
    switch (task) {
      case TASK_SLEEP: SetSuspendState(FALSE, FALSE, deepsleep); break;
      case TASK_HIBER: SetSuspendState(TRUE, FALSE, deepsleep); break;
      case TASK_DISPOFF: {
        SendNotifyMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
        break;
      }
      default: SetLastError(ERROR_NOT_SUPPORTED), popError(); break;
    }
  }
  // finish
  return msg.wParam;
}
