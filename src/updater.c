#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "updater.h"
#include "update_parser.h"

#include <bcrypt.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <winhttp.h>

#define UPDATE_POPUP_CLASS L"PersistentCalculatorUpdatePopup"
#define UPDATE_STATUS_MESSAGE (WM_APP + 40)
#define UPDATE_POPUP_TIMER 1
#define UPDATE_POPUP_MILLISECONDS 5000
#define UPDATE_JSON_LIMIT (2u * 1024u * 1024u)
#define UPDATE_DOWNLOAD_LIMIT (64u * 1024u * 1024u)
#define UPDATE_PATH_CAPACITY 32768

static const wchar_t *UPDATE_API_URL =
    L"https://api.github.com/repos/Garries420/Persistent-Calculator/releases/latest";

static HWND g_update_popup;
static HFONT g_update_font;
static volatile LONG g_update_check_active;
static wchar_t g_update_status[192] = L"Checking for updates…";

typedef struct HttpRequest {
    HINTERNET session;
    HINTERNET connection;
    HINTERNET request;
} HttpRequest;

static void close_http_request(HttpRequest *http) {
    if (http->request) WinHttpCloseHandle(http->request);
    if (http->connection) WinHttpCloseHandle(http->connection);
    if (http->session) WinHttpCloseHandle(http->session);
    memset(http, 0, sizeof(*http));
}

static int copy_url_component(wchar_t *destination, size_t capacity,
                              const wchar_t *source, DWORD length) {
    if (!destination || !source || capacity == 0 || (size_t)length >= capacity) return 0;
    wmemcpy(destination, source, length);
    destination[length] = L'\0';
    return 1;
}

static int open_https_get(const wchar_t *url, HttpRequest *http) {
    URL_COMPONENTSW components;
    wchar_t host[512];
    wchar_t request_path[8192];
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    static const wchar_t headers[] =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
    memset(http, 0, sizeof(*http));
    memset(&components, 0, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = (DWORD)-1;
    components.dwUrlPathLength = (DWORD)-1;
    components.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url, 0, 0, &components) ||
        components.nScheme != INTERNET_SCHEME_HTTPS ||
        !copy_url_component(host, _countof(host), components.lpszHostName,
                            components.dwHostNameLength)) return 0;
    if (!components.lpszUrlPath ||
        (size_t)components.dwUrlPathLength + (size_t)components.dwExtraInfoLength + 1 >=
            _countof(request_path)) return 0;
    wmemcpy(request_path, components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength)
        wmemcpy(request_path + components.dwUrlPathLength, components.lpszExtraInfo,
                components.dwExtraInfoLength);
    request_path[components.dwUrlPathLength + components.dwExtraInfoLength] = L'\0';

    http->session = WinHttpOpen(L"PersistentCalculator/" PERSISTENT_CALCULATOR_VERSION_W,
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!http->session) goto failure;
    WinHttpSetTimeouts(http->session, 5000, 5000, 10000, 10000);
    http->connection = WinHttpConnect(http->session, host, components.nPort, 0);
    if (!http->connection) goto failure;
    http->request = WinHttpOpenRequest(http->connection, L"GET", request_path, NULL,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    if (!http->request) goto failure;
    WinHttpSetOption(http->request, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirect_policy, sizeof(redirect_policy));
    if (!WinHttpSendRequest(http->request, headers, (DWORD)-1L,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(http->request, NULL) ||
        !WinHttpQueryHeaders(http->request,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX) || status != 200) goto failure;
    return 1;

failure:
    close_http_request(http);
    return 0;
}

static int http_get_memory(const wchar_t *url, char **output, size_t *output_size) {
    HttpRequest http;
    char chunk[16384];
    char *data = NULL;
    size_t total = 0;
    DWORD received;
    int success = 0;
    if (!output || !output_size || !open_https_get(url, &http)) return 0;
    data = (char *)HeapAlloc(GetProcessHeap(), 0, 1);
    if (!data) goto cleanup;
    for (;;) {
        received = 0;
        if (!WinHttpReadData(http.request, chunk, sizeof(chunk), &received)) goto cleanup;
        if (!received) break;
        if (total + received + 1 > UPDATE_JSON_LIMIT) goto cleanup;
        {
            char *resized = (char *)HeapReAlloc(GetProcessHeap(), 0, data,
                                                total + received + 1);
            if (!resized) goto cleanup;
            data = resized;
        }
        memcpy(data + total, chunk, received);
        total += received;
    }
    data[total] = '\0';
    *output = data;
    *output_size = total;
    data = NULL;
    success = 1;

cleanup:
    if (data) HeapFree(GetProcessHeap(), 0, data);
    close_http_request(&http);
    return success;
}

static int http_download_file(const wchar_t *url, const wchar_t *path) {
    HttpRequest http;
    HANDLE file = INVALID_HANDLE_VALUE;
    unsigned char chunk[32768];
    size_t total = 0;
    DWORD received, written;
    int success = 0;
    if (!open_https_get(url, &http)) return 0;
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;
    for (;;) {
        received = 0;
        if (!WinHttpReadData(http.request, chunk, sizeof(chunk), &received)) goto cleanup;
        if (!received) break;
        if (total + received > UPDATE_DOWNLOAD_LIMIT) goto cleanup;
        if (!WriteFile(file, chunk, received, &written, NULL) || written != received) goto cleanup;
        total += received;
    }
    if (!FlushFileBuffers(file) || total == 0) goto cleanup;
    success = 1;

cleanup:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    close_http_request(&http);
    if (!success) DeleteFileW(path);
    return success;
}

static int sha256_file(const wchar_t *path, char output[65]) {
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    PUCHAR hash_object = NULL;
    DWORD object_size = 0, hash_size = 0, property_size = 0;
    unsigned char digest[32];
    unsigned char buffer[32768];
    DWORD received;
    int success = 0;
    static const char hex[] = "0123456789abcdef";
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_size,
                          sizeof(object_size), &property_size, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_size,
                          sizeof(hash_size), &property_size, 0) < 0 ||
        hash_size != sizeof(digest)) goto cleanup;
    hash_object = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, object_size);
    if (!hash_object || BCryptCreateHash(algorithm, &hash, hash_object, object_size,
                                         NULL, 0, 0) < 0) goto cleanup;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &received, NULL)) goto cleanup;
        if (!received) break;
        if (BCryptHashData(hash, buffer, received, 0) < 0) goto cleanup;
    }
    if (BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0) goto cleanup;
    for (size_t index = 0; index < sizeof(digest); ++index) {
        output[index * 2] = hex[digest[index] >> 4];
        output[index * 2 + 1] = hex[digest[index] & 15];
    }
    output[64] = '\0';
    success = 1;

cleanup:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash) BCryptDestroyHash(hash);
    if (hash_object) HeapFree(GetProcessHeap(), 0, hash_object);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return success;
}

static int utf8_to_wide(const char *source, wchar_t *destination, size_t capacity) {
    int needed;
    if (!source || !destination || capacity == 0) return 0;
    needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1, NULL, 0);
    if (needed <= 0 || (size_t)needed > capacity) return 0;
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
                               destination, (int)capacity) > 0;
}

static void set_popup_region(HWND popup, int width, int height) {
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, 12, 12);
    if (region) SetWindowRgn(popup, region, TRUE);
}

static LRESULT CALLBACK update_popup_proc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_TIMER:
            if (wparam == UPDATE_POPUP_TIMER) {
                KillTimer(window, UPDATE_POPUP_TIMER);
                ShowWindow(window, SW_HIDE);
            }
            return 0;
        case UPDATE_STATUS_MESSAGE: {
            wchar_t *status = (wchar_t *)lparam;
            if (status) {
                wcsncpy(g_update_status, status, _countof(g_update_status) - 1);
                g_update_status[_countof(g_update_status) - 1] = L'\0';
                HeapFree(GetProcessHeap(), 0, status);
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT client;
            HBRUSH background;
            HPEN border;
            HGDIOBJ old_brush, old_pen, old_font;
            HDC dc = BeginPaint(window, &paint);
            GetClientRect(window, &client);
            background = CreateSolidBrush(RGB(48, 48, 48));
            border = CreatePen(PS_SOLID, 1, RGB(91, 91, 91));
            old_brush = SelectObject(dc, background);
            old_pen = SelectObject(dc, border);
            RoundRect(dc, client.left, client.top, client.right, client.bottom, 12, 12);
            SelectObject(dc, old_pen);
            SelectObject(dc, old_brush);
            DeleteObject(border);
            DeleteObject(background);
            old_font = SelectObject(dc, g_update_font ? g_update_font : GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(242, 242, 242));
            InflateRect(&client, -12, -7);
            DrawTextW(dc, g_update_status, -1, &client,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, old_font);
            EndPaint(window, &paint);
            return 0;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static int ensure_popup(HWND owner) {
    WNDCLASSEXW window_class;
    HINSTANCE instance = GetModuleHandleW(NULL);
    if (g_update_popup && IsWindow(g_update_popup)) return 1;
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = update_popup_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = UPDATE_POPUP_CLASS;
    RegisterClassExW(&window_class);
    g_update_popup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     UPDATE_POPUP_CLASS, L"", WS_POPUP,
                                     0, 0, 300, 58, owner, NULL, instance, NULL);
    return g_update_popup != NULL;
}

static void show_update_popup(HWND owner, const wchar_t *status) {
    RECT owner_rect;
    UINT dpi;
    int width, height, x, y;
    if (!ensure_popup(owner)) return;
    wcsncpy(g_update_status, status, _countof(g_update_status) - 1);
    g_update_status[_countof(g_update_status) - 1] = L'\0';
    dpi = GetDpiForWindow(owner);
    if (!dpi) dpi = 96;
    width = MulDiv(300, (int)dpi, 96);
    height = MulDiv(58, (int)dpi, 96);
    GetWindowRect(owner, &owner_rect);
    x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    y = owner_rect.top + MulDiv(62, (int)dpi, 96);
    set_popup_region(g_update_popup, width, height);
    SetWindowPos(g_update_popup, HWND_TOP, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetTimer(g_update_popup, UPDATE_POPUP_TIMER, UPDATE_POPUP_MILLISECONDS, NULL);
    InvalidateRect(g_update_popup, NULL, FALSE);
}

static void post_update_status(const wchar_t *status) {
    size_t bytes;
    wchar_t *copy;
    HWND popup = g_update_popup;
    if (!popup || !IsWindow(popup)) return;
    bytes = (wcslen(status) + 1) * sizeof(wchar_t);
    copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (!copy) return;
    memcpy(copy, status, bytes);
    if (!PostMessageW(popup, UPDATE_STATUS_MESSAGE, 0, (LPARAM)copy))
        HeapFree(GetProcessHeap(), 0, copy);
}

static int launch_update_helper(const wchar_t *download_path,
                                const wchar_t *target_path) {
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    wchar_t *command;
    size_t command_capacity = wcslen(download_path) + wcslen(target_path) + 96;
    int success = 0;
    command = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                   command_capacity * sizeof(wchar_t));
    if (!command) return 0;
    _snwprintf(command, command_capacity,
               L"\"%ls\" --apply-update \"%ls\" %lu",
               download_path, target_path, GetCurrentProcessId());
    command[command_capacity - 1] = L'\0';
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    if (CreateProcessW(download_path, command, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        success = 1;
    }
    HeapFree(GetProcessHeap(), 0, command);
    return success;
}

static DWORD WINAPI update_check_thread(void *parameter) {
    HWND owner = (HWND)parameter;
    char *json = NULL;
    size_t json_size = 0;
    UpdateReleaseInfo release;
    wchar_t download_url[2048];
    wchar_t current_path[UPDATE_PATH_CAPACITY];
    wchar_t temporary_directory[UPDATE_PATH_CAPACITY];
    wchar_t download_path[UPDATE_PATH_CAPACITY];
    wchar_t status[192];
    char actual_sha256[65];
    DWORD path_length;

    if (!http_get_memory(UPDATE_API_URL, &json, &json_size) || json_size == 0) {
        post_update_status(L"Update check unavailable. The calculator remains fully usable.");
        goto done;
    }
    if (!update_parse_release_json(json, &release)) {
        post_update_status(L"The release information could not be verified safely.");
        goto done;
    }
    if (update_compare_versions(release.version, PERSISTENT_CALCULATOR_VERSION_A) <= 0) {
        _snwprintf(status, _countof(status),
                   L"Persistent Calculator is up to date — version %ls.",
                   PERSISTENT_CALCULATOR_VERSION_W);
        status[_countof(status) - 1] = L'\0';
        post_update_status(status);
        goto done;
    }
    if (!utf8_to_wide(release.download_url, download_url, _countof(download_url))) {
        post_update_status(L"The update download address was invalid.");
        goto done;
    }
    _snwprintf(status, _countof(status), L"Downloading verified update v%hs…", release.version);
    status[_countof(status) - 1] = L'\0';
    post_update_status(status);
    path_length = GetModuleFileNameW(NULL, current_path, _countof(current_path));
    if (!path_length || path_length >= _countof(current_path) ||
        !(path_length = GetTempPathW(_countof(temporary_directory), temporary_directory)) ||
        path_length >= _countof(temporary_directory)) {
        post_update_status(L"Windows could not prepare the update location.");
        goto done;
    }
    if (_snwprintf(download_path, _countof(download_path),
                   L"%lsPersistentCalculator-Update-%lu.exe",
                   temporary_directory, GetCurrentProcessId()) < 0) {
        post_update_status(L"The update path was too long.");
        goto done;
    }
    DeleteFileW(download_path);
    if (!http_download_file(download_url, download_path) ||
        !sha256_file(download_path, actual_sha256) ||
        strcmp(actual_sha256, release.sha256) != 0) {
        DeleteFileW(download_path);
        post_update_status(L"Update cancelled because its SHA-256 verification failed.");
        goto done;
    }
    post_update_status(L"Update verified. Restarting Persistent Calculator…");
    if (!launch_update_helper(download_path, current_path)) {
        DeleteFileW(download_path);
        post_update_status(L"The verified update could not be started.");
        goto done;
    }
    PostMessageW(owner, WM_CLOSE, 0, 0);

done:
    if (json) HeapFree(GetProcessHeap(), 0, json);
    InterlockedExchange(&g_update_check_active, 0);
    return 0;
}

static void start_update_check(HWND owner) {
    HANDLE thread;
    if (InterlockedCompareExchange(&g_update_check_active, 1, 0) != 0) {
        show_update_popup(owner, L"An update check is already running.");
        return;
    }
    thread = CreateThread(NULL, 0, update_check_thread, owner, 0, NULL);
    if (!thread) {
        InterlockedExchange(&g_update_check_active, 0);
        show_update_popup(owner, L"Windows could not start the update check.");
        return;
    }
    CloseHandle(thread);
}

void updater_initialize(HWND owner) {
    LOGFONTW font;
    memset(&font, 0, sizeof(font));
    font.lfHeight = -MulDiv(10, (int)(GetDpiForWindow(owner) ? GetDpiForWindow(owner) : 96), 72);
    font.lfWeight = FW_NORMAL;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(font.lfFaceName, L"Segoe UI");
    g_update_font = CreateFontIndirectW(&font);
    show_update_popup(owner, L"Checking GitHub securely for updates…");
    start_update_check(owner);
}

void updater_check_now(HWND owner) {
    show_update_popup(owner, L"Checking GitHub securely for updates…");
    start_update_check(owner);
}

void updater_shutdown(void) {
    if (g_update_font) {
        DeleteObject(g_update_font);
        g_update_font = NULL;
    }
    g_update_popup = NULL;
}

static int is_absolute_executable_path(const wchar_t *path) {
    size_t length;
    if (!path) return 0;
    length = wcslen(path);
    if (length < 5 || _wcsicmp(path + length - 4, L".exe") != 0) return 0;
    return (length > 2 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) ||
           (length > 1 && path[0] == L'\\' && path[1] == L'\\');
}

int updater_apply_if_requested(void) {
    int argument_count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    DWORD old_process_id;
    wchar_t *process_id_end;
    HANDLE old_process = NULL;
    wchar_t own_path[UPDATE_PATH_CAPACITY];
    wchar_t command[UPDATE_PATH_CAPACITY + 4];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    int copied = 0;
    if (!arguments) return 0;
    if (argument_count != 4 || wcscmp(arguments[1], L"--apply-update") != 0) {
        LocalFree(arguments);
        return 0;
    }
    old_process_id = wcstoul(arguments[3], &process_id_end, 10);
    if (!old_process_id || !process_id_end || *process_id_end != L'\0' ||
        !is_absolute_executable_path(arguments[2]) ||
        !GetModuleFileNameW(NULL, own_path, _countof(own_path)) ||
        _wcsicmp(own_path, arguments[2]) == 0) {
        LocalFree(arguments);
        return 1;
    }
    old_process = OpenProcess(SYNCHRONIZE, FALSE, old_process_id);
    if (old_process) {
        WaitForSingleObject(old_process, 30000);
        CloseHandle(old_process);
    }
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (CopyFileW(own_path, arguments[2], FALSE)) {
            copied = 1;
            break;
        }
        Sleep(250);
    }
    if (!copied) {
        MessageBoxW(NULL,
                    L"The verified update was downloaded, but Windows could not replace the old executable.\n\n"
                    L"Move Persistent Calculator to a folder where your account has write permission and try again.",
                    L"Persistent Calculator update", MB_OK | MB_ICONERROR);
        LocalFree(arguments);
        return 1;
    }
    _snwprintf(command, _countof(command), L"\"%ls\"", arguments[2]);
    command[_countof(command) - 1] = L'\0';
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    if (CreateProcessW(arguments[2], command, NULL, NULL, FALSE, 0,
                       NULL, NULL, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    MoveFileExW(own_path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    LocalFree(arguments);
    return 1;
}
