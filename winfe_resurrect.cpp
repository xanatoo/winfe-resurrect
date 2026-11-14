/*
 * winfe_resurrect.cpp
 *
 * Visual resurrection of the 1993 Udanax Gold Windows frontend (winfe).
 * In-memory backend stub for pseudo document / link operations.
 * Historical effort — not a real FeBe / Xanadu protocol client.
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

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
HWND      g_hClient = nullptr;   // MDICLIENT
HWND      g_hStatus = nullptr;
HFONT     g_hFont   = nullptr;
int       g_childCount = 0;

void CascadeChildren() {
    if (g_hClient)
        SendMessageA(g_hClient, WM_MDICASCADE, MDITILE_SKIPDISABLED, 0);
}
void TileChildren() {
    if (g_hClient)
        SendMessageA(g_hClient, WM_MDITILE, MDITILE_HORIZONTAL, 0);
}
void ArrangeIcons() {
    if (g_hClient)
        SendMessageA(g_hClient, WM_MDIICONARRANGE, 0, 0);
}
void CloseAllChildren() {
    if (!g_hClient) return;
    HWND child;
    while ((child = (HWND)SendMessageA(g_hClient, WM_MDIGETACTIVE, 0, 0)) != nullptr) {
        SendMessageA(g_hClient, WM_MDIDESTROY, (WPARAM)child, 0);
    }
}

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


void SetStatus(const char* text) {
    if (g_hStatus) SetWindowTextA(g_hStatus, text);
}

// ---------------------------------------------------------------------------
// In-memory backend stub (pseudo FeBe)
// ---------------------------------------------------------------------------
struct StubDoc {
    int id = 0;
    std::string title;
    std::string owner = "System Admin";
    std::string content;
    std::string lastRevBy = "Ravi Pandya";
    std::string currentEditor = "(none)";
    int readPerm = 0;   // 0 Private, 1 Club, 2 Public
    int editPerm = 0;
    int history  = 0;   // 0 None, 1 Basic, 2 Full
    int variant  = 1;
    bool grabbed = false;
    bool dirty   = false;
};

struct StubLink {
    int id = 0;
    std::string title;
    std::string owner = "System Admin";
    int fromDoc = 0;
    int toDoc   = 0;
    int readPerm = 0;
    int editPerm = 0;
    std::string lastRevBy = "Ravi Pandya";
    bool pendingStart = false;
};

struct Backend {
    bool connected = false;
    std::string host = "socket vlad 65002";
    std::string user = "System Admin";
    int nextDocId  = 1;
    int nextLinkId = 1;
    int activeDoc  = 0;
    int pendingLinkFrom = 0; // for Start Link / End Link
    std::string lastFind;
    size_t lastFindPos = 0;
    std::map<int, StubDoc> docs;
    std::vector<StubLink> links;

    void seed() {
        if (!docs.empty()) return;
        StubDoc home;
        home.id = nextDocId++;
        home.title = "Desktop";
        home.content =
            "Udanax Gold Desktop\r\n"
            "==================\r\n\r\n"
            "This is a stub mid-end. Documents and links are held in memory only.\r\n";
        home.readPerm = 2;
        docs[home.id] = home;

        StubDoc memo;
        memo.id = nextDocId++;
        memo.title = "Memo: Enfilade progress";
        memo.owner = "Ravi Pandya";
        memo.content =
            "To: Ted Nelson\r\n"
            "From: Ravi Pandya\r\n\r\n"
            "The coordinate space work is looking solid.\r\n"
            "Transclusion tests pass on the current Ent.\r\n";
        memo.readPerm = 1;
        memo.editPerm = 1;
        docs[memo.id] = memo;

        StubDoc cgl;
        cgl.id = nextDocId++;
        cgl.title = "Unlucky Markets — CGL package";
        cgl.content =
            "Applicant: Unlucky Markets\r\n"
            "Producer: Willy Loman\r\n"
            "Effective: 4/1/93 — 4/1/94\r\n"
            "Coverages: CGL, Products/Completed Ops, Contractual\r\n";
        cgl.readPerm = 1;
        docs[cgl.id] = cgl;

        StubLink l;
        l.id = nextLinkId++;
        l.title = "Memo → CGL package";
        l.fromDoc = memo.id;
        l.toDoc = cgl.id;
        l.readPerm = 1;
        links.push_back(l);

        activeDoc = home.id;
    }

    StubDoc* getActive() {
        auto it = docs.find(activeDoc);
        return it == docs.end() ? nullptr : &it->second;
    }

    StubDoc* get(int id) {
        auto it = docs.find(id);
        return it == docs.end() ? nullptr : &it->second;
    }

    int createDoc(const std::string& title) {
        StubDoc d;
        d.id = nextDocId++;
        d.title = title;
        d.owner = user;
        d.content = "(new document)\r\n";
        d.currentEditor = user;
        d.grabbed = true;
        d.dirty = true;
        docs[d.id] = d;
        activeDoc = d.id;
        return d.id;
    }

    bool grab(int id) {
        StubDoc* d = get(id);
        if (!d) return false;
        if (d->grabbed && d->currentEditor != user) return false;
        d->grabbed = true;
        d->currentEditor = user;
        activeDoc = id;
        return true;
    }

    bool release(int id) {
        StubDoc* d = get(id);
        if (!d || !d->grabbed) return false;
        d->grabbed = false;
        d->currentEditor = "(none)";
        d->lastRevBy = user;
        return true;
    }

    bool save(int id) {
        StubDoc* d = get(id);
        if (!d) return false;
        d->dirty = false;
        d->lastRevBy = user;
        return true;
    }

    bool newVariant(int id) {
        StubDoc* d = get(id);
        if (!d) return false;
        StubDoc v = *d;
        v.id = nextDocId++;
        v.variant = d->variant + 1;
        v.title = d->title + " (v" + std::to_string(v.variant) + ")";
        v.grabbed = true;
        v.dirty = true;
        v.currentEditor = user;
        docs[v.id] = v;
        activeDoc = v.id;
        return true;
    }

    bool remove(int id) {
        if (docs.erase(id) == 0) return false;
        if (activeDoc == id) activeDoc = docs.empty() ? 0 : docs.begin()->first;
        return true;
    }

    int startLink() {
        if (!activeDoc) return 0;
        pendingLinkFrom = activeDoc;
        return pendingLinkFrom;
    }

    int endLink() {
        if (!pendingLinkFrom || !activeDoc || pendingLinkFrom == activeDoc)
            return 0;
        StubLink l;
        l.id = nextLinkId++;
        StubDoc* a = get(pendingLinkFrom);
        StubDoc* b = get(activeDoc);
        l.title = (a ? a->title : "?") + " → " + (b ? b->title : "?");
        l.fromDoc = pendingLinkFrom;
        l.toDoc = activeDoc;
        l.owner = user;
        links.push_back(l);
        int id = l.id;
        pendingLinkFrom = 0;
        return id;
    }

    std::string statusLine() const {
        std::ostringstream os;
        os << " " << (connected ? "Connected" : "Offline")
           << "  " << host
           << "  |  user: " << user
           << "  |  docs: " << docs.size()
           << "  links: " << links.size();
        if (activeDoc) {
            auto it = docs.find(activeDoc);
            if (it != docs.end()) {
                os << "  |  active: " << it->second.title;
                if (it->second.grabbed) os << " [GRABBED]";
                if (it->second.dirty) os << " *";
            }
        }
        if (pendingLinkFrom) os << "  |  link-start set";
        return os.str();
    }
};

static Backend g_be;

void RefreshStatus() {
    SetStatus(g_be.statusLine().c_str());
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
    int code = RunModalLoop(hDlg, &res);
    if (code == IDOK) {
        g_be.connected = true;
        g_be.host = "socket vlad 65002";
        g_be.user = "System Admin";
    }
    g_be.seed();
    RefreshStatus();
    return code;
}

// ---------------------------------------------------------------------------
// Link Info dialog (matches ID_LINKINFO)
// ---------------------------------------------------------------------------
int ShowLinkInfoDialog(HWND parent) {
    g_be.seed();
    StubLink* L = g_be.links.empty() ? nullptr : &g_be.links.back();
    HWND hDlg = BeginModalDialog(parent, "Link info", 400, 300);

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
    edit(ID_OWNER, 140, 14, 220, L ? L->owner.c_str() : g_be.user.c_str());

    label("Title:", 12, 46);
    edit(ID_TITLE, 140, 44, 220, L ? L->title.c_str() : "(no links yet)");

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
    edit(ID_LASTREVISIONBY, 140, 148, 220, L ? L->lastRevBy.c_str() : g_be.user.c_str());
    label("Current editor:", 12, 180);
    edit(ID_CURRENTEDITOR, 140, 178, 220, "(none)");

    char countbuf[64];
    sprintf(countbuf, "Links in mid-end: %d", (int)g_be.links.size());
    CreateWindowA("STATIC", countbuf, WS_CHILD | WS_VISIBLE, 20, 210, 220, 16, hDlg, nullptr, g_hInst, nullptr);

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
    g_be.seed();
    StubDoc* d = g_be.getActive();
    std::string caption = "Document info for ";
    caption += d ? d->title : "(none)";
    HWND hDlg = BeginModalDialog(parent, caption.c_str(), 440, 360);

    auto label = [&](const char* t, int x, int y, int w = 120) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_RIGHT,
            x, y, w, 16, hDlg, nullptr, g_hInst, nullptr);
    };

    label("Title:", 12, 16, 50);
    CreateWindowA("EDIT", d ? d->title.c_str() : "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        70, 14, 340, 22, hDlg, (HMENU)(intptr_t)ID_TITLE, g_hInst, nullptr);

    label("Owner:", 12, 50, 50);
    CreateWindowA("STATIC", d ? d->owner.c_str() : "",
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
    // Single MDI child class for all form types
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = FormWndProc;
        wc.cbWndExtra    = sizeof(void*);
        wc.hInstance     = g_hInst;
        wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "WinFEMdiChild";
        RegisterClassA(&wc);
        registered = true;
    }

    int w = 380, h = 300;
    if (type == 0) { w = 520; h = 360; }      // Desktop
    if (type == 1) { w = 560; h = 520; }      // Memo
    if (type == 2) { w = 600; h = 560; }      // Rate
    if (type == 3) { w = 620; h = 580; }      // Akord
    if (type == 4) { w = 520; h = 560; }      // Underwriters Manual
    if (type == 5) { w = 420; h = 160; }      // Attention
    if (type == 6) { w = 400; h = 140; }      // Reference Area (Tabs)
    if (type == 7) { w = 560; h = 580; }      // Contractual Profile
    if (type == 8) { w = 520; h = 400; }      // Document content (stub)
    if (type == 9) { w = 420; h = 320; }      // Document browser
    if (type == 10) { w = 420; h = 320; }     // Link browser

    // Encode type as type+1 so lpCreateParams is never NULL (type 0 Desktop)
    HWND hwnd = CreateMDIWindowA(
        "WinFEMdiChild",
        title,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
            WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        12 + (g_childCount % 8) * 24,
        12 + (g_childCount % 8) * 24,
        w, h,
        g_hClient,
        g_hInst,
        (LPARAM)(intptr_t)(type + 1));
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

// Extra form IDs for Desktop launch buttons
enum {
    FI_HOME_UM     = 104,
    FI_HOME_TABS   = 105,
    FI_HOME_ATTN   = 106,
    FI_HOME_CPROF  = 107,
    FI_HOME_DOCS   = 108,
    FI_HOME_DOCBASE= 200   // 200+N -> open seed doc by index
};

void BuildHomeForm(HWND hwnd) {
    auto btn = [&](const char* label, int id, int x, int y, int bw, int bh) {
        HWND ctl = CreateWindowA("BUTTON", label,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            x, y, bw, bh, hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
        if (ctl) ShowWindow(ctl, SW_SHOW);
        return ctl;
    };
    auto lab = [&](const char* t, int x, int y, int bw) {
        HWND ctl = CreateWindowA("STATIC", t,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, bw, 16, hwnd, nullptr, g_hInst, nullptr);
        if (ctl) ShowWindow(ctl, SW_SHOW);
        return ctl;
    };

    lab("Desktop — forms and mid-end documents", 12, 8, 400);
    lab("Forms", 12, 32, 80);

    btn("Akord form",          FI_HOME_AKORD, 12,  52, 120, 28);
    btn("Rate form",           FI_HOME_RATE,  144, 52, 120, 28);
    btn("Memo form",           FI_HOME_MEMO1, 276, 52, 120, 28);
    btn("Underwriters Manual",FI_HOME_UM,    12,  88, 160, 28);
    btn("Reference Area",     FI_HOME_TABS,  184, 88, 130, 28);
    btn("Attention",          FI_HOME_ATTN,  326, 88, 100, 28);
    btn("Contractual Profile",FI_HOME_CPROF, 12, 124, 160, 28);
    btn("Document Browser...",FI_HOME_DOCS,  184, 124, 160, 28);
    btn("Link Browser...",    109,           356, 124, 120, 28);

    lab("Mid-end documents (open / grab)", 12, 168, 300);

    g_be.seed();
    int y = 190, n = 0;
    for (auto& kv : g_be.docs) {
        btn(kv.second.title.c_str(), FI_HOME_DOCBASE + n, 12, y, 280, 26);
        y += 30;
        if (++n >= 6) break;
    }
    if (n == 0)
        lab("(no documents — use Initialize)", 12, 190, 300);

    ApplyClassicFontToChildren(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void BuildMemoForm(HWND hwnd) {
    // Historical "Memorandum" from pcaide/memo.pvt
    auto lab = [&](const char* t, int x, int y, int w = 70) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 14, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int x, int y, int w, int h, const char* text = "") {
        DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | WS_TABSTOP | ES_AUTOHSCROLL;
        if (h > 24) st |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        CreateWindowA("EDIT", text, st, x, y, w, h,
            hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    lab("ACME", 10, 8, 50);
    lab("CASUALTY INSURANCE", 70, 8, 160);
    lab("MEMORANDUM", 10, 28, 120);
    lab("DATE", 400, 8, 40);
    ed(1, 440, 6, 90, 20, "3/15/93");

    lab("FROM", 10, 50, 50);
    ed(FI_MEMO_FROM, 60, 48, 220, 40, "Ravi Pandya");
    lab("FILE", 300, 50, 40);
    ed(2, 340, 48, 180, 40, "CGL-92-014");

    lab("TO", 10, 100, 40);
    ed(FI_MEMO_TO, 60, 98, 220, 50, "Ted Nelson\r\nMark Miller");

    CreateWindowA("BUTTON", "References...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        400, 110, 110, 26, hwnd, (HMENU)(intptr_t)FI_MEMO_REFERENCE, g_hInst, nullptr);

    lab("MEMORANDUM", 10, 160, 100);
    ed(FI_MEMO_MESSAGE, 10, 178, 520, 240,
        "The coordinate space work is looking solid.\r\n"
        "Transclusion tests pass on the current Ent.\r\n\r\n"
        "Please review the attached rate worksheet before\r\n"
        "the Thursday underwriting meeting.\r\n\r\n"
        "— Ravi, 1993");

    CreateWindowA("BUTTON", "Set Aside",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        340, 440, 90, 28, hwnd, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Done",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        440, 440, 90, 28, hwnd, (HMENU)IDCANCEL, g_hInst, nullptr);

    ApplyClassicFontToChildren(hwnd);
}

void BuildRateForm(HWND hwnd) {
    // Historical "Rating Instructions" from pcaide/rate.pvt
    auto lab = [&](const char* t, int x, int y, int w = 80) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 14, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int x, int y, int w, int h, const char* text = "") {
        DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | WS_TABSTOP | ES_AUTOHSCROLL;
        if (h > 22) st |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        CreateWindowA("EDIT", text, st, x, y, w, h,
            hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };
    auto chk = [&](int id, const char* t, int x, int y, int w) {
        CreateWindowA("BUTTON", t,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            x, y, w, 16, hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    lab("DATE", 480, 10, 40);
    ed(1, 520, 8, 60, 20, "2/1/93");

    lab("PRODUCER", 10, 36, 70);
    ed(3, 10, 52, 150, 70, "Willy Loman");
    lab("APPLICANT", 180, 36, 70);
    ed(2, 180, 52, 280, 50, "Unlucky Markets");

    lab("PROP EFF", 180, 115, 60);
    ed(4, 245, 113, 70, 20, "4/1/93");
    lab("PROP EXP", 330, 115, 60);
    ed(5, 395, 113, 70, 20, "4/1/94");
    chk(6, "AGENCY", 480, 110, 80);
    chk(7, "DIRECT", 480, 128, 80);
    lab("PAYMENT", 10, 115, 60);
    ed(8, 70, 113, 50, 20, "TBD");
    lab("AUDIT", 10, 140, 50);
    ed(9, 70, 138, 30, 20, "Y");

    // Rate grid header
    lab("RATE", 10, 175, 40);
    lab("BASE", 80, 175, 50);
    lab("REFERENCE", 200, 175, 80);
    lab("COMMENTS", 380, 175, 80);

    // A few rate rows (original had many empty edit cells)
    for (int row = 0; row < 8; ++row) {
        int y = 195 + row * 24;
        ed(16 + row * 4,     10,  y, 60, 20);
        ed(16 + row * 4 + 1, 80,  y, 110, 20);
        ed(16 + row * 4 + 2, 200, y, 160, 20);
        ed(16 + row * 4 + 3, 380, y, 180, 20);
    }

    // Sample first row
    SetDlgItemTextA(hwnd, 16, "1.25");
    SetDlgItemTextA(hwnd, 17, "CGL Basic");
    SetDlgItemTextA(hwnd, 18, "ISO CG 00 01");
    SetDlgItemTextA(hwnd, 19, "Standard premises");

    CreateWindowA("BUTTON", "Set Aside",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        380, 480, 90, 28, hwnd, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Done",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        480, 480, 90, 28, hwnd, (HMENU)IDCANCEL, g_hInst, nullptr);

    ApplyClassicFontToChildren(hwnd);
}

void BuildUmForm(HWND hwnd) {
    // Historical "Underwriters Manual" from pcaide/um.pvt
    auto lab = [&](const char* t, int x, int y, int w = 80) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 16, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int x, int y, int w, int h, const char* text = "") {
        DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | WS_TABSTOP | ES_AUTOHSCROLL;
        if (h > 24) st |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        CreateWindowA("EDIT", text, st, x, y, w, h,
            hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    lab("ACME", 12, 10, 60);
    lab("Commercial Casualty", 80, 10, 140);
    lab("Volume I", 360, 8, 60);
    lab("MANUAL", 300, 30, 50);
    lab("PROCEDURE", 360, 30, 70);
    lab("PAGE", 450, 30, 40);
    ed(12, 300, 48, 40, 20, "12");
    ed(13, 360, 48, 60, 20, "4.2");
    ed(14, 450, 48, 40, 20, "18");

    lab("TITLE", 12, 80, 40);
    ed(17, 12, 98, 300, 50, "General Liability — Premises and Operations");
    lab("DATE", 330, 80, 40);
    ed(15, 370, 98, 70, 20, "1/15/93");
    lab("REV", 450, 80, 30);
    ed(16, 480, 98, 30, 20, "B");

    CreateWindowA("BUTTON", "Exhibit 1",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        360, 130, 100, 24, hwnd, (HMENU)(intptr_t)11, g_hInst, nullptr);

    ed(10, 12, 165, 480, 280,
        "UNDERWRITING GUIDELINES\r\n"
        "======================\r\n\r\n"
        "1. Premises liability is rated on area and occupancy class.\r\n"
        "2. Products/Completed Operations requires separate submission\r\n"
        "   when receipts exceed $500,000.\r\n"
        "3. Contractual liability — blanket coverage preferred;\r\n"
        "   designated contracts must be scheduled.\r\n\r\n"
        "Refer to ISO circular GL-92-3 for updated loss costs.\r\n\r\n"
        "(Sample text for historical UI resurrection — 1993)");

    CreateWindowA("BUTTON", "Set Aside",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        300, 470, 90, 28, hwnd, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Done",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        400, 470, 90, 28, hwnd, (HMENU)IDCANCEL, g_hInst, nullptr);

    ApplyClassicFontToChildren(hwnd);
}

void BuildAttnForm(HWND hwnd) {
    // Historical "Attention" from pcaide/attn.pvt
    CreateWindowA("EDIT",
        "New submission received for Unlucky Markets — CGL package.\r\n"
        "Please review before the 3pm underwriting desk.",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
        16, 12, 380, 60, hwnd, (HMENU)(intptr_t)3, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Read Now",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        200, 90, 90, 28, hwnd, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Later...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        300, 90, 90, 28, hwnd, (HMENU)IDCANCEL, g_hInst, nullptr);
    ApplyClassicFontToChildren(hwnd);
}

void BuildTabsForm(HWND hwnd) {
    // Historical "Reference Area" from pcaide/tabs.pvt — launcher for related forms
    CreateWindowA("BUTTON", "Rating Instructions",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        20, 20, 160, 28, hwnd, (HMENU)(intptr_t)1, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Underwriters Manual",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        200, 20, 170, 28, hwnd, (HMENU)(intptr_t)2, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Akord Application",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        20, 60, 160, 28, hwnd, (HMENU)(intptr_t)3, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Contractual Profile",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        200, 60, 170, 28, hwnd, (HMENU)(intptr_t)4, g_hInst, nullptr);
    ApplyClassicFontToChildren(hwnd);
}

void BuildCprofForm(HWND hwnd) {
    // Historical "Contractual Profile" from pcaide/cprof.pvt (simplified grid)
    auto lab = [&](const char* t, int x, int y, int w = 100) {
        CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 14, hwnd, nullptr, g_hInst, nullptr);
    };
    auto ed = [&](int id, int x, int y, int w, int h, const char* text = "") {
        CreateWindowA("EDIT", text,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | WS_TABSTOP | ES_AUTOHSCROLL,
            x, y, w, h, hwnd, (HMENU)(intptr_t)id, g_hInst, nullptr);
    };

    lab("ACME", 12, 8, 50);
    lab("Commercial Casualty", 70, 8, 140);
    lab("Volume I", 360, 8, 60);
    lab("MANUAL", 300, 28, 50);
    lab("PROCEDURE", 360, 28, 70);
    lab("PAGE", 450, 28, 40);
    ed(12, 300, 46, 40, 20, "AB14");
    ed(13, 360, 46, 60, 20, "3220");
    ed(14, 450, 46, 40, 20, "Ex1.2");
    lab("DATE", 360, 72, 40);
    ed(15, 400, 70, 80, 20, "10 Feb 93");
    lab("REV", 490, 72, 30);
    ed(16, 520, 70, 30, 20, "00");

    lab("PRODUCER", 12, 72, 70);
    ed(3, 12, 90, 160, 40, "Willy Loman");
    lab("APPLICANT", 12, 140, 70);
    ed(2, 12, 158, 280, 30, "Unlucky Markets");

    lab("CONTRACTUAL PROFILE", 12, 200, 200);
    lab("A. Exposure Prospector - Where May Hold Harmless be Present", 12, 220, 400);

    // Severity headers
    const char* sev[] = { "Mild", "Mod", "Avg", "Sev", "Extr" };
    for (int i = 0; i < 5; ++i)
        lab(sev[i], 300 + i * 44, 240, 40);

    const char* items[] = {
        "1. Freight Handlers Agreements",
        "2. Agreements Required by Municipal Ordinance",
        "3. Cartage Agreements",
        "4. Construction Agreements",
        "5. Easement Agreements (Streets or Sidewalk)",
        "6. Railroad Agreements - Crossings, Rights-of-way"
    };
    for (int r = 0; r < 6; ++r) {
        int y = 260 + r * 28;
        lab(items[r], 20, y, 270);
        for (int c = 0; c < 5; ++c) {
            CreateWindowA("BUTTON", "",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                310 + c * 44, y, 20, 18,
                hwnd, (HMENU)(intptr_t)(100 + r * 5 + c), g_hInst, nullptr);
        }
    }

    CreateWindowA("BUTTON", "Set Aside",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        340, 500, 90, 28, hwnd, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Done",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        440, 500, 90, 28, hwnd, (HMENU)IDCANCEL, g_hInst, nullptr);

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



void BuildDocBrowser(HWND hwnd) {
    g_be.seed();
    CreateWindowA("STATIC", "Documents in mid-end (double-click via Open)",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        12, 10, 380, 16, hwnd, nullptr, g_hInst, nullptr);
    HWND hList = CreateWindowA("LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
        12, 32, 380, 200, hwnd, (HMENU)(intptr_t)9100, g_hInst, nullptr);
    for (auto& kv : g_be.docs) {
        std::string line = kv.second.title;
        if (kv.second.grabbed) line += " [GRABBED]";
        if (kv.second.dirty) line += " *";
        int idx = (int)SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        SendMessageA(hList, LB_SETITEMDATA, idx, (LPARAM)kv.second.id);
    }
    CreateWindowA("BUTTON", "Open / Grab",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        12, 250, 110, 28, hwnd, (HMENU)(intptr_t)9101, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Make Active",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        135, 250, 110, 28, hwnd, (HMENU)(intptr_t)9102, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Info...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        258, 250, 90, 28, hwnd, (HMENU)(intptr_t)9103, g_hInst, nullptr);
    ApplyClassicFontToChildren(hwnd);
}


void BuildLinkBrowser(HWND hwnd) {
    g_be.seed();
    CreateWindowA("STATIC", "Links in mid-end",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        12, 10, 300, 16, hwnd, nullptr, g_hInst, nullptr);
    HWND hList = CreateWindowA("LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
        12, 32, 380, 200, hwnd, (HMENU)(intptr_t)9200, g_hInst, nullptr);
    for (size_t i = 0; i < g_be.links.size(); ++i) {
        auto& L = g_be.links[i];
        std::string line = "#" + std::to_string(L.id) + "  " + L.title;
        int idx = (int)SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        SendMessageA(hList, LB_SETITEMDATA, idx, (LPARAM)i);
    }
    if (g_be.links.empty())
        SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)"(no links — use Start Link / End Link)");
    CreateWindowA("BUTTON", "Info...",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        12, 250, 90, 28, hwnd, (HMENU)(intptr_t)9201, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Open From",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        115, 250, 90, 28, hwnd, (HMENU)(intptr_t)9202, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Open To",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        218, 250, 90, 28, hwnd, (HMENU)(intptr_t)9203, g_hInst, nullptr);
    ApplyClassicFontToChildren(hwnd);
}

void BuildDocForm(HWND hwnd) {
    g_be.seed();
    StubDoc* d = g_be.getActive();
    CreateWindowA("STATIC", d ? d->title.c_str() : "Document",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        8, 6, 480, 16, hwnd, nullptr, g_hInst, nullptr);
    std::string meta;
    if (d) {
        meta = "Owner: " + d->owner + "   Variant: " + std::to_string(d->variant);
        if (d->grabbed) meta += "   [GRABBED]";
        if (d->dirty) meta += " *";
    }
    CreateWindowA("STATIC", meta.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        8, 24, 480, 16, hwnd, nullptr, g_hInst, nullptr);
    CreateWindowA("EDIT", d ? d->content.c_str() : "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_TABSTOP,
        8, 48, 490, 300, hwnd, (HMENU)(intptr_t)9001, g_hInst, nullptr);
    // Store doc id in GWLP_USERDATA high half? type is already there.
    // Keep content edits local; Save reads back.
    ApplyClassicFontToChildren(hwnd);
}


LRESULT CALLBACK FormWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int type = (int)(intptr_t)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        // type was encoded as type+1 so Desktop (0) is not a NULL lpCreateParams
        int encoded = (int)(intptr_t)cs->lpCreateParams;
        type = encoded > 0 ? encoded - 1 : 0;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)type);
        switch (type) {
        case 0: BuildHomeForm(hwnd); break;
        case 1: BuildMemoForm(hwnd); break;
        case 2: BuildRateForm(hwnd); break;
        case 3: BuildAkordForm(hwnd); break;
        case 4: BuildUmForm(hwnd); break;
        case 5: BuildAttnForm(hwnd); break;
        case 6: BuildTabsForm(hwnd); break;
        case 7: BuildCprofForm(hwnd); break;
        case 8: BuildDocForm(hwnd); break;
        case 9: BuildDocBrowser(hwnd); break;
        case 10: BuildLinkBrowser(hwnd); break;
        }
        // Let DefMDIChildProc finish MDI child creation
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (type == 0) {
            if (id == FI_HOME_AKORD) CreateFormWindow("AKORD General Liability Section", 3);
            if (id == FI_HOME_RATE)  CreateFormWindow("Rating Instructions", 2);
            if (id == FI_HOME_MEMO1) CreateFormWindow("Memorandum", 1);
            if (id == FI_HOME_UM)    CreateFormWindow("Underwriters Manual", 4);
            if (id == FI_HOME_ATTN)  CreateFormWindow("Attention", 5);
            if (id == FI_HOME_TABS)  CreateFormWindow("Reference Area", 6);
            if (id == FI_HOME_CPROF) CreateFormWindow("Contractual Profile", 7);
            if (id == FI_HOME_DOCS)  CreateFormWindow("Document Browser", 9);
            if (id == 109) CreateFormWindow("Link Browser", 10);
            if (id >= FI_HOME_DOCBASE && id < FI_HOME_DOCBASE + 32) {
                g_be.seed();
                int n = 0;
                for (auto& kv : g_be.docs) {
                    if (n == id - FI_HOME_DOCBASE) {
                        g_be.activeDoc = kv.second.id;
                        g_be.grab(kv.second.id);
                        CreateFormWindow(kv.second.title.c_str(), 8);
                        RefreshStatus();
                        break;
                    }
                    ++n;
                }
            }
        }
        if (type == 9) {
            HWND hList = GetDlgItem(hwnd, 9100);
            int sel = (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
            int docId = (sel >= 0) ? (int)SendMessageA(hList, LB_GETITEMDATA, sel, 0) : 0;
            if (id == 9101 || (id == 9100 && HIWORD(wParam) == LBN_DBLCLK)) {
                if (docId) {
                    g_be.activeDoc = docId;
                    g_be.grab(docId);
                    StubDoc* d = g_be.get(docId);
                    CreateFormWindow(d ? d->title.c_str() : "Document", 8);
                    RefreshStatus();
                }
            } else if (id == 9102 && docId) {
                g_be.activeDoc = docId;
                RefreshStatus();
                ClassicMessage(hwnd, "Active", "Active document changed.");
            } else if (id == 9103 && docId) {
                g_be.activeDoc = docId;
                ShowDocInfoDialog(hwnd);
            }
        }
        if (type == 10) {
            HWND hList = GetDlgItem(hwnd, 9200);
            int sel = (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
            int idx = (sel >= 0) ? (int)SendMessageA(hList, LB_GETITEMDATA, sel, 0) : -1;
            if (idx >= 0 && idx < (int)g_be.links.size()) {
                StubLink& L = g_be.links[idx];
                if (id == 9201 || (id == 9200 && HIWORD(wParam) == LBN_DBLCLK)) {
                    ShowLinkInfoDialog(hwnd);
                } else if (id == 9202) {
                    g_be.activeDoc = L.fromDoc;
                    g_be.grab(L.fromDoc);
                    StubDoc* d = g_be.get(L.fromDoc);
                    CreateFormWindow(d ? d->title.c_str() : "Document", 8);
                    RefreshStatus();
                } else if (id == 9203) {
                    g_be.activeDoc = L.toDoc;
                    g_be.grab(L.toDoc);
                    StubDoc* d = g_be.get(L.toDoc);
                    CreateFormWindow(d ? d->title.c_str() : "Document", 8);
                    RefreshStatus();
                }
            }
        }
        // Reference Area buttons open other forms
        if (type == 6) {
            if (id == 1) CreateFormWindow("Rating Instructions", 2);
            if (id == 2) CreateFormWindow("Underwriters Manual", 4);
            if (id == 3) CreateFormWindow("AKORD General Liability Section", 3);
            if (id == 4) CreateFormWindow("Contractual Profile", 7);
        }
        if (id == FI_MEMO_REFERENCE) {
            ShowLinkInfoDialog(hwnd);
        }
        if (type == 8 && id == 9001 && HIWORD(wParam) == EN_CHANGE) {
            if (StubDoc* d = g_be.getActive()) {
                d->dirty = true;
                RefreshStatus();
            }
        }
        return 0;
    }
    case WM_CLOSE:
        if (type == 8) {
            char buf[8192];
            GetDlgItemTextA(hwnd, 9001, buf, sizeof(buf));
            if (StubDoc* d = g_be.getActive()) {
                d->content = buf;
                d->dirty = true;
            }
            RefreshStatus();
        }
        if (g_hClient)
            SendMessageA(g_hClient, WM_MDIDESTROY, (WPARAM)hwnd, 0);
        else
            DestroyWindow(hwnd);
        return 0;
    }
    return DefMDIChildProcA(hwnd, msg, wParam, lParam);
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
    AppendMenuA(hDoc, MF_STRING, 251,            "N&ew document");
    AppendMenuA(hDoc, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hDoc, MF_STRING, CM_INFO,        "&Info");
    AppendMenuA(hLinks, MF_STRING, CM_LINKS,     "&Links");
    AppendMenuA(hLinks, MF_STRING, CM_STARTLINK, "&Start Link");
    AppendMenuA(hLinks, MF_STRING, CM_ENDLINK,   "End &Link");
    AppendMenuA(hLinks, MF_STRING, CM_ATTACHMENT,"&Attachment");
    AppendMenuA(hLinks, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hLinks, MF_STRING, 252,          "Link &Browser...");
    AppendMenuA(hDoc, MF_POPUP, (UINT_PTR)hLinks, "&Links");
    AppendMenuA(hDoc, MF_STRING, CM_DELETE,      "D&elete");
    AppendMenuA(hDoc, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hDoc, MF_STRING, CM_SAVE,        "Save All");
    AppendMenuA(hDoc, MF_STRING, CM_RETURNHOME,  "Return &Home");
    AppendMenuA(hDoc, MF_STRING, 250,            "Document &Browser...");
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

    HMENU hHelp = CreatePopupMenu();
    AppendMenuA(hHelp, MF_STRING, 253, "&About winfe resurrection...");

    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hDoc,  "&Document");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hEdit, "&Edit");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hWin,  "&Window");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hHelp, "&Help");
    return hMenu;
}


// Simple find in active document content (stub)
bool FindInActiveDoc(bool again) {
    StubDoc* d = g_be.getActive();
    if (!d || g_be.lastFind.empty()) return false;
    const std::string& hay = d->content;
    size_t start = again ? (g_be.lastFindPos + 1) : 0;
    size_t pos = hay.find(g_be.lastFind, start);
    if (pos == std::string::npos && again) {
        pos = hay.find(g_be.lastFind, 0); // wrap
    }
    if (pos == std::string::npos) return false;
    g_be.lastFindPos = pos;
    // Highlight in open doc window if present
    if (g_hClient) {
        HWND active = (HWND)SendMessageA(g_hClient, WM_MDIGETACTIVE, 0, 0);
        HWND edit = active ? GetDlgItem(active, 9001) : nullptr;
        if (edit) {
            SendMessageA(edit, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + g_be.lastFind.size()));
            SendMessageA(edit, EM_SCROLLCARET, 0, 0);
            SetFocus(edit);
        }
    }
    return true;
}

int ShowFindDialog(HWND parent) {
    HWND hDlg = BeginModalDialog(parent, "Find", 360, 130);
    CreateWindowA("STATIC", "Find what:", WS_CHILD | WS_VISIBLE,
        16, 20, 70, 16, hDlg, nullptr, g_hInst, nullptr);
    CreateWindowA("EDIT", g_be.lastFind.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        90, 18, 240, 22, hDlg, (HMENU)(intptr_t)9300, g_hInst, nullptr);
    CreateWindowA("BUTTON", "&Find Next",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        150, 60, 90, 28, hDlg, (HMENU)IDOK, g_hInst, nullptr);
    CreateWindowA("BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        250, 60, 80, 28, hDlg, (HMENU)IDCANCEL, g_hInst, nullptr);
    ApplyClassicFontToChildren(hDlg);
    // Custom loop to read edit before destroy
    EnableWindow(parent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    MSG msg;
    int code = IDCANCEL;
    char buf[256] = {};
    while (IsWindow(hDlg) && GetMessageA(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDOK) {
            GetDlgItemTextA(hDlg, 9300, buf, sizeof(buf));
            code = IDOK;
            DestroyWindow(hDlg);
            break;
        }
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDCANCEL) {
            code = IDCANCEL;
            DestroyWindow(hDlg);
            break;
        }
        if (!IsDialogMessageA(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
    if (code == IDOK && buf[0]) {
        g_be.lastFind = buf;
        g_be.lastFindPos = 0;
        if (!FindInActiveDoc(false))
            ClassicMessage(parent, "Find", "Text not found.");
    }
    return code;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hFont = CreateClassicFont(-12, false);

        // MDI client fills the frame (above status bar)
        CLIENTCREATESTRUCT ccs = {};
        // Window menu is the 3rd top-level popup (Document=0, Edit=1, Window=2)
        HMENU hMenu = GetMenu(hwnd);
        ccs.hWindowMenu  = hMenu ? GetSubMenu(hMenu, 2) : nullptr;
        ccs.idFirstChild = 5000;
        g_hClient = CreateWindowExA(WS_EX_CLIENTEDGE, "MDICLIENT", nullptr,
            WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE,
            0, 0, 0, 0, hwnd, (HMENU)(intptr_t)1, g_hInst, &ccs);

        g_hStatus = CreateWindowA("STATIC",
            " Udanax Gold Frontend  —  offline  |  stub mid-end",
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
        const int statusH = 22;
        if (g_hClient)
            MoveWindow(g_hClient, 0, 0, rc.right, rc.bottom - statusH, TRUE);
        if (g_hStatus)
            MoveWindow(g_hStatus, 0, rc.bottom - statusH, rc.right, statusH, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case CM_EXIT:
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
            break;
        case CM_RETURNHOME:
            CreateFormWindow("Desktop", 0);
            RefreshStatus();
            break;
        case 250:
            CreateFormWindow("Document Browser", 9);
            break;
        case 251: {
            g_be.seed();
            int idNew = g_be.createDoc("Untitled Document");
            StubDoc* d = g_be.get(idNew);
            CreateFormWindow(d->title.c_str(), 8);
            RefreshStatus();
            break;
        }
        case 252:
            CreateFormWindow("Link Browser", 10);
            break;
        case 253:
            ClassicMessage(hwnd, "About winfe resurrection",
                "Udanax Gold Windows frontend — visual resurrection\n"
                "Original: 1993 Xanadu / Udanax Gold winfe + pcaide\n"
                "Forms from fe.rc and pcaide/*.pvt\n\n"
                "In-memory stub mid-end only.\n"
                "Historical effort — not a live FeBe client.");
            break;
        case CM_INITIALIZE:
            g_be = Backend();
            g_be.connected = true;
            g_be.seed();
            CloseAllChildren();
            CreateFormWindow("Desktop", 0);
            ClassicMessage(hwnd, "Initialize", "Mid-end reinitialized with seed documents.");
            RefreshStatus();
            break;
        case CM_INFO:
            ShowDocInfoDialog(hwnd);
            break;
        case CM_LINKS:
            CreateFormWindow("Link Browser", 10);
            break;
        case CM_STARTLINK: {
            g_be.seed();
            int from = g_be.startLink();
            if (from)
                ClassicMessage(hwnd, "Start Link", "Link start anchored on active document.\nSelect another document and use End Link.");
            else
                ClassicMessage(hwnd, "Start Link", "No active document.");
            RefreshStatus();
            break;
        }
        case CM_ENDLINK: {
            g_be.seed();
            int lid = g_be.endLink();
            if (lid) {
                char buf[128];
                sprintf(buf, "Link #%d created.\n%s", lid, g_be.links.back().title.c_str());
                ClassicMessage(hwnd, "End Link", buf);
            } else {
                ClassicMessage(hwnd, "End Link", "Need Start Link first, then a different active document.");
            }
            RefreshStatus();
            break;
        }
        case CM_ATTACHMENT:
            ShowLinkInfoDialog(hwnd);
            break;
        case CM_GRAB: {
            g_be.seed();
            if (g_be.grab(g_be.activeDoc)) {
                StubDoc* d = g_be.getActive();
                char buf[256];
                sprintf(buf, "Grabbed \"%s\" for editing.", d ? d->title.c_str() : "?");
                ClassicMessage(hwnd, "Grab", buf);
                CreateFormWindow(d ? d->title.c_str() : "Document", 8);
            } else {
                ClassicMessage(hwnd, "Grab", "Could not grab document (held by another editor?).");
            }
            RefreshStatus();
            break;
        }
        case CM_RELEASE: {
            g_be.seed();
            if (g_be.release(g_be.activeDoc))
                ClassicMessage(hwnd, "Release", "Document released.");
            else
                ClassicMessage(hwnd, "Release", "Document was not grabbed.");
            RefreshStatus();
            break;
        }
        case CM_NEWVARIANT: {
            g_be.seed();
            if (g_be.newVariant(g_be.activeDoc)) {
                StubDoc* d = g_be.getActive();
                char buf[256];
                sprintf(buf, "Created variant %d:\n%s", d->variant, d->title.c_str());
                ClassicMessage(hwnd, "New variant", buf);
                CreateFormWindow(d->title.c_str(), 8);
            } else {
                ClassicMessage(hwnd, "New variant", "No active document.");
            }
            RefreshStatus();
            break;
        }
        case CM_SAVE: {
            g_be.seed();
            // Pull content from active MDI child if it is a document form
            if (g_hClient) {
                HWND active = (HWND)SendMessageA(g_hClient, WM_MDIGETACTIVE, 0, 0);
                if (active) {
                    HWND edit = GetDlgItem(active, 9001);
                    if (edit) {
                        char buf[8192];
                        GetWindowTextA(edit, buf, sizeof(buf));
                        if (StubDoc* d = g_be.getActive()) {
                            d->content = buf;
                        }
                    }
                }
            }
            if (g_be.save(g_be.activeDoc))
                ClassicMessage(hwnd, "Save All", "Active document saved (in-memory).");
            else
                ClassicMessage(hwnd, "Save All", "Nothing to save.");
            RefreshStatus();
            break;
        }
        case CM_DELETE: {
            g_be.seed();
            StubDoc* d = g_be.getActive();
            if (!d) {
                ClassicMessage(hwnd, "Delete", "No active document.");
            } else if (MessageBoxA(hwnd, ("Delete \"" + d->title + "\"?").c_str(),
                                   "Delete", MB_YESNO | MB_ICONWARNING) == IDYES) {
                g_be.remove(d->id);
                ClassicMessage(hwnd, "Delete", "Document removed from mid-end.");
            }
            RefreshStatus();
            break;
        }
        case CM_TESTCLUBS:
            ClassicMessage(hwnd, "Test Clubs",
                "Club permissions stub.\n"
                "Private / Club / Public levels are stored on each document.");
            break;
        case CM_EDITUNDO: case CM_EDITCUT: case CM_EDITCOPY:
        case CM_EDITPASTE: case CM_EDITDELETE:
            ClassicMessage(hwnd, "Edit", "Use the document window edit control (Ctrl+X/C/V).");
            break;
        case CM_INSERTFILE: case CM_COPYTOFILE:
            ClassicMessage(hwnd, "Edit", "File insert/export not implemented in stub.");
            break;
        case CM_EDITFIND:
            ShowFindDialog(hwnd);
            break;
        case CM_EDITFINDNEXT:
            if (g_be.lastFind.empty())
                ShowFindDialog(hwnd);
            else if (!FindInActiveDoc(true))
                ClassicMessage(hwnd, "Find", "Text not found.");
            break;
        case CM_EDITREPLACE:
            ClassicMessage(hwnd, "Replace", "Replace not implemented in stub.\nUse Find + manual edit.");
            break;
        case CM_CASCADECHILDREN:
            CascadeChildren();
            break;
        case CM_TILECHILDREN:
            TileChildren();
            break;
        case CM_ARRANGEICONS:
            ArrangeIcons();
            break;
        case CM_CLOSECHILDREN:
            CloseAllChildren();
            break;
        default:
            // Forward unhandled commands to the active MDI child
            if (g_hClient) {
                HWND active = (HWND)SendMessageA(g_hClient, WM_MDIGETACTIVE, 0, 0);
                if (active) SendMessageA(active, WM_COMMAND, wParam, lParam);
            }
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
    return DefFrameProcA(hwnd, g_hClient, msg, wParam, lParam);
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

    // Accelerators matching original fe.rc COMMANDS_ACC
    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'X', CM_EDITCUT },
        { FVIRTKEY | FCONTROL, 'C', CM_EDITCOPY },
        { FVIRTKEY | FCONTROL, 'V', CM_EDITPASTE },
        { FVIRTKEY,            VK_DELETE, CM_EDITDELETE },
        { FVIRTKEY | FCONTROL, VK_DELETE, CM_EDITCLEAR },
        { FVIRTKEY | FCONTROL, 'Z', CM_EDITUNDO },
        { FVIRTKEY | FCONTROL, 'F', CM_EDITFIND },
        { FVIRTKEY | FCONTROL, 'G', CM_EDITFINDNEXT },
        { FVIRTKEY | FCONTROL, 'R', CM_EDITREPLACE },
    };
    HACCEL hAccel = CreateAcceleratorTableA(accels, (int)(sizeof(accels) / sizeof(accels[0])));

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        if (!TranslateMDISysAccel(g_hClient, &msg) &&
            !TranslateAcceleratorA(g_hMain, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    if (hAccel) DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}
