// ===================================================================================
// ==   OpenSSH Controller - 修正提示文字無法顯示的最終版本               ==
// ===================================================================================

#define _WIN32_WINNT 0x0601 
#define MIB_TCP_STATE_ESTABLISHED 5

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <cstdio>
#include <shellapi.h>
#include <string.h> 
#include <richedit.h> 
#include <CommCtrl.h>

#include "resource.h" 

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Comctl32.lib")

// *** 新增：強制連結 Common Controls v6.0，以啟用視覺化樣式和 Cue Banner 等現代功能 ***
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


// --- 全域變數 ---
HINSTANCE g_hInstance;

// --- 函式宣告 ---
void StartSSHService() { system("net start sshd"); MessageBox(NULL, "正在嘗試啟動 OpenSSH 服務...", "通知", MB_OK | MB_ICONINFORMATION); }
void StopSSHService() { system("net stop sshd"); MessageBox(NULL, "正在嘗試停止 OpenSSH 服務...", "通知", MB_OK | MB_ICONINFORMATION); }
void RestartSSHService() { system("net stop sshd && net start sshd"); MessageBox(NULL, "正在嘗試重啟 OpenSSH 服務...", "通知", MB_OK | MB_ICONINFORMATION); }
void EditSSHConfig() { ShellExecute(NULL, "open", "notepad.exe", "C:\\ProgramData\\ssh\\sshd_config", NULL, SW_SHOWNORMAL); }
void OpenHelpFile(HWND hwnd) { ShellExecute(hwnd, "open", "sop.html", NULL, NULL, SW_SHOWNORMAL); }
void LaunchEventViewer() { ShellExecute(NULL, "open", "eventvwr.msc", NULL, NULL, SW_SHOWNORMAL); }
void LaunchFirewallSettings() { ShellExecute(NULL, "open", "wf.msc", NULL, NULL, SW_SHOWNORMAL); }
void RefreshConnections(HWND hClientList, HWND hServerList);
void UpdateNetworkInfo(HWND hLabel);
void UpdateButtonsStatus(HWND hwnd);
LRESULT CALLBACK InputEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ExecuteCommand(HWND hOutput, const char* command);
void AppendTextToOutputW(HWND hOutput, const wchar_t* text);
WNDPROC oldEditProc;
void RepositionControls(HWND hwnd);


// --- 函式實作 ---

void UpdateButtonsStatus(HWND hwnd) {
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (NULL == schSCManager) return;
    SC_HANDLE schService = OpenService(schSCManager, "sshd", SERVICE_QUERY_STATUS);
    if (schService == NULL) {
        EnableWindow(GetDlgItem(hwnd, IDC_START_BUTTON), FALSE); EnableWindow(GetDlgItem(hwnd, IDC_STOP_BUTTON), FALSE); EnableWindow(GetDlgItem(hwnd, IDC_RESTART_BUTTON), FALSE);
        CloseServiceHandle(schSCManager); return;
    }
    SERVICE_STATUS_PROCESS ssp;
    DWORD dwBytesNeeded;
    if (!QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &dwBytesNeeded)) {
        CloseServiceHandle(schService); CloseServiceHandle(schSCManager); return;
    }
    bool isRunning = (ssp.dwCurrentState == SERVICE_RUNNING);
    bool isStopped = (ssp.dwCurrentState == SERVICE_STOPPED);
    EnableWindow(GetDlgItem(hwnd, IDC_START_BUTTON), isStopped);
    EnableWindow(GetDlgItem(hwnd, IDC_STOP_BUTTON), isRunning);
    EnableWindow(GetDlgItem(hwnd, IDC_RESTART_BUTTON), isRunning);
    CloseServiceHandle(schService); CloseServiceHandle(schSCManager);
}

void RepositionControls(HWND hwnd) {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    const int padding = 30;
    const int leftColumnWidth = 180;
    const int buttonHeight = 35;
    const int spacingX = 20;
    const int spacingY = 10;
    const int sectionSpacing = 15;
    int currentY = padding;

    MoveWindow(GetDlgItem(hwnd, IDC_TITLE_LABEL), padding, currentY, clientWidth - (padding * 2), 30, TRUE);
    currentY += 30 + sectionSpacing;

    int rightColumnX = padding + leftColumnWidth + spacingX;
    int rightColumnWidth = clientWidth - rightColumnX - padding;
    if (rightColumnWidth < 100) rightColumnWidth = 100;
    int listWidth = (rightColumnWidth - spacingX) / 2;
    int listHeight = 155;
    
    MoveWindow(GetDlgItem(hwnd, IDC_CLIENT_LIST_LABEL), rightColumnX, currentY, listWidth, 20, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SERVER_LIST_LABEL), rightColumnX + listWidth + spacingX, currentY, listWidth, 20, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_CLIENT_LIST), rightColumnX, currentY + 25, listWidth, listHeight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SERVER_LIST), rightColumnX + listWidth + spacingX, currentY + 25, listWidth, listHeight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_REFRESH_CONN_BTN), rightColumnX, currentY + 25 + listHeight + spacingY, rightColumnWidth, buttonHeight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_NET_INFO_LABEL), rightColumnX, currentY + 25 + listHeight + spacingY + buttonHeight + spacingY, rightColumnWidth, 20, TRUE);

    int terminalTopY = padding + 30 + sectionSpacing + (buttonHeight + spacingY) * 7;
    int terminalWidth = clientWidth - (padding * 2);
    int terminalInputHeight = 25;
    int terminalOutputHeight = clientHeight - terminalTopY - terminalInputHeight - spacingY - padding;
    if (terminalOutputHeight < 50) terminalOutputHeight = 50;

    MoveWindow(GetDlgItem(hwnd, IDC_OUTPUT_EDIT), padding, terminalTopY, terminalWidth, terminalOutputHeight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_INPUT_EDIT), padding, terminalTopY + terminalOutputHeight + spacingY, terminalWidth, terminalInputHeight, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    static HFONT hFont, hTitleFont, hListFont;
    static HBRUSH hBkgBrush;

    switch(Message) {
        case WM_CREATE: {
            hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            hTitleFont = CreateFont(24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            hListFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Consolas");
            hBkgBrush = CreateSolidBrush(RGB(240, 240, 240));

            HWND hTitle = CreateWindow("STATIC", "OpenSSH Server Console", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_TITLE_LABEL, NULL, NULL);
            HWND btnStart = CreateWindow("BUTTON", "啟動服務", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 75, 180, 35, hwnd, (HMENU)IDC_START_BUTTON, NULL, NULL);
            HWND btnStop = CreateWindow("BUTTON", "停止服務", WS_VISIBLE | WS_CHILD, 30, 120, 180, 35, hwnd, (HMENU)IDC_STOP_BUTTON, NULL, NULL);
            HWND btnRestart = CreateWindow("BUTTON", "重啟服務", WS_VISIBLE | WS_CHILD, 30, 165, 180, 35, hwnd, (HMENU)IDC_RESTART_BUTTON, NULL, NULL);
            HWND btnEdit = CreateWindow("BUTTON", "編輯設定檔", WS_VISIBLE | WS_CHILD, 30, 210, 180, 35, hwnd, (HMENU)IDC_EDIT_BUTTON, NULL, NULL);
            HWND btnHelp = CreateWindow("BUTTON", "使用說明", WS_VISIBLE | WS_CHILD, 30, 255, 180, 35, hwnd, (HMENU)IDC_HELP_BUTTON, NULL, NULL);
            HWND btnEvent = CreateWindow("BUTTON", "事件檢視器", WS_VISIBLE | WS_CHILD, 30, 300, 180, 35, hwnd, (HMENU)IDC_EVENT_VIEWER_BUTTON, NULL, NULL);
            HWND btnFirewall = CreateWindow("BUTTON", "防火牆設定", WS_VISIBLE | WS_CHILD, 30, 345, 180, 35, hwnd, (HMENU)IDC_FIREWALL_BUTTON, NULL, NULL);
            
            HWND hClientLabel = CreateWindow("STATIC", "客戶端 (遠端)", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_CLIENT_LIST_LABEL, NULL, NULL);
            HWND hServerLabel = CreateWindow("STATIC", "伺服器端 (本機)", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_SERVER_LIST_LABEL, NULL, NULL);
            HWND hClientListBox = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)IDC_CLIENT_LIST, NULL, NULL);
            HWND hServerListBox = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)IDC_SERVER_LIST, NULL, NULL);
            HWND hRefreshBtn = CreateWindow("BUTTON", "刷新連線", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_REFRESH_CONN_BTN, NULL, NULL);
            HWND hNetInfoLabel = CreateWindow("STATIC", "連線方式: 正在獲取...", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_NET_INFO_LABEL, NULL, NULL);

            HWND hOutputEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0, hwnd, (HMENU)IDC_OUTPUT_EDIT, NULL, NULL);
            HWND hInputEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)IDC_INPUT_EDIT, NULL, NULL);

            SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
            SendMessage(btnStart, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(btnStop, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(btnRestart, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(btnEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(btnHelp, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(btnEvent, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(btnFirewall, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hClientLabel, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(hServerLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hClientListBox, WM_SETFONT, (WPARAM)hListFont, TRUE); SendMessage(hServerListBox, WM_SETFONT, (WPARAM)hListFont, TRUE);
            SendMessage(hRefreshBtn, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(hNetInfoLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hOutputEdit, WM_SETFONT, (WPARAM)hListFont, TRUE); SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            SendMessage(hOutputEdit, EM_SETLIMITTEXT, 0, 0);
            oldEditProc = (WNDPROC)SetWindowLongPtr(hInputEdit, GWLP_WNDPROC, (LONG_PTR)InputEditProc);
            SetWindowLongPtr(hInputEdit, GWLP_USERDATA, (LONG_PTR)hOutputEdit); 

            wchar_t inputHint[128];
            LoadStringW(g_hInstance, IDS_INPUT_HINT, inputHint, 128);
            SendMessageW(hInputEdit, EM_SETCUEBANNER, FALSE, (LPARAM)inputHint);

            wchar_t welcomeMsg[512];
            LoadStringW(g_hInstance, IDS_WELCOME_MSG, welcomeMsg, 512);
            AppendTextToOutputW(hOutputEdit, welcomeMsg);
            
            RefreshConnections(hClientListBox, hServerListBox);
            UpdateNetworkInfo(hNetInfoLabel);
            UpdateButtonsStatus(hwnd);
            break;
        }
        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                RepositionControls(hwnd);
            }
            break;
        }
        case WM_COMMAND: {
            bool bUpdateStatus = false;
            switch(LOWORD(wParam)) {
                case IDC_START_BUTTON: StartSSHService(); bUpdateStatus = true; break;
                case IDC_STOP_BUTTON: StopSSHService(); bUpdateStatus = true; break;
                case IDC_RESTART_BUTTON: RestartSSHService(); bUpdateStatus = true; break;
                case IDC_EDIT_BUTTON: EditSSHConfig(); break;
                case IDC_REFRESH_CONN_BTN: RefreshConnections(GetDlgItem(hwnd, IDC_CLIENT_LIST), GetDlgItem(hwnd, IDC_SERVER_LIST)); UpdateNetworkInfo(GetDlgItem(hwnd, IDC_NET_INFO_LABEL)); bUpdateStatus = true; break;
                case IDC_HELP_BUTTON: OpenHelpFile(hwnd); break;
                case IDC_EVENT_VIEWER_BUTTON: LaunchEventViewer(); break;
                case IDC_FIREWALL_BUTTON: LaunchFirewallSettings(); break;
            }
            if (bUpdateStatus) {
                Sleep(1500); 
                UpdateButtonsStatus(hwnd);
                RefreshConnections(GetDlgItem(hwnd, IDC_CLIENT_LIST), GetDlgItem(hwnd, IDC_SERVER_LIST));
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam; SetTextColor(hdcStatic, RGB(50, 50, 50)); SetBkMode(hdcStatic, TRANSPARENT); return (INT_PTR)hBkgBrush;
        }
        case WM_DESTROY: {
            DeleteObject(hFont); DeleteObject(hTitleFont); DeleteObject(hListFont); DeleteObject(hBkgBrush);
            PostQuitMessage(0); 
            break;
        }
        default: return DefWindowProc(hwnd, Message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    LoadLibrary(TEXT("Msftedit.dll"));
    WSADATA wsaData; 
    WSAStartup(MAKEWORD(2, 2), &wsaData); 

    if (lstrcmpiA(lpCmdLine, "eventviewer") == 0) {
        LaunchEventViewer();
        return 0;
    }

    WNDCLASSEX wc; 
    wc.cbSize = sizeof(WNDCLASSEX); wc.style = 0; wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0; wc.cbWndExtra = 0; wc.hInstance = hInstance; 
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON)); 
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON)); 
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(240, 240, 240)));
    wc.lpszMenuName = NULL; wc.lpszClassName = "MyWindowClass"; 
    if(!RegisterClassEx(&wc)) { MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK); return 0; }
    
    const int initialWidth = 800;
    const int initialHeight = 600; 
    
    HWND hwnd = CreateWindowEx(0, "MyWindowClass", "OpenSSH Controller", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
        CW_USEDEFAULT, CW_USEDEFAULT, initialWidth, initialHeight, NULL, NULL, hInstance, NULL);
        
    ShowWindow(hwnd, nCmdShow); 
    UpdateWindow(hwnd);
    
    MSG Msg;
    while(GetMessage(&Msg, NULL, 0, 0) > 0) { TranslateMessage(&Msg); DispatchMessage(&Msg); }
    
    WSACleanup();
    return Msg.wParam;
}

void RefreshConnections(HWND hClientList, HWND hServerList) {
    SendMessage(hClientList, LB_RESETCONTENT, 0, 0); SendMessage(hServerList, LB_RESETCONTENT, 0, 0);
    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL; DWORD dwSize = 0;
    if (GetExtendedTcpTable(NULL, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (PMIB_TCPTABLE_OWNER_PID) new char[dwSize];
        if (pTcpTable == NULL) { return; }
    } else { return; }
    if (GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
            if (ntohs(pTcpTable->table[i].dwLocalPort) == 22 && pTcpTable->table[i].dwState == MIB_TCP_STATE_ESTABLISHED) {
                char szLocalAddr[128], serverInfo[256], szRemoteAddr[128], clientInfo[256];
                struct in_addr LocalAddr = { (u_long)pTcpTable->table[i].dwLocalAddr };
                struct in_addr RemoteAddr = { (u_long)pTcpTable->table[i].dwRemoteAddr };
                inet_ntop(AF_INET, &LocalAddr, szLocalAddr, sizeof(szLocalAddr));
                inet_ntop(AF_INET, &RemoteAddr, szRemoteAddr, sizeof(szRemoteAddr));
                snprintf(serverInfo, sizeof(serverInfo), "%s:%d", szLocalAddr, ntohs(pTcpTable->table[i].dwLocalPort));
                snprintf(clientInfo, sizeof(clientInfo), "%s:%d", szRemoteAddr, ntohs(pTcpTable->table[i].dwRemotePort));
                SendMessage(hServerList, LB_ADDSTRING, 0, (LPARAM)serverInfo);
                SendMessage(hClientList, LB_ADDSTRING, 0, (LPARAM)clientInfo);
            }
        }
    }
    if (pTcpTable != NULL) { delete[] pTcpTable; }
}

void UpdateNetworkInfo(HWND hLabel) {
    std::string netInfo = "連線方式: 未知";
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
    if (pAddresses == NULL) { SetWindowText(hLabel, "連線方式: 記憶體分配失敗"); return; }
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
            if (pCurr->OperStatus == IfOperStatusUp && pCurr->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                for(PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                        switch (pCurr->IfType) {
                            case IF_TYPE_ETHERNET_CSMACD: netInfo = "連線方式: 乙太網路"; break;
                            case IF_TYPE_IEEE80211: {
                                char mbstr[256] = {0}; wcstombs(mbstr, pCurr->FriendlyName, sizeof(mbstr) - 1);
                                netInfo = "連線方式: Wi-Fi ("; netInfo += mbstr; netInfo += ")";
                                break;
                            }
                            default: { char typeStr[256]; snprintf(typeStr, sizeof(typeStr), "連線方式: 其他 (%lu)", pCurr->IfType); netInfo = typeStr; break; }
                        }
                        goto found_adapter;
                    }
                }
            }
        }
    } else { netInfo = "連線方式: API呼叫失敗"; }
found_adapter:
    free(pAddresses);
    SetWindowText(hLabel, netInfo.c_str());
}

void AppendTextToOutputW(HWND hOutput, const wchar_t* text) {
    int len = GetWindowTextLengthW(hOutput);
    SendMessageW(hOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len); 
    SendMessageW(hOutput, EM_REPLACESEL, 0, (LPARAM)text);     
}

void ExecuteCommand(HWND hOutput, const char* command) {
    char cmdLine[1024];
    snprintf(cmdLine, sizeof(cmdLine), "cmd.exe /c %s", command);

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        AppendTextToOutputW(hOutput, L"Error: CreatePipe failed.\r\n");
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
 
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        AppendTextToOutputW(hOutput, L"Error: CreateProcess failed.\r\n");
        CloseHandle(hReadPipe); CloseHandle(hWritePipe);
        return;
    }
    CloseHandle(hWritePipe);

    CHAR buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead != 0) {
        buffer[bytesRead] = '\0';
        int wideCharLen = MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, NULL, 0);
        if (wideCharLen > 0) {
            wchar_t* wideBuffer = new wchar_t[wideCharLen];
            MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, wideBuffer, wideCharLen);
            AppendTextToOutputW(hOutput, wideBuffer);
            delete[] wideBuffer;
        }
    }

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

LRESULT CALLBACK InputEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        int len = GetWindowTextLength(hwnd);
        if (len > 0) {
            char* command = new char[len + 1];
            GetWindowText(hwnd, command, len + 1);
            HWND hOutput = (HWND)GetWindowLongPtr(hwnd, GWLP_USERDATA);

            if (_stricmp(command, "clear") == 0 || _stricmp(command, "cls") == 0) {
                SetWindowTextW(hOutput, L"");
                wchar_t welcomeMsg[512];
                LoadStringW(g_hInstance, IDS_WELCOME_MSG, welcomeMsg, 512);
                AppendTextToOutputW(hOutput, welcomeMsg);
            } else {
                wchar_t prompt[1024];
                swprintf(prompt, sizeof(prompt)/sizeof(wchar_t), L"\r\nC:\\>%hs\r\n", command);
                AppendTextToOutputW(hOutput, prompt);
                ExecuteCommand(hOutput, command);
            }

            SetWindowText(hwnd, "");
            delete[] command;
        }
        return 0;
    }
    return CallWindowProc(oldEditProc, hwnd, uMsg, wParam, lParam);
}
