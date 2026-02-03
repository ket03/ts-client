#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <teamspeak/public_definitions.h>
#include <teamspeak/public_errors.h>
#include <teamspeak/clientlib.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

// Theme colors (easy to modify)
#define CLR_BG          RGB(30, 30, 30)
#define CLR_BG_LIGHT    RGB(45, 45, 45)
#define CLR_TEXT        RGB(220, 220, 220)
#define CLR_TEXT_DIM    RGB(160, 160, 160)
#define CLR_ACCENT      RGB(0, 120, 212)
#define CLR_ACCENT_HOV  RGB(30, 140, 230)
#define CLR_BTN_BG      RGB(55, 55, 55)
#define CLR_BTN_HOV     RGB(70, 70, 70)
#define CLR_BORDER      RGB(80, 80, 80)

enum {
    ID_LOG_LIST = 101,
    ID_CHANNEL_LIST,
    ID_BTN_CREATE,
    ID_BTN_MOVE,
    ID_BTN_MUTE,
    ID_EDIT_CH_NAME,
    ID_EDIT_CH_PASS,
    ID_EDIT_MOVE_ID,
    ID_EDIT_MOVE_PASS
};

HWND g_hWnd, g_hLogList, g_hChannelList, g_hBtnMute;
HWND g_hEditChName, g_hEditChPass, g_hEditMoveID, g_hEditMovePass;
HWND g_hBtnCreate, g_hBtnMove;
uint64 g_scHandlerID;
char* g_identity;
int g_isMuted;
int g_ts3Initialized;
char g_pendingChannelPassword[32];

HFONT g_hFont;
HBRUSH g_hBrushBg, g_hBrushEdit;
WNDPROC g_OrigEditProc;

// Forward declarations
void OnCreateChannel();
void OnMoveToChannel();

const char* animals[] = {
    "Wolf", "Fox", "Bear", "Eagle", "Hawk", "Falcon", "Tiger", "Lion",
    "Panther", "Jaguar", "Leopard", "Cobra", "Viper", "Python", "Dragon",
    "Phoenix", "Griffin", "Raven", "Crow", "Owl", "Shark", "Whale"
};
const char* colors[] = {
    "Red", "Blue", "Green", "Yellow", "Orange", "Purple", "Violet",
    "Crimson", "Scarlet", "Ruby", "Coral", "Amber", "Gold", "Bronze"
};

void LogMessage(const char* msg) {
    if (!g_hLogList) return;

    int count = (int)SendMessageA(g_hLogList, LB_GETCOUNT, 0, 0);
    if (count >= 100) {
        SendMessageA(g_hLogList, LB_DELETESTRING, 0, 0);
    }
    SendMessageA(g_hLogList, LB_ADDSTRING, 0, (LPARAM)msg);
    count = (int)SendMessageA(g_hLogList, LB_GETCOUNT, 0, 0);
    SendMessageA(g_hLogList, LB_SETTOPINDEX, count - 1, 0);
}

void LogFormat(const char* fmt, ...) {
    char buf[100];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    LogMessage(buf);
}

void RefreshChannelList() {
    uint64* channels = NULL;

    SendMessageA(g_hChannelList, LB_RESETCONTENT, 0, 0);
    if (ts3client_getChannelList(g_scHandlerID, &channels) != ERROR_ok || !channels)
        return;

    for (int i = 0; channels[i]; i++) {
        char* name = NULL;
        char buf[128];
        if (ts3client_getChannelVariableAsString(g_scHandlerID, channels[i], CHANNEL_NAME, &name) == ERROR_ok && name) {
            snprintf(buf, sizeof(buf), "%llu: %s", channels[i], name);
            ts3client_freeMemory(name);
        } else snprintf(buf, sizeof(buf), "%llu", channels[i]);
        SendMessageA(g_hChannelList, LB_ADDSTRING, 0, (LPARAM)buf);
    }
    ts3client_freeMemory(channels);
}

int load_env(const char* filename, char** ip, unsigned int* port, char** password) {
    FILE* file = fopen(filename, "r");
    if (!file) return 0;

    char* port_str = NULL;
    char line[128];

    *ip = NULL;
    *password = NULL;
    *port = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        const char* key = line;
        const char* value = eq + 1;

        if (strcmp(key, "IP") == 0) {
            free(*ip);
            *ip = _strdup(value);
        } else if (strcmp(key, "PORT") == 0) {
            free(port_str);
            port_str = _strdup(value);
        } else if (strcmp(key, "PASSWORD") == 0) {
            free(*password);
            *password = _strdup(value);
        }
    }
    fclose(file);

    if (port_str) {
        *port = (unsigned int)atoi(port_str);
        free(port_str);
    }

    if (*ip && *port && *password)
        return 1;

    free(*ip); *ip = NULL;
    free(*password); *password = NULL;
    return 0;
}

void programPath(char* out, size_t size) {
    GetModuleFileNameA(NULL, out, (DWORD)size);
    char* lastSlash = strrchr(out, '\\');
    if (lastSlash) *lastSlash = '\0';
}

void generate_nickname(char* buffer, size_t size) {
    srand(time(NULL));
    const int num_animals = sizeof(animals) / sizeof(animals[0]);
    const int num_colors = sizeof(colors) / sizeof(colors[0]);
    snprintf(buffer, size, "%s %s", colors[rand() % num_colors], animals[rand() % num_animals]);
}

void onConnectStatusChangeEvent(uint64 scHandlerID, int newStatus, unsigned int errorNumber) {
    if (newStatus == STATUS_CONNECTION_ESTABLISHED) {
        LogMessage("Connected to server");
        RefreshChannelList();
    } else if (newStatus == STATUS_DISCONNECTED) {
        LogMessage("Disconnected from server");
    }
}

void onTalkStatusChangeEvent(uint64 scHandlerID, int status, int isReceivedWhisper, anyID clientID) {
    char* name = NULL;
    if (ts3client_getClientVariableAsString(scHandlerID, clientID, CLIENT_NICKNAME, &name) == ERROR_ok && name) {
        LogFormat("%s %s", name, status == STATUS_TALKING ? "is talking" : "stopped talking");
        ts3client_freeMemory(name);
    }
}

void onClientMoveEvent(uint64 scHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
    LogFormat("Client moved from %llu to %llu", oldChannelID, newChannelID);
    RefreshChannelList();
}

void onNewChannelCreatedEvent(uint64 scHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
    LogFormat("User %s created channel %llu", invokerName ? invokerName : "Unknown", channelID);
    g_pendingChannelPassword[0] = '\0';
    RefreshChannelList();
}

void onDelChannelEvent(uint64 scHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
    LogFormat("Channel %llu deleted by %s", channelID, invokerName ? invokerName : "Unknown");
    RefreshChannelList();
}

void onServerErrorEvent(uint64 scHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
    switch (error) {
        case ERROR_ok: break;
        case ERROR_channel_invalid_password:
            LogMessage("Error: Invalid channel password");
            break;
        case ERROR_channel_invalid_id:
            LogMessage("Error: Invalid channel ID");
            break;
        default:
            LogFormat("Server error: %s (%u)", errorMessage ? errorMessage : "Unknown", error);
            break;
    }
}

unsigned int doCreateChannel(const char* name, const char* password) {
    ts3client_setChannelVariableAsString(g_scHandlerID, 0, CHANNEL_NAME, name);
    ts3client_setChannelVariableAsString(g_scHandlerID, 0, CHANNEL_PASSWORD, password);
    ts3client_setChannelVariableAsInt(g_scHandlerID, 0, CHANNEL_CODEC, CODEC_OPUS_VOICE);
    ts3client_setChannelVariableAsInt(g_scHandlerID, 0, CHANNEL_CODEC_QUALITY, 10);
    ts3client_setChannelVariableAsInt(g_scHandlerID, 0, CHANNEL_MAXCLIENTS, 4);
    return ts3client_flushChannelCreation(g_scHandlerID, 0, NULL);
}

void OnCreateChannel() {
    char name[32], pass[32];
    GetWindowTextA(g_hEditChName, name, sizeof(name));
    GetWindowTextA(g_hEditChPass, pass, sizeof(pass));

    if (name[0] == '\0' || pass[0] == '\0') {
        LogMessage("Name and password are required");
        return;
    }

    strncpy(g_pendingChannelPassword, pass, sizeof(g_pendingChannelPassword) - 1);
    g_pendingChannelPassword[sizeof(g_pendingChannelPassword) - 1] = '\0';

    if (doCreateChannel(name, pass) == ERROR_ok) {
        LogFormat("Creating channel '%s'...", name);
        SetWindowTextA(g_hEditChName, "");
        SetWindowTextA(g_hEditChPass, "");
    } else {
        g_pendingChannelPassword[0] = '\0';
        LogMessage("Error creating channel");
    }
}

int ChannelExists(uint64 channelID) {
    uint64* channels = NULL;
    if (ts3client_getChannelList(g_scHandlerID, &channels) != ERROR_ok || !channels)
        return 0;

    int exists = 0;
    for (int i = 0; channels[i]; i++) {
        if (channels[i] == channelID) {
            exists = 1;
            break;
        }
    }
    ts3client_freeMemory(channels);
    return exists;
}

void OnMoveToChannel() {
    char idStr[32], pass[32];
    GetWindowTextA(g_hEditMoveID, idStr, sizeof(idStr));
    GetWindowTextA(g_hEditMovePass, pass, sizeof(pass));

    if (idStr[0] == '\0') return;

    uint64 channelID = strtoull(idStr, NULL, 10);
    if (!ChannelExists(channelID)) {
        LogFormat("Channel %llu does not exist", channelID);
        return;
    }

    anyID myID;
    if (ts3client_getClientID(g_scHandlerID, &myID) == ERROR_ok) {
        anyID clients[] = {myID, 0};
        ts3client_requestClientMove(g_scHandlerID, clients, channelID, pass, NULL);
        SetWindowTextA(g_hEditMoveID, "");
        SetWindowTextA(g_hEditMovePass, "");
    }
}

void ToggleMute() {
    g_isMuted = !g_isMuted;
    ts3client_setClientSelfVariableAsInt(g_scHandlerID, CLIENT_INPUT_MUTED, g_isMuted);
    ts3client_flushClientSelfUpdates(g_scHandlerID, NULL);
    SetWindowTextA(g_hBtnMute, g_isMuted ? "Unmute" : "Mute");
    LogMessage(g_isMuted ? "Microphone muted" : "Microphone unmuted");
}

void DrawButton(DRAWITEMSTRUCT* dis, int isAccent) {
    BOOL isHover = dis->itemState & ODS_HOTLIGHT || dis->itemState & ODS_SELECTED;
    BOOL isPressed = dis->itemState & ODS_SELECTED;

    COLORREF bgColor;
    if (isAccent) {
        bgColor = isHover ? CLR_ACCENT_HOV : CLR_ACCENT;
    } else {
        bgColor = isHover ? CLR_BTN_HOV : CLR_BTN_BG;
    }

    // Draw background
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(dis->hDC, &dis->rcItem, hBrush);
    DeleteObject(hBrush);

    // Draw border
    HPEN hPen = CreatePen(PS_SOLID, 1, isAccent ? CLR_ACCENT_HOV : CLR_BORDER);
    SelectObject(dis->hDC, hPen);
    SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
    DeleteObject(hPen);

    // Draw text
    char text[64];
    GetWindowTextA(dis->hwndItem, text, sizeof(text));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, CLR_TEXT);
    SelectObject(dis->hDC, g_hFont);

    RECT rc = dis->rcItem;
    if (isPressed) {
        rc.top += 1;
        rc.left += 1;
    }
    DrawTextA(dis->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

HWND CreateLabel(HWND parent, const char* text, int x, int y, int w, int h) {
    return CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, w, h, parent, NULL, NULL, NULL);
}

HWND CreateEdit(HWND parent, int x, int y, int w, int h, int id, DWORD extraStyle) {
    return CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | extraStyle, x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND CreateBtn(HWND parent, const char* text, int x, int y, int w, int h, int id) {
    return CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND CreateList(HWND parent, int x, int y, int w, int h, int id, DWORD extraStyle) {
    return CreateWindowExA(0, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | extraStyle, x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

void ApplyFontToChildren(HWND parent) {
    HWND child = GetWindow(parent, GW_CHILD);
    while (child) {
        SendMessage(child, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        child = GetWindow(child, GW_HWNDNEXT);
    }
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        int id = GetDlgCtrlID(hWnd);
        if (id == ID_EDIT_CH_PASS) {
            OnCreateChannel();
            return 0;
        } else if (id == ID_EDIT_MOVE_PASS) {
            OnMoveToChannel();
            return 0;
        }
    }
    return CallWindowProc(g_OrigEditProc, hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateLabel(hWnd, "Channels:", 10, 8, 60, 16);
        g_hChannelList = CreateList(hWnd, 10, 26, 200, 80, ID_CHANNEL_LIST, 0);

        CreateLabel(hWnd, "Create channel:", 10, 112, 100, 16);
        CreateLabel(hWnd, "Name:", 10, 134, 55, 16);
        g_hEditChName = CreateEdit(hWnd, 70, 131, 140, 22, ID_EDIT_CH_NAME, 0);
        CreateLabel(hWnd, "Pass:", 10, 160, 55, 16);
        g_hEditChPass = CreateEdit(hWnd, 70, 157, 140, 22, ID_EDIT_CH_PASS, ES_PASSWORD);
        g_hBtnCreate = CreateBtn(hWnd, "Create", 10, 185, 200, 28, ID_BTN_CREATE);

        CreateLabel(hWnd, "Move to channel:", 10, 222, 100, 16);
        CreateLabel(hWnd, "ID:", 10, 244, 55, 16);
        g_hEditMoveID = CreateEdit(hWnd, 70, 241, 140, 22, ID_EDIT_MOVE_ID, ES_NUMBER);
        CreateLabel(hWnd, "Pass:", 10, 270, 55, 16);
        g_hEditMovePass = CreateEdit(hWnd, 70, 267, 140, 22, ID_EDIT_MOVE_PASS, ES_PASSWORD);
        g_hBtnMove = CreateBtn(hWnd, "Move", 10, 295, 200, 28, ID_BTN_MOVE);

        CreateLabel(hWnd, "Log:", 10, 332, 30, 16);
        g_hLogList = CreateList(hWnd, 10, 350, 200, 100, ID_LOG_LIST, LBS_NOSEL);

        g_hBtnMute = CreateBtn(hWnd, "Mute", 10, 458, 200, 28, ID_BTN_MUTE);

        ApplyFontToChildren(hWnd);

        // Subclass password fields for Enter key
        g_OrigEditProc = (WNDPROC)SetWindowLongPtr(g_hEditChPass, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        SetWindowLongPtr(g_hEditMovePass, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG);
        return (LRESULT)g_hBrushBg;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG_LIGHT);
        return (LRESULT)g_hBrushEdit;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_BUTTON) {
            DrawButton(dis, 1);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_CREATE: OnCreateChannel(); break;
        case ID_BTN_MOVE:   OnMoveToChannel(); break;
        case ID_BTN_MUTE:   ToggleMute(); break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int InitTS3() {
    char* ip = NULL;
    char* password = NULL;
    unsigned int port = 0;
    char nickname[32];
    char path[MAX_PATH];
    struct ClientUIFunctions funcs = {0};
    int result = 0;
    enum { INIT_NONE, INIT_LIB, INIT_HANDLER, INIT_IDENTITY } level = INIT_NONE;

    generate_nickname(nickname, sizeof(nickname));
    programPath(path, sizeof(path));

    if (!load_env(".env", &ip, &port, &password)) {
        LogMessage("Error reading .env file");
        return 0;
    }

    funcs.onConnectStatusChangeEvent = onConnectStatusChangeEvent;
    funcs.onTalkStatusChangeEvent = onTalkStatusChangeEvent;
    funcs.onNewChannelCreatedEvent = onNewChannelCreatedEvent;
    funcs.onDelChannelEvent = onDelChannelEvent;
    funcs.onClientMoveEvent = onClientMoveEvent;
    funcs.onServerErrorEvent = onServerErrorEvent;

    do {
        if (ts3client_initClientLib(&funcs, NULL, LogType_NONE, NULL, path) != ERROR_ok) {
            LogMessage("Failed to initialize client library");
            break;
        }
        level = INIT_LIB;

        if (ts3client_spawnNewServerConnectionHandler(0, &g_scHandlerID) != ERROR_ok) {
            LogMessage("Failed to spawn connection handler");
            break;
        }
        level = INIT_HANDLER;

        if (ts3client_openCaptureDevice(g_scHandlerID, "", NULL) != ERROR_ok) {
            LogMessage("Failed to open microphone");
            break;
        }

        if (ts3client_openPlaybackDevice(g_scHandlerID, "", NULL) != ERROR_ok) {
            LogMessage("Failed to open speakers");
            break;
        }

        ts3client_setPreProcessorConfigValue(g_scHandlerID, "vad", "true");
        ts3client_setPreProcessorConfigValue(g_scHandlerID, "vad_mode", "2");
        ts3client_setPreProcessorConfigValue(g_scHandlerID, "voiceactivation_level", "-20");

        if (ts3client_createIdentity(&g_identity) != ERROR_ok) {
            LogMessage("Failed to create identity");
            break;
        }
        level = INIT_IDENTITY;

        LogMessage("Connecting to server...");
        LogFormat("IP: %s, Port: %u", ip, port);

        if (ts3client_startConnection(g_scHandlerID, g_identity, ip, port, nickname, NULL, "", password) != ERROR_ok) {
            LogMessage("Failed to connect");
            break;
        }

        ts3client_setClientSelfVariableAsInt(g_scHandlerID, CLIENT_INPUT_DEACTIVATED, INPUT_ACTIVE);
        ts3client_setClientSelfVariableAsInt(g_scHandlerID, CLIENT_INPUT_MUTED, 0);
        ts3client_flushClientSelfUpdates(g_scHandlerID, NULL);

        g_ts3Initialized = 1;
        result = 1;
    } while (0);

    if (!result) {
        if (level >= INIT_IDENTITY) {
            ts3client_freeMemory(g_identity);
            g_identity = NULL;
        }
        if (level >= INIT_HANDLER)
            ts3client_destroyServerConnectionHandler(g_scHandlerID);
        if (level >= INIT_LIB)
            ts3client_destroyClientLib();
    }

    free(ip);
    free(password);
    return result;
}

void CleanupTS3() {
    if (!g_ts3Initialized) return;

    if (g_identity) {
        ts3client_freeMemory(g_identity);
        g_identity = NULL;
    }
    ts3client_stopConnection(g_scHandlerID, "leaving");
    ts3client_destroyServerConnectionHandler(g_scHandlerID);
    ts3client_destroyClientLib();
    g_ts3Initialized = 0;
}

void InitTheme() {
    g_hFont = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");

    g_hBrushBg = CreateSolidBrush(CLR_BG);
    g_hBrushEdit = CreateSolidBrush(CLR_BG_LIGHT);
}

void CleanupTheme() {
    if (g_hFont) DeleteObject(g_hFont);
    if (g_hBrushBg) DeleteObject(g_hBrushBg);
    if (g_hBrushEdit) DeleteObject(g_hBrushEdit);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    InitTheme();

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = g_hBrushBg;
    wc.lpszClassName = "TS3ClientGUI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));

    if (!RegisterClassA(&wc)) return 1;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, 220, 498};
    AdjustWindowRect(&r, style, FALSE);

    g_hWnd = CreateWindowA("TS3ClientGUI", "TS3 Client", style,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        NULL, NULL, hInstance, NULL);

    if (!g_hWnd) return 1;

    // Enable dark title bar (Windows 10+)
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(g_hWnd, 20, &darkMode, sizeof(darkMode));

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    if (!InitTS3()) {
        MessageBoxA(g_hWnd, "Failed to initialize TS3 client", "Error", MB_OK | MB_ICONERROR);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CleanupTS3();
    CleanupTheme();
    return (int)msg.wParam;
}
