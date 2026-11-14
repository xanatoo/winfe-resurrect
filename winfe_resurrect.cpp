/*
 * winfe_resurrect.cpp
 *
 * Pure visual resurrection of the 1993 Udanax Gold Windows frontend (winfe).
 * Historical effort only — no real backend, no Xanadu protocol.
 *
 * Step 2: More accurate dialog layouts taken directly from fe.rc,
 *          proper modal Login / Document Info / Link Info dialogs,
 *          tighter form geometry matching original dialog units.
 *
 * Build (Windows / MinGW):
 *   g++ -std=c++17 -O2 -mwindows winfe_resurrect.cpp -o winfe_resurrect.exe -lcomctl32 -lgdi32 -luser32
 *
 * Run under Wine:
 *   wine winfe_resurrect.exe
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ---------------------------------------------------------------------------
// IDs matching original fe.h / fe.rc
// ---------------------------------------------------------------------------
enum {
    CM_GRAB            = 208,
    CM_RELEASE         = 209,
    CM_NEWVARIANT      = 204,
    CM_INFO            = 305,
    CM_LINKS           = 202,
    CM_STARTLINK       = 211,
    CM_ENDLINK         = 212,
    CM_ATTACHMENT      = 213,
    CM_DELETE          = 306,
    CM_SAVE            = 210,
    CM_RETURNHOME      = 207,
    CM_INITIALIZE      = 206,
    CM_TESTCLUBS       = 240,
    CM_EXIT            = 243,

    CM_EDITUNDO        = 57643,
    CM_EDITCUT         = 57635,
    CM_EDITCOPY        = 57634,
    CM_EDITPASTE       = 57637,
    CM_EDITDELETE      = 57636,
    CM_EDITCLEAR       = 57632,
    CM_INSERTFILE      = 214,
    CM_COPYTOFILE      = 215,
    CM_EDITFIND        = 57640,
    CM_EDITREPLACE     = 57641,
    CM_EDITFINDNEXT    = 57642,

    CM_CASCADECHILDREN = 57650,
    CM_TILECHILDREN    = 57651,
    CM_ARRANGEICONS    = 57652,
    CM_CLOSECHILDREN   = 57653,

    ID_HOSTNAME        = 140,
    ID_LOGINNAME       = 150,
    ID_OWNER           = 111,
    ID_READPERM        = 112,
    ID_EDITPERM        = 113,
    ID_LASTREVISIONBY  = 114,
    ID_CURRENTEDITOR   = 115,
    ID_TITLE           = 116,
    ID_RECORDHISTORY   = 117,

    FI_HOME_AKORD      = 101,
    FI_HOME_RATE       = 102,
    FI_HOME_MEMO1      = 103,

    FI_MEMO_TO         = 101,
    FI_MEMO_FROM       = 102,
    FI_MEMO_RE         = 103,
    FI_MEMO_MESSAGE    = 104,
    FI_MEMO_REFERENCE  = 105,

    FI_RATE_NAME       = 101,
    FI_RATE_UNDERWRITER= 102,
    FI_RATE_INSTRUCTIONS = 103,
    FI_RATE_SAMPLECHECKBOX = 104,

    FI_AKORD_CGL       = 101,
    FI_AKORD_CCC       = 102,

    ID_MAIN_STATUS     = 1001
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
HINSTANCE g_hInst   = nullptr;
HWND      g_hMain   = nullptr;
HWND      g_hStatus = nullptr;
HFONT     g_hFont   = nullptr;
int       g_childCount = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
HFONT CreateClassicFont(int height = -12, bool bold = false) {
    return CreateFontA(height, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");
}

void SetClassicFont(HWND hwnd) {
    if (g_hFont) SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

void ApplyClassicFontToChildren(HWND parent) {
    EnumChildWindows(parent, [](HWND hwnd, LPARAM) -> BOOL {
        SetClassicFont(hwnd);
        return TRUE;
    }, 0);
}

void ClassicMessage(HWND parent, const char* title, const char* text) {
    MessageBoxA(parent, text, title, MB_OK | MB_ICONINFORMATION);
}

// Convert rough dialog units to pixels (approximate for 8pt MS Sans Serif)
int duX(int d) { return (d * 6) / 4; }   // ~1.5x
int duY(int d) { return (d * 13) / 8; }  // ~1.625x

// ---------------------------------------------------------------------------
// Modal dialog helper (manual CreateWindow + modal loop)
// ---------------------------------------------------------------------------
struct ModalResult {
    int code = IDCANCEL;
};

// Simple generic modal dialog procedure for our constructed dialogs
static ModalResult* g_modalResult = nullptr;
static HWND         g_modalDlg    = nullptr;

LRESULT CALLBACK ModalDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK || id == IDCANCEL) {
            if (g_modalResult) g_modalResult->code = id;
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        EnableWindow(GetParent(hwnd), TRUE);
        DestroyWindow(hwnd);
        g_modalDlg = nullptr;
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

HWND BeginModalDialog(HWND parent, const char* title, int width, int height) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = ModalDlgProc;
        wc.hInstance     = g_hInst;
        wc.lpszClassName = "WinFEModalDlg";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassA(&wc);
        registered = true;
    }

    RECT rcParent;
    GetWindowRect(parent, &rcParent);
    int x = rcParent.left + ((rcParent.right - rcParent.left) - width) / 2;
    int y = rcParent.top  + ((rcParent.bottom - rcParent.top) - height) / 2;

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        "WinFEModalDlg", title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        x, y, width, height,
        parent, nullptr, g_hInst, nullptr);

    g_modalDlg = hDlg;
    EnableWindow(parent, FALSE);
    return hDlg;
}

int RunModalLoop(HWND hDlg, ModalResult* result) {
    g_modalResult = result;
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(hDlg);

    MSG msg;
    while (g_modalDlg && GetMessageA(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageA(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    g_modalResult = nullptr;
    return result->code;
}

// ---------------------------------------------------------------------------
// Login dialog (matches ID_LOGIN from fe.rc)
// ---------------------------------------------------------------------------
int ShowLoginDialog(HWND parent) {
    // Original: 164x71 dialog units → approx 320x160 px
    HWND hDlg = BeginModalDialog(parent, "Login Please", 340, 170);

    CreateWindowA("STATIC", "Host info:", WS_CHILD | WS_VISIBLE,
        18, 18, 80, 16, hDlg, nullptr, g_hInst, nullptr);
    CreateWindowA("EDIT", "socket vlad 65002",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        105, 16, 210, 22, hDlg, (HMENU)(intptr_t)ID_HOSTNAME, g_hInst, nullptr);

    CreateWindowA("STATIC", "Login name:", WS_CHILD | WS_VISIBLE,
        18, 50, 80, 16, hDlg, nullptr, g_hInst, nullptr);
    CreateWindowA("EDIT", "System Admin",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        105, 48, 210, 22, hDlg, (HMENU)(intptr_t)ID_LOGINNAME, g_hInst, nullptr);

    CreateWindowA("BUTTON", "&OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        155, 100, 75, 28, hDlg, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "&Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        240, 100, 75, 28, hDlg, (HMENU)IDCANCEL, g_hInst, nullptr);

    ApplyClassicFontToChildren(hDlg);
    SetClassicFont(hDlg);

    ModalResult res;
    return RunModalLoop(hDlg, &res);
}

// ---------------------------------------------------------------------------
// Link Info dialog (matches ID_LINKINFO)
// ---------------------------------------------------------------------------
int ShowLinkInfoDialog(HWND parent) {
    HWND hDlg = BeginModalDialog(parent, "Link info", 360, 280);

    auto label = [&](const char* t, int x, int y, int w = 120) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_RIGHT,
            x, y, w, 16, hDlg, nullptr, g_hInst, nullptr);
    };
    auto edit = [&](int id, int x, int y, int w, const char* text) {
        HWND h = CreateWindowA("EDIT", text,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            x, y, w, 22, hDlg, (HMENU)(intptr_t)id, g_hInst, nullptr);
        return h;
    };
    auto combo = [&](int id, int x, int y, int w) {
        return CreateWindowA("COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
            x, y, w, 100, hDlg, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    label("Owner:", 12, 16);
    edit(ID_OWNER, 140, 14, 180, "System Admin");

    label("Title:", 12, 46);
    edit(ID_TITLE, 140, 44, 180, "Sample Link");

    label("Read permission:", 12, 80);
    HWND hRead = combo(ID_READPERM, 140, 78, 140);
    label("Edit permission:", 12, 112);
    HWND hEdit = combo(ID_EDITPERM, 140, 110, 140);

    const char* perms[] = { "Private", "Club", "Public" };
    for (auto p : perms) {
        SendMessageA(hRead, CB_ADDSTRING, 0, (LPARAM)p);
        SendMessageA(hEdit, CB_ADDSTRING, 0, (LPARAM)p);
    }
    SendMessageA(hRead, CB_SETCURSEL, 0, 0);
    SendMessageA(hEdit, CB_SETCURSEL, 0, 0);

    label("Last revision by:", 12, 150);
    edit(ID_LASTREVISIONBY, 140, 148, 180, "Ravi Pandya");
    label("Current editor:", 12, 180);
    edit(ID_CURRENTEDITOR, 140, 178, 180, "(none)");

    CreateWindowA("BUTTON", "&OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        170, 220, 75, 28, hDlg, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "&Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        255, 220, 75, 28, hDlg, (HMENU)IDCANCEL, g_hInst, nullptr);

    ApplyClassicFontToChildren(hDlg);
    SetClassicFont(hDlg);

    ModalResult res;
    return RunModalLoop(hDlg, &res);
}

// ---------------------------------------------------------------------------
// Document Info dialog (matches ID_DOCINFO)
// ---------------------------------------------------------------------------
int ShowDocInfoDialog(HWND parent) {
    HWND hDlg = BeginModalDialog(parent, "Document info for ", 440, 340);

    auto label = [&](const char* t, int x, int y, int w = 120) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_RIGHT,
            x, y, w, 16, hDlg, nullptr, g_hInst, nullptr);
    };

    label("Title:", 12, 16, 50);
    CreateWindowA("EDIT", "Untitled Document",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        70, 14, 340, 22, hDlg, (HMENU)(intptr_t)ID_TITLE, g_hInst, nullptr);

    label("Owner:", 12, 50, 50);
    CreateWindowA("STATIC", "System Admin",
        WS_CHILD | WS_VISIBLE,
        70, 50, 200, 16, hDlg, (HMENU)(intptr_t)ID_OWNER, g_hInst, nullptr);

    // Separator-like line
    CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        20, 80, 390, 2, hDlg, nullptr, g_hInst, nullptr);

    label("Read permission:", 12, 100);
    HWND hRead = CreateWindowA("COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
        140, 98, 160, 100, hDlg, (HMENU)(intptr_t)ID_READPERM, g_hInst, nullptr);

    label("Edit permission:", 12, 134);
    HWND hEdit = CreateWindowA("COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
        140, 132, 160, 100, hDlg, (HMENU)(intptr_t)ID_EDITPERM, g_hInst, nullptr);

    label("History recording:", 12, 168);
    HWND hHist = CreateWindowA("COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
        140, 166, 160, 100, hDlg, (HMENU)(intptr_t)ID_RECORDHISTORY, g_hInst, nullptr);

    const char* perms[] = { "Private", "Club", "Public" };
    const char* hist[]  = { "None", "Basic", "Full" };
    for (auto p : perms) {
        SendMessageA(hRead, CB_ADDSTRING, 0, (LPARAM)p);
        SendMessageA(hEdit, CB_ADDSTRING, 0, (LPARAM)p);
    }
    for (auto h : hist) SendMessageA(hHist, CB_ADDSTRING, 0, (LPARAM)h);
    SendMessageA(hRead, CB_SETCURSEL, 0, 0);
    SendMessageA(hEdit, CB_SETCURSEL, 0, 0);
    SendMessageA(hHist, CB_SETCURSEL, 0, 0);

    CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        20, 210, 390, 2, hDlg, nullptr, g_hInst, nullptr);

    label("Last revision by:", 12, 230);
    CreateWindowA("STATIC", "Ravi Pandya",
        WS_CHILD | WS_VISIBLE, 140, 230, 150, 16, hDlg, (HMENU)(intptr_t)ID_LASTREVISIONBY, g_hInst, nullptr);
    label("Current editor:", 12, 256);
    CreateWindowA("STATIC", "(none)",
        WS_CHILD | WS_VISIBLE, 140, 256, 150, 16, hDlg, (HMENU)(intptr_t)ID_CURRENTEDITOR, g_hInst, nullptr);

    CreateWindowA("BUTTON", "&OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        250, 290, 75, 28, hDlg, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "&Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        335, 290, 75, 28, hDlg, (HMENU)IDCANCEL, g_hInst, nullptr);

    ApplyClassicFontToChildren(hDlg);
    SetClassicFont(hDlg);

    ModalResult res;
    return RunModalLoop(hDlg, &res);
}

// ---------------------------------------------------------------------------
// Child form windows
// ---------------------------------------------------------------------------
LRESULT CALLBACK FormWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND CreateFormWindow(const char* title, int type) {
    g_childCount++;
    char className[64];
    sprintf(className, "WinFEForm%d", type);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = FormWndProc;
    wc.hInstance     = g_hInst;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    int w = 380, h = 300;
    if (type == 0) { w = 340; h = 120; }      // Desktop
    if (type == 1) { w = 400; h = 340; }      // Memo
    if (type == 2) { w = 400; h = 320; }      // Rate
    if (type == 3) { w = 620; h = 580; }      // Akord (full historical form)

    HWND hwnd = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        className, title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        30 + g_childCount * 28, 40 + g_childCount * 28,
        w, h,
        g_hMain, nullptr, g_hInst, (LPVOID)(intptr_t)type);
    return hwnd;
}

void BuildHomeForm(HWND hwnd) {
    // FDLG_HOME: three buttons
    CreateWindowA("BUTTON", "Akord form",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        20, 30, 90, 30, hwnd, (HMENU)(intptr_t)FI_HOME_AKORD, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Rate form",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        125, 30, 90, 30, hwnd, (HMENU)(intptr_t)FI_HOME_RATE, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Memo 1",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        230, 30, 90, 30, hwnd, (HMENU)(intptr_t)FI_HOME_MEMO1, g_hInst, nullptr);
    ApplyClassicFontToChildren(hwnd);
}

void BuildMemoForm(HWND hwnd) {
    // FDLG_MEMO layout
    auto lab = [&](const char* t, int y) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_RIGHT,
            12, y, 70, 16, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int y, int h = 22, bool multi = false) {
        DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP | ES_LEFT;
        if (multi) st |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        CreateWindowA("EDIT", "", st, 90, y, 270, h, hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    lab("To:", 18);      ed(FI_MEMO_TO, 16);
    lab("From:", 50);    ed(FI_MEMO_FROM, 48);
    lab("Re:", 82);      ed(FI_MEMO_RE, 80);
    lab("Message:", 120); ed(FI_MEMO_MESSAGE, 118, 100, true);

    CreateWindowA("BUTTON", "Reference",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        150, 235, 100, 28, hwnd, (HMENU)(intptr_t)FI_MEMO_REFERENCE, g_hInst, nullptr);

    SetDlgItemTextA(hwnd, FI_MEMO_TO, "Ted Nelson");
    SetDlgItemTextA(hwnd, FI_MEMO_FROM, "Ravi Pandya");
    SetDlgItemTextA(hwnd, FI_MEMO_RE, "Enfilade progress");
    SetDlgItemTextA(hwnd, FI_MEMO_MESSAGE,
        "The coordinate space work is looking solid.\r\n"
        "Transclusion tests pass on the current Ent.\r\n"
        "— 1993");

    ApplyClassicFontToChildren(hwnd);
}

void BuildRateForm(HWND hwnd) {
    // FDLG_RATE / "ACME Rate Request"
    auto lab = [&](const char* t, int y) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_RIGHT,
            12, y, 100, 16, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int y, int h = 22) {
        CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            120, y, 240, h, hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    lab("Name:", 18);         ed(FI_RATE_NAME, 16);
    lab("Underwriter:", 50);  ed(FI_RATE_UNDERWRITER, 48);
    lab("Instructions:", 82); ed(FI_RATE_INSTRUCTIONS, 80, 90);

    CreateWindowA("BUTTON", "Sample Checkbox",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        120, 190, 180, 22, hwnd, (HMENU)(intptr_t)FI_RATE_SAMPLECHECKBOX, g_hInst, nullptr);

    SetDlgItemTextA(hwnd, FI_RATE_NAME, "Standard CGL");
    SetDlgItemTextA(hwnd, FI_RATE_UNDERWRITER, "Udanax Underwriting");
    SetDlgItemTextA(hwnd, FI_RATE_INSTRUCTIONS,
        "Rate according to 1992 schedule.\r\n"
        "Apply club discounts where applicable.");

    ApplyClassicFontToChildren(hwnd);
}

void BuildAkordForm(HWND hwnd) {
    // Full historical "AKORD General Liability Section" from pcaide/akord.pvt
    // Dialog units scaled ~2x for modern readability while preserving layout density.

    auto lab = [&](const char* t, int x, int y, int w = 80) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 14, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int x, int y, int w, int h, const char* text = "") {
        DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP | ES_LEFT;
        if (h > 20) st |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        HWND hWnd = CreateWindowA("EDIT", text, st, x, y, w, h,
            hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
        return hWnd;
    };
    auto chk = [&](int id, const char* t, int x, int y, int w) {
        CreateWindowA("BUTTON", t,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            x, y, w, 16, hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    // Header
    lab("GENERAL LIABILITY SECTION", 100, 6, 200);
    lab("DATE", 460, 8, 40);
    ed(1, 500, 6, 90, 18, "1/30/93");

    // Producer / Applicant block
    lab("PRODUCER", 8, 32, 70);
    ed(3, 8, 48, 170, 70, "Willy Loman");

    lab("APPLICANT", 190, 32, 70);
    ed(2, 190, 48, 280, 50, "Unlucky Markets");

    // Policy period / billing
    lab("PROP EFF", 190, 108, 60);
    ed(4, 250, 106, 70, 18, "4/1/93");
    lab("PROP EXP", 330, 108, 60);
    ed(5, 390, 106, 70, 18, "4/1/94");

    lab("BILLING", 470, 90, 50);
    chk(6, "AGENCY", 470, 106, 70);
    chk(7, "DIRECT", 470, 122, 70);
    lab("PAYMENT", 540, 90, 50);
    ed(8, 540, 106, 50, 18, "TBD");
    lab("AUDIT", 540, 128, 40);
    ed(9, 540, 144, 30, 18, "Y");

    // Coverages
    lab("COVERAGES / LIMITS", 8, 160, 140);
    lab("LIMITS OF LIABILITY", 300, 160, 140);
    lab("(000)", 480, 160, 40);

    chk(15, "COMPREHENSIVE GENERAL LIABILITY", 8, 180, 240);
    chk(16, "OWNERS, LANDLORDS, && TENANTS", 8, 198, 240);
    chk(10, "MANUFACTURER'S && CONTRACTORS", 8, 216, 240);
    chk(11, "STOREKEEPERS LIABILITY", 8, 234, 240);
    chk(12, "OWNER'S && CONTRACTORS PROTECTIVE", 8, 252, 240);
    chk(17, "CONTRACTUAL", 8, 270, 100);
    chk(18, "BLANKET", 110, 270, 80);
    chk(13, "DESIG.", 200, 270, 60);
    chk(14, "PRODUCTS/COMPLETED OPERATIONS", 8, 288, 240);

    // Options / endorsements
    lab("OPTIONS", 8, 320, 80);
    chk(24, "BROAD FORM PROPERTY DAMAGE", 8, 338, 240);
    chk(22, "EXCLUDE COMPL. OPS", 8, 356, 160);
    chk(19, "BROAD FORM CGL ENDORSEMENT", 8, 374, 240);
    chk(20, "XC", 8, 392, 40);
    chk(51, "DELETE EXCLUSION C", 60, 392, 160);
    chk(21, "FIRE LEGAL LIABILITY (Give Loc's && Lim's)", 8, 410, 280);
    chk(25, "ELEVATOR COLLISION", 8, 428, 180);
    chk(23, "NON-OWNED AUTO (Give Terr's && Emp's)", 8, 446, 260);

    // Limits / money fields (right column)
    lab("COVERAGE", 300, 180, 80);
    lab("COMB SING LIM", 300, 200, 100);
    ed(40, 410, 198, 60, 18, "");
    lab("P.D. DEDUCT.", 300, 230, 90);
    ed(45, 410, 228, 60, 18, "$");
    lab("PARTIC. %", 300, 258, 80);
    ed(50, 410, 256, 50, 18, "");
    lab("TOTAL", 300, 290, 50);
    ed(46, 410, 288, 60, 18, "$");

    // Care/custody note (matches original sample text)
    lab("Care, custody, and control / Broad form liability", 300, 330, 280);
    ed(52, 300, 348, 280, 60,
        "Care, custody, and control / Broad form liability");

    // Class letters
    chk(48, "A", 300, 420, 30);
    chk(49, "B", 340, 420, 30);
    chk(47, "C", 380, 420, 30);

    // Action buttons (historical labels)
    CreateWindowA("BUTTON", "Set Aside",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        380, 500, 90, 28, hwnd, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Done",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        480, 500, 90, 28, hwnd, (HMENU)IDCANCEL, g_hInst, nullptr);

    // Pre-check the historically checked boxes from SetupWindow()
    // (PTCheck15, 11, 17, 18, 14 were checked in the original)
    CheckDlgButton(hwnd, 15, BST_CHECKED); // COMPREHENSIVE GENERAL LIABILITY
    CheckDlgButton(hwnd, 11, BST_CHECKED); // STOREKEEPERS LIABILITY
    CheckDlgButton(hwnd, 17, BST_CHECKED); // CONTRACTUAL
    CheckDlgButton(hwnd, 18, BST_CHECKED); // BLANKET
    CheckDlgButton(hwnd, 14, BST_CHECKED); // PRODUCTS/COMPLETED OPERATIONS

    ApplyClassicFontToChildren(hwnd);
}

LRESULT CALLBACK FormWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int type = (int)(intptr_t)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        type = (int)(intptr_t)cs->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)type);
        switch (type) {
        case 0: BuildHomeForm(hwnd); break;
        case 1: BuildMemoForm(hwnd); break;
        case 2: BuildRateForm(hwnd); break;
        case 3: BuildAkordForm(hwnd); break;
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (type == 0) {
            if (id == FI_HOME_AKORD) CreateFormWindow("AKORD General Liability Section", 3);
            if (id == FI_HOME_RATE)  CreateFormWindow("ACME Rate Request", 2);
            if (id == FI_HOME_MEMO1) CreateFormWindow("Memo", 1);
        }
        if (id == FI_MEMO_REFERENCE) {
            ShowLinkInfoDialog(hwnd);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Main frame
// ---------------------------------------------------------------------------
HMENU BuildMainMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hDoc  = CreatePopupMenu();
    HMENU hLinks= CreatePopupMenu();
    HMENU hEdit = CreatePopupMenu();
    HMENU hWin  = CreatePopupMenu();

    AppendMenuA(hDoc, MF_STRING, CM_GRAB,        "&Grab");
    AppendMenuA(hDoc, MF_STRING, CM_RELEASE,     "&Release");
    AppendMenuA(hDoc, MF_STRING, CM_NEWVARIANT,  "&New variant");
    AppendMenuA(hDoc, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hDoc, MF_STRING, CM_INFO,        "&Info");
    AppendMenuA(hLinks, MF_STRING, CM_LINKS,     "&Links");
    AppendMenuA(hLinks, MF_STRING, CM_STARTLINK, "&Start Link");
    AppendMenuA(hLinks, MF_STRING, CM_ENDLINK,   "End &Link");
    AppendMenuA(hLinks, MF_STRING, CM_ATTACHMENT,"&Attachment");
    AppendMenuA(hDoc, MF_POPUP, (UINT_PTR)hLinks, "&Links");
    AppendMenuA(hDoc, MF_STRING, CM_DELETE,      "D&elete");
    AppendMenuA(hDoc, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hDoc, MF_STRING, CM_SAVE,        "Save All");
    AppendMenuA(hDoc, MF_STRING, CM_RETURNHOME,  "Return &Home");
    AppendMenuA(hDoc, MF_STRING, CM_INITIALIZE,  "Initiali&ze");
    AppendMenuA(hDoc, MF_STRING, CM_TESTCLUBS,   "Test Clubs");
    AppendMenuA(hDoc, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hDoc, MF_STRING, CM_EXIT,        "E&xit");

    AppendMenuA(hEdit, MF_STRING, CM_EDITUNDO,   "&Undo\tCtrl+Z");
    AppendMenuA(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hEdit, MF_STRING, CM_EDITCUT,    "&Cut\tCtrl+X");
    AppendMenuA(hEdit, MF_STRING, CM_EDITCOPY,   "C&opy\tCtrl+C");
    AppendMenuA(hEdit, MF_STRING, CM_EDITPASTE,  "&Paste\tCtrl+V");
    AppendMenuA(hEdit, MF_STRING, CM_EDITDELETE, "&Delete\tDel");
    AppendMenuA(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hEdit, MF_STRING, CM_INSERTFILE, "Insert file...");
    AppendMenuA(hEdit, MF_STRING, CM_COPYTOFILE, "Copy to file...");
    AppendMenuA(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hEdit, MF_STRING, CM_EDITFIND,   "&Find...\tCtrl+F");
    AppendMenuA(hEdit, MF_STRING, CM_EDITREPLACE,"&Replace...\tCtrl+R");
    AppendMenuA(hEdit, MF_STRING, CM_EDITFINDNEXT,"&Next\tCtrl+G");

    AppendMenuA(hWin, MF_STRING, CM_CASCADECHILDREN, "&Cascade");
    AppendMenuA(hWin, MF_STRING, CM_TILECHILDREN,    "&Tile");
    AppendMenuA(hWin, MF_STRING, CM_ARRANGEICONS,    "Arrange &Icons");
    AppendMenuA(hWin, MF_STRING, CM_CLOSECHILDREN,   "C&lose All");

    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hDoc,  "&Document");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hEdit, "&Edit");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hWin,  "&Window");
    return hMenu;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hFont = CreateClassicFont(-12, false);
        g_hStatus = CreateWindowA("STATIC",
            " Udanax Gold Frontend  —  1993 visual resurrection (step 3)  |  Full Akord form from akord.pvt",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            0, 0, 0, 0, hwnd, (HMENU)(intptr_t)ID_MAIN_STATUS, g_hInst, nullptr);
        SetClassicFont(g_hStatus);
        PostMessageA(hwnd, WM_USER + 1, 0, 0);
        return 0;
    }
    case WM_USER + 1: {
        ShowLoginDialog(hwnd);
        CreateFormWindow("Desktop", 0);
        return 0;
    }
    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (g_hStatus) MoveWindow(g_hStatus, 0, rc.bottom - 22, rc.right, 22, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case CM_EXIT:
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
            break;
        case CM_RETURNHOME:
        case CM_INITIALIZE:
            CreateFormWindow("Desktop", 0);
            break;
        case CM_INFO:
            ShowDocInfoDialog(hwnd);
            break;
        case CM_LINKS:
        case CM_STARTLINK:
        case CM_ENDLINK:
        case CM_ATTACHMENT:
            ShowLinkInfoDialog(hwnd);
            break;
        case CM_GRAB: case CM_RELEASE: case CM_NEWVARIANT:
        case CM_SAVE: case CM_DELETE: case CM_TESTCLUBS:
            ClassicMessage(hwnd, "Document",
                "Historical command recognised.\nNo live backend in this visual resurrection.");
            break;
        case CM_EDITUNDO: case CM_EDITCUT: case CM_EDITCOPY:
        case CM_EDITPASTE: case CM_EDITDELETE:
        case CM_INSERTFILE: case CM_COPYTOFILE:
        case CM_EDITFIND: case CM_EDITREPLACE: case CM_EDITFINDNEXT:
            ClassicMessage(hwnd, "Edit", "Standard edit command (menu fidelity).");
            break;
        case CM_CASCADECHILDREN: case CM_TILECHILDREN:
        case CM_ARRANGEICONS: case CM_CLOSECHILDREN:
            ClassicMessage(hwnd, "Window", "MDI management (visual stub).");
            break;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "WinFEResurrectMain";
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassA(&wc);

    HMENU hMenu = BuildMainMenu();
    g_hMain = CreateWindowExA(0, "WinFEResurrectMain",
        "Udanax Gold  —  Frontend (1993 visual resurrection)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 920, 680,
        nullptr, hMenu, hInstance, nullptr);

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
