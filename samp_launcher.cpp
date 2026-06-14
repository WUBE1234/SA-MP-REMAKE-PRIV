#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

#define IDC_SERVER_LIST 201
#define IDC_CONNECT_BTN 202
#define IDC_ADD_SERVER_BTN 203
#define IDC_LANG_PL 204
#define IDC_LANG_EN 205

HWND hServerList, hNickInput, hConnectBtn, hAddServerBtn, hVersionLbl, hNameStatic, hBtnPl, hBtnEn;
HFONT hFont;

struct ServerInfo {
    std::string name; std::string players; std::string ping; std::string address;
};

std::vector<ServerInfo> g_Servers = {
    {"Gaming Side", "142 / 500", "22 ms", "83.168.69.6:4000"},
    {"Serwer Pomocniczy", "12 / 100", "19 ms", "samp.pomocnik-samp.pl:7777"}
};

bool g_IsEnglish = false;

void SaveHistoryToRegistry() {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SAMP\\CustomHistory", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        std::string historyStr = "";
        for (size_t i = 2; i < g_Servers.size(); ++i) {
            historyStr += g_Servers[i].address + ";";
        }
        RegSetValueExA(hKey, "PlayedIPs", 0, REG_SZ, (const BYTE*)historyStr.c_str(), (DWORD)(historyStr.length() + 1));
        RegCloseKey(hKey);
    }
}

void LoadHistoryFromRegistry() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SAMP\\CustomHistory", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buf[2048] = { 0 }; DWORD size = sizeof(buf);
        if (RegQueryValueExA(hKey, "PlayedIPs", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
            std::stringstream ss(buf); std::string token;
            while (std::getline(ss, token, ';')) {
                if (token.empty()) continue;
                bool duplicate = false;
                for (const auto& srv : g_Servers) { if (srv.address == token) { duplicate = true; break; } }
                if (!duplicate) {
                    g_Servers.push_back({ "Ostatnio Grany / History", "Online", "---", token });
                }
            }
        }
        RegCloseKey(hKey);
    }
}

std::string GetSAMPPlayerName() {
    HKEY hKey; char nameBuf[MAX_PATH] = { 0 }; DWORD size = sizeof(nameBuf);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "PlayerName", NULL, NULL, (LPBYTE)nameBuf, &size); RegCloseKey(hKey);
    }
    return std::string(nameBuf);
}

void SaveSAMPPlayerName(const std::string& name) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "PlayerName", 0, REG_SZ, (const BYTE*)name.c_str(), (DWORD)(name.length() + 1));
        RegCloseKey(hKey);
    }
}

void RefreshListView() {
    ListView_DeleteAllItems(hServerList);
    for (size_t i = 0; i < g_Servers.size(); ++i) {
        LVITEMA lvi = { LVIF_TEXT, (int)i, 0, 0, 0, (LPSTR)g_Servers[i].name.c_str() };
        ListView_InsertItem(hServerList, &lvi);
        ListView_SetItemText(hServerList, (int)i, 1, (LPSTR)g_Servers[i].players.c_str());
        ListView_SetItemText(hServerList, (int)i, 2, (LPSTR)g_Servers[i].ping.c_str());
        ListView_SetItemText(hServerList, (int)i, 3, (LPSTR)g_Servers[i].address.c_str());
    }
}

void TranslateGUI() {
    LVCOLUMNA lvc = { 0 }; lvc.mask = LVCF_TEXT;
    if (g_IsEnglish) {
        SetWindowTextA(hConnectBtn, "Connect"); SetWindowTextA(hAddServerBtn, "Quick Connect");
        SetWindowTextA(hVersionLbl, "Death & Kalcor & Spookie       Supported versions: 0.3.7/0.3DL");
        lvc.pszText = (LPSTR)"HostName"; ListView_SetColumn(hServerList, 0, &lvc);
        lvc.pszText = (LPSTR)"Players";  ListView_SetColumn(hServerList, 1, &lvc);
        lvc.pszText = (LPSTR)"Mode / Internet Address"; ListView_SetColumn(hServerList, 3, &lvc);
    }
    else {
        SetWindowTextA(hConnectBtn, "Connect"); SetWindowTextA(hAddServerBtn, "Szybkie Polaczenie");
        SetWindowTextA(hVersionLbl, "Death & Kalcor & Spookie       Wspierane wersje: 0.3.7/0.3DL");
        lvc.pszText = (LPSTR)"Nazwa Serwera"; ListView_SetColumn(hServerList, 0, &lvc);
        lvc.pszText = (LPSTR)"Gracze";        ListView_SetColumn(hServerList, 1, &lvc);
        lvc.pszText = (LPSTR)"Tryb / Adres Internetowy"; ListView_SetColumn(hServerList, 3, &lvc);
    }
}

bool LaunchSAMP(HWND hwnd, const std::string& address, const std::string& name) {
    // KROK 1: Bezwarunkowy zapis wybranego Nicku bezpośrednio do rejestru komputera
    SaveSAMPPlayerName(name);

    PROCESS_INFORMATION pi; STARTUPINFO si; memset(&pi, 0, sizeof(pi)); memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    char exe[MAX_PATH] = { 0 }, path[MAX_PATH] = { 0 }; DWORD b = sizeof(exe); HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return false;
    RegQueryValueExA(hKey, "gta_sa_exe", NULL, NULL, (LPBYTE)&exe, &b); RegCloseKey(hKey);
    strcpy_s(path, sizeof(path), exe); path[strlen(path) - 11] = '\0';
    size_t colon = address.find(':'); if (colon == std::string::npos) return false;

    // KROK 2: KOMPLETNE WYCZYSZCZENIE BŁĘDNYCH FLAG. Zostaje tylko czysty parametr wejściowy dla SA-MP.
    char cmd[MAX_PATH];
    sprintf_s(cmd, sizeof(cmd), "-c -h %s -p %s -n %s -fpslimit 100 -mem 2048", address.substr(0, colon).c_str(), address.substr(colon + 1).c_str(), name.c_str());

    DWORD creationFlags = DETACHED_PROCESS | CREATE_SUSPENDED | ABOVE_NORMAL_PRIORITY_CLASS;

    if (CreateProcessA(exe, cmd, NULL, NULL, FALSE, creationFlags, NULL, path, &si, &pi)) {
        char dll[MAX_PATH]; sprintf_s(dll, sizeof(dll), "%s\\samp.dll", path);
        void* addr = (void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
        void* arg = (void*)VirtualAllocEx(pi.hProcess, NULL, strlen(dll) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        WriteProcessMemory(pi.hProcess, arg, dll, strlen(dll) + 1, NULL);
        HANDLE id = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)addr, arg, 0, NULL);
        if (id) { WaitForSingleObject(id, INFINITE); CloseHandle(id); }
        VirtualFreeEx(pi.hProcess, arg, 0, MEM_RELEASE);

        ResumeThread(pi.hThread); ShowWindow(hwnd, SW_HIDE);
        WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        ShowWindow(hwnd, SW_SHOW); SetActiveWindow(hwnd); SetForegroundWindow(hwnd); return true;
    } return false;
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hEdit;
    if (msg == WM_CREATE) {
        CreateWindowA("STATIC", g_IsEnglish ? "Enter Server IP and Port:" : "Wpisz IP i Port serwera:", WS_VISIBLE | WS_CHILD, 15, 15, 300, 20, hwnd, NULL, NULL, NULL);
        hEdit = CreateWindowA("EDIT", "83.168.69.6:4000", WS_VISIBLE | WS_CHILD | WS_BORDER, 15, 40, 305, 24, hwnd, NULL, NULL, NULL);
        HWND hOk = CreateWindowA("BUTTON", "OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 115, 80, 100, 30, hwnd, (HMENU)IDOK, NULL, NULL);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(hOk, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        char buf[MAX_PATH] = { 0 }; char nBuf[MAX_PATH] = { 0 };
        GetWindowTextA(hEdit, buf, sizeof(buf)); GetWindowTextA(hNickInput, nBuf, sizeof(nBuf));
        std::string input(buf), name(nBuf);

        if (name.empty() || name.find_first_not_of(' ') == std::string::npos) {
            MessageBoxA(hwnd, g_IsEnglish ? "Enter Nick first!" : "Najpierw wpisz Nick!", "Death & Kalcor & Spookie", MB_ICONWARNING);
            return 0;
        }
        if (input.find(':') != std::string::npos) {
            DestroyWindow(hwnd);
            bool exists = false;
            for (const auto& srv : g_Servers) { if (srv.address == input) { exists = true; break; } }
            if (!exists) {
                g_Servers.push_back({ "Ostatnio Grany / History", "Online", "---", input });
                SaveHistoryToRegistry(); RefreshListView();
            }
            LaunchSAMP(GetParent(hwnd), input, name);
        }
        else {
            MessageBoxA(hwnd, g_IsEnglish ? "Invalid format! Use IP:PORT" : "Zly format! Uzyj wzoru IP:PORT", "Death & Kalcor & Spookie", MB_ICONWARNING);
        }
    }
    if (msg == WM_CLOSE) DestroyWindow(hwnd);
    return DefWindowProcA(hwnd, msg, wp, lp);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp) {
    if (uMsg == WM_CREATE) {
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES }; InitCommonControlsEx(&icex);
        hFont = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

        hConnectBtn = CreateWindowA("BUTTON", "Connect", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 90, 30, hwnd, (HMENU)IDC_CONNECT_BTN, NULL, NULL);
        hAddServerBtn = CreateWindowA("BUTTON", "Szybkie Polaczenie", WS_VISIBLE | WS_CHILD, 105, 10, 140, 30, hwnd, (HMENU)IDC_ADD_SERVER_BTN, NULL, NULL);
        hNameStatic = CreateWindowA("STATIC", "Name:", WS_VISIBLE | WS_CHILD, 460, 15, 50, 20, hwnd, NULL, NULL, NULL);

        // Czysty start pola tekstowego (pobiera aktualnie przypisany w systemie nick w celach weryfikacji)
        hNickInput = CreateWindowA("EDIT", GetSAMPPlayerName().c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER, 510, 13, 140, 24, hwnd, NULL, NULL, NULL);

        hServerList = CreateWindowA("SysListView32", "", WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL, 10, 50, 640, 300, hwnd, (HMENU)IDC_SERVER_LIST, NULL, NULL);
        ListView_SetExtendedListViewStyle(hServerList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNA lvc = { LVCF_TEXT | LVCF_WIDTH, 0, 240, (LPSTR)"Nazwa Serwera" }; ListView_InsertColumn(hServerList, 0, &lvc);
        lvc.cx = 80; lvc.pszText = (LPSTR)"Gracze"; ListView_InsertColumn(hServerList, 1, &lvc);
        lvc.cx = 60; lvc.pszText = (LPSTR)"Ping"; ListView_InsertColumn(hServerList, 2, &lvc);lvc.cx = 240; lvc.pszText = (LPSTR)"Tryb / Adres Internetowy"; ListView_InsertColumn(hServerList, 3, &lvc);
        LoadHistoryFromRegistry(); RefreshListView();hVersionLbl = CreateWindowA("STATIC", "Death & Kalcor & Spookie       Wspierane wersje: 0.3.7/0.3DL", WS_VISIBLE | WS_CHILD, 15, 365, 450, 20, hwnd, NULL, NULL, NULL);hBtnPl = CreateWindowA("BUTTON", "PL", WS_VISIBLE | WS_CHILD, 595, 365, 25, 22, hwnd, (HMENU)IDC_LANG_PL, NULL, NULL);hBtnEn = CreateWindowA("BUTTON", "EN", WS_VISIBLE | WS_CHILD, 625, 365, 25, 22, hwnd, (HMENU)IDC_LANG_EN, NULL, NULL);EnumChildWindows(hwnd, [](HWND c, LPARAM f) { SendMessage(c, WM_SETFONT, f, TRUE); return TRUE; }, (LPARAM)hFont);
    }if (uMsg == WM_COMMAND) { if (LOWORD(wp) == IDC_CONNECT_BTN) { int idx = ListView_GetNextItem(hServerList, -1, LVNI_SELECTED);if (idx != -1) { char nBuf[MAX_PATH] = { 0 }; GetWindowTextA(hNickInput, nBuf, sizeof(nBuf)); std::string name = nBuf;if (name.empty() || name.find_first_not_of(' ') == std::string::npos) { MessageBoxA(hwnd, g_IsEnglish ? "Enter Nick first!" : "Najpierw wpisz Nick!", "Death & Kalcor & Spookie", MB_ICONWARNING);return 0; }LaunchSAMP(hwnd, g_Servers[idx].address, name); } }if (LOWORD(wp) == IDC_ADD_SERVER_BTN) { WNDCLASSA wca = { 0, DialogProc, 0, 0, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_3DFACE + 1), NULL, "PopupUI" };RegisterClassA(&wca);HWND hPopup = CreateWindowExA(0, "PopupUI", g_IsEnglish ? "Quick Join" : "Szybkie Dolaczenie", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 350, 160, hwnd, NULL, GetModuleHandle(NULL), NULL);ShowWindow(hPopup, SW_SHOW); }if (LOWORD(wp) == IDC_LANG_PL) { g_IsEnglish = false; TranslateGUI(); }if (LOWORD(wp) == IDC_LANG_EN) { g_IsEnglish = true; TranslateGUI(); } }if (uMsg == WM_NOTIFY) { LPNMHDR lpnm = (LPNMHDR)lp;if (lpnm->idFrom == IDC_SERVER_LIST && lpnm->code == NM_RCLICK) { int idx = ListView_GetNextItem(hServerList, -1, LVNI_SELECTED);if (idx >= 2) { g_Servers.erase(g_Servers.begin() + idx);SaveHistoryToRegistry(); RefreshListView(); } else if (idx != -1) { MessageBoxA(hwnd, g_IsEnglish ? "You cannot delete base servers!" : "Nie mozesz usunac podstawowych serwerow!", "Death & Kalcor & Spookie", MB_ICONINFORMATION); } } }if (uMsg == WM_DESTROY) PostQuitMessage(0);return DefWindowProcA(hwnd, uMsg, wp, lp);
}int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) { WNDCLASSA wc = { 0, WindowProc, 0, 0, hInst, NULL, LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_3DFACE + 1), NULL, "SAMP_Final_Remake_V23" };RegisterClassA(&wc);RECT wr = { 0, 0, 660, 390 }; AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);HWND hwnd = CreateWindowExA(0, "SAMP_Final_Remake_V23", "SA-MP 0.3DL remake", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInst, NULL);ShowWindow(hwnd, nShow); MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }return 0; }