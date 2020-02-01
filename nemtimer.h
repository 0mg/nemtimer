#include <windows.h>
#include <commctrl.h>
#include <powrprof.h>
#include <shobjidl.h>

// const value
#define C_VAL_APPNAME   TEXT("NEMTimer")
#define C_VAL_APPVER    0,9,0,0
#define C_VAL_APPCODE   "nemtimer"

#define ID_ICO_EXE      0x001

// controls ID
#define ID_DLG_MAIN     0xC01
#define ID_DLG_OPTION   0xC02
#define ID_PB_MAIN      0xC03
#define ID_ICO_AWDISP   0xC05
#define ID_ICO_AWSYS    0xC06
#define ID_ICO_EXTRA    0xC07
#define ID_TXT_TIMER    0xC08
#define ID_TXT_TASK     0xC09
#define ID_TXT_EXTRA    0xC0A
#define ID_MNU_MAIN     0xC0B
#define ID_BTN_NEW      0xC0C
#define ID_BTN_STOP     0xC0D
#define ID_ICO_TASK     0xC10
#define ID_ICO_SLEEP    0xC10 // == ID_ICO_TASK
#define ID_ICO_HIBER    0xC11
#define ID_ICO_DISPOFF  0xC12
//efine ID_ICO_TASKLAST 0xC1F
// dialog: option
#define ID_EDIT_TIME    0xA0C
#define ID_EDIT_TASK    0xA0D
#define ID_EDIT_AWDISP  0xA0E
#define ID_EDIT_AWSYS   0xA0F
#define ID_GP_MAIN      0xA10
#define ID_DESC_TIME    0xA11
#define ID_CAP_TIME     0xA12
#define ID_DESC_TASK    0xA13
#define ID_CAP_TASK     0xA14
#define ID_GP_AWAKE     0xA15
#define ID_DESC_AWAKE   0xA16
#define ID_CAP_AWDISP   0xA17
#define ID_CAP_AWSYS    0xA18
#define ID_DESC_AWSYS   0xA19
#define ID_CHK_TASK     0xA1A
// dialog: about
#define ID_DLG_ABOUT    0xB01

// command ID, string ID. using with lang mask
#define C_CMD_STOP      0x100
#define C_CMD_EXIT      0x101
#define C_CMD_DISPOFF   0x102
#define C_CMD_AWAKEN    0x103
#define C_CMD_NEW       0x104
#define C_CMD_SAVEAS    0x105
#define C_CMD_ABOUT     0x106
#define C_CMD_MANUAL    0x107
#define C_CMD_START     0x108
/**/
#define C_STR_FILE      0x301
#define C_STR_TOOL      0x302
#define C_STR_HELP      0x303
#define C_STR_CTRL      0x304
#define C_STR_AUTO      0x308
#define C_STR_SUPRESS   0x309
#define C_STR_UNDEFINED 0x30A
#define C_STR_ABOUT_CAP 0x30B
#define C_STR_TASKFIRST 0x310
#define C_STR_SLEEP     0x310
#define C_STR_HIBER     0x311
#define C_STR_DISPOFF   0x312
//efine C_STR_TASKLAST  0x31F
/**/
#define C_CMD_LANG_DEFAULT  0xFF0
#define C_CMD_LANG_JA       0xFF1

// lang mask
#define C_LANG_DEFAULT  0x0000
#define C_LANG_JA       0x1000
