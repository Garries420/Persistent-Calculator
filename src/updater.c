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
#include <windowsx.h>

#define UPDATE_POPUP_CLASS L"PersistentCalculatorUpdatePopup"
#define UPDATE_UI_MESSAGE (WM_APP + 40)
#define UPDATE_POPUP_TIMER 1
#define UPDATE_POPUP_MILLISECONDS 2000
#define UPDATE_JSON_LIMIT (2u * 1024u * 1024u)
#define UPDATE_DOWNLOAD_LIMIT (64u * 1024u * 1024u)
#define UPDATE_PATH_CAPACITY 32768

static const wchar_t *UPDATE_API_URL =
    L"https://api.github.com/repos/Garries420/Persistent-Calculator/releases/latest";

static HWND g_update_popup;
static HFONT g_update_font;
static HFONT g_update_title_font;
static HFONT g_update_small_font;
static UINT g_update_dpi;
static volatile LONG g_update_activity;
static wchar_t g_update_status[192] = L"Checking for updates…";
static UpdateReleaseInfo g_pending_release;
static int g_update_ui_mode;
static int g_update_progress;
static int g_update_stage;
static int g_update_hot_button;

enum UpdateUiMode {
    UPDATE_UI_NOTICE,
    UPDATE_UI_PROMPT,
    UPDATE_UI_PROGRESS
};

enum UpdateStage {
    UPDATE_STAGE_PREPARE,
    UPDATE_STAGE_DOWNLOAD,
    UPDATE_STAGE_VERIFY,
    UPDATE_STAGE_INSTALL
};

enum UpdateButton {
    UPDATE_BUTTON_NONE,
    UPDATE_BUTTON_NO,
    UPDATE_BUTTON_YES
};

typedef struct UpdateUiPayload {
    int mode;
    int progress;
    int stage;
    wchar_t status[192];
    UpdateReleaseInfo release;
} UpdateUiPayload;

typedef struct UpdateDownloadTask {
    HWND owner;
    UpdateReleaseInfo release;
} UpdateDownloadTask;

static void post_update_progress(int stage, int progress, const wchar_t *status);

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
    DWORD content_length = 0;
    DWORD content_length_size = sizeof(content_length);
    int last_percentage = -1;
    int success = 0;
    if (!open_https_get(url, &http)) return 0;
    WinHttpQueryHeaders(http.request,
                        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &content_length,
                        &content_length_size, WINHTTP_NO_HEADER_INDEX);
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
        {
            int download_percentage;
            int overall_percentage;
            wchar_t status[96];
            if (content_length > 0)
                download_percentage = (int)((total * 100u) / content_length);
            else {
                download_percentage = (int)(total / (128u * 1024u)) * 10;
                if (download_percentage > 90) download_percentage = 90;
            }
            if (download_percentage > 100) download_percentage = 100;
            if (download_percentage >= last_percentage + 2) {
                overall_percentage = 10 + (download_percentage * 65) / 100;
                _snwprintf(status, _countof(status),
                           L"Downloading update… %d%%", download_percentage);
                status[_countof(status) - 1] = L'\0';
                post_update_progress(UPDATE_STAGE_DOWNLOAD,
                                     overall_percentage, status);
                last_percentage = download_percentage;
            }
        }
    }
    if (!FlushFileBuffers(file) || total == 0) goto cleanup;
    post_update_progress(UPDATE_STAGE_DOWNLOAD, 75, L"Download complete.");
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

static UINT owner_dpi(HWND owner) {
    UINT dpi = owner ? GetDpiForWindow(owner) : 0;
    return dpi ? dpi : 96;
}

static int popup_scale(HWND owner, int value) {
    return MulDiv(value, (int)owner_dpi(owner), 96);
}

static HFONT create_update_font(UINT dpi, int points, int weight) {
    LOGFONTW font;
    memset(&font, 0, sizeof(font));
    font.lfHeight = -MulDiv(points, (int)dpi, 72);
    font.lfWeight = weight;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(font.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&font);
}

static void refresh_update_font(HWND owner) {
    UINT dpi = owner_dpi(owner);
    if (g_update_font && g_update_dpi == dpi) return;
    if (g_update_font) DeleteObject(g_update_font);
    if (g_update_title_font) DeleteObject(g_update_title_font);
    if (g_update_small_font) DeleteObject(g_update_small_font);
    g_update_font = create_update_font(dpi, 10, FW_NORMAL);
    g_update_title_font = create_update_font(dpi, 16, FW_SEMIBOLD);
    g_update_small_font = create_update_font(dpi, 9, FW_NORMAL);
    g_update_dpi = dpi;
}

static void set_popup_region(HWND popup, int width, int height, UINT dpi) {
    int radius = MulDiv(12, (int)dpi, 96);
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
    if (region) SetWindowRgn(popup, region, TRUE);
}

static void layout_update_popup(HWND popup) {
    HWND owner = GetParent(popup);
    RECT owner_client;
    RECT measure = {0, 0, 0, 0};
    HDC dc;
    HGDIOBJ old_font;
    UINT dpi;
    int margin, padding, vertical_padding, width, height, minimum_height, x, y;
    if (!owner || !IsWindow(owner)) return;
    dpi = owner_dpi(owner);
    refresh_update_font(owner);
    GetClientRect(owner, &owner_client);
    if (g_update_ui_mode != UPDATE_UI_NOTICE) {
        SetWindowRgn(popup, NULL, TRUE);
        SetWindowPos(popup, HWND_TOP, 0, 0, owner_client.right,
                     owner_client.bottom, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        return;
    }
    margin = MulDiv(8, (int)dpi, 96);
    padding = MulDiv(12, (int)dpi, 96);
    vertical_padding = MulDiv(9, (int)dpi, 96);
    minimum_height = MulDiv(48, (int)dpi, 96);
    width = owner_client.right - margin * 2;
    if (width < MulDiv(180, (int)dpi, 96)) width = MulDiv(180, (int)dpi, 96);
    measure.right = width - padding * 2;
    dc = GetDC(popup);
    if (dc) {
        old_font = SelectObject(dc, g_update_font ? g_update_font : GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(dc, g_update_status, -1, &measure,
                  DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        SelectObject(dc, old_font);
        ReleaseDC(popup, dc);
    } else {
        measure.bottom = minimum_height - vertical_padding * 2;
    }
    height = measure.bottom + vertical_padding * 2;
    if (height < minimum_height) height = minimum_height;
    x = (owner_client.right - width) / 2;
    y = MulDiv(56, (int)dpi, 96);
    if (y + height > owner_client.bottom - margin)
        y = owner_client.bottom - margin - height;
    if (y < margin) y = margin;
    set_popup_region(popup, width, height, dpi);
    SetWindowPos(popup, HWND_TOP, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

static RECT update_card_rect(HWND window) {
    RECT client;
    RECT card;
    int margin = popup_scale(GetParent(window), 18);
    int desired_height = popup_scale(GetParent(window),
                                     g_update_ui_mode == UPDATE_UI_PROGRESS ? 350 : 250);
    GetClientRect(window, &client);
    if (desired_height > client.bottom - margin * 2)
        desired_height = client.bottom - margin * 2;
    card.left = margin;
    card.right = client.right - margin;
    card.top = (client.bottom - desired_height) / 2;
    card.bottom = card.top + desired_height;
    return card;
}

static void update_prompt_buttons(HWND window, RECT *no_button, RECT *yes_button) {
    RECT card = update_card_rect(window);
    int margin = popup_scale(GetParent(window), 18);
    int gap = popup_scale(GetParent(window), 10);
    int button_height = popup_scale(GetParent(window), 42);
    int middle = (card.left + card.right) / 2;
    no_button->left = card.left + margin;
    no_button->right = middle - gap / 2;
    no_button->top = card.bottom - margin - button_height;
    no_button->bottom = card.bottom - margin;
    yes_button->left = middle + gap / 2;
    yes_button->right = card.right - margin;
    yes_button->top = no_button->top;
    yes_button->bottom = no_button->bottom;
}

static int update_prompt_hit(HWND window, int x, int y) {
    POINT point = {x, y};
    RECT no_button, yes_button;
    if (g_update_ui_mode != UPDATE_UI_PROMPT) return UPDATE_BUTTON_NONE;
    update_prompt_buttons(window, &no_button, &yes_button);
    if (PtInRect(&no_button, point)) return UPDATE_BUTTON_NO;
    if (PtInRect(&yes_button, point)) return UPDATE_BUTTON_YES;
    return UPDATE_BUTTON_NONE;
}

static void fill_update_round_rect(HDC dc, const RECT *rect, COLORREF fill,
                                   COLORREF outline, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_update_text(HDC dc, const wchar_t *text_value, RECT rect,
                             HFONT font, COLORREF color, UINT format) {
    HGDIOBJ old_font = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text_value, -1, &rect, format | DT_NOPREFIX);
    SelectObject(dc, old_font);
}

static void paint_update_notice(HWND window, HDC dc) {
    RECT client;
    RECT text_rect;
    RECT measure = {0, 0, 0, 0};
    int horizontal_padding = popup_scale(GetParent(window), 12);
    GetClientRect(window, &client);
    fill_update_round_rect(dc, &client, RGB(48, 48, 48), RGB(91, 91, 91),
                           popup_scale(GetParent(window), 12));
    text_rect = client;
    text_rect.left += horizontal_padding;
    text_rect.right -= horizontal_padding;
    measure.right = text_rect.right - text_rect.left;
    {
        HGDIOBJ old_font = SelectObject(dc, g_update_font ? g_update_font :
                                                   GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(dc, g_update_status, -1, &measure,
                  DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        SelectObject(dc, old_font);
    }
    text_rect.top = (client.bottom - measure.bottom) / 2;
    text_rect.bottom = text_rect.top + measure.bottom;
    draw_update_text(dc, g_update_status, text_rect, g_update_font,
                     RGB(242, 242, 242), DT_CENTER | DT_WORDBREAK);
}

static void paint_update_prompt(HWND window, HDC dc) {
    RECT client;
    RECT card = update_card_rect(window);
    RECT no_button, yes_button;
    RECT title, body;
    int margin = popup_scale(GetParent(window), 18);
    GetClientRect(window, &client);
    {
        HBRUSH dim = CreateSolidBrush(RGB(15, 15, 15));
        FillRect(dc, &client, dim);
        DeleteObject(dim);
    }
    fill_update_round_rect(dc, &card, RGB(45, 45, 45), RGB(100, 100, 100),
                           popup_scale(GetParent(window), 12));
    title.left = card.left + margin;
    title.right = card.right - margin;
    title.top = card.top + margin;
    title.bottom = title.top + popup_scale(GetParent(window), 38);
    body = title;
    body.top = title.bottom + popup_scale(GetParent(window), 8);
    body.bottom = card.bottom - popup_scale(GetParent(window), 78);
    draw_update_text(dc, L"Update available", title, g_update_title_font,
                     RGB(248, 248, 248), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_update_text(dc, g_update_status, body, g_update_font,
                     RGB(224, 224, 224), DT_LEFT | DT_TOP | DT_WORDBREAK);
    update_prompt_buttons(window, &no_button, &yes_button);
    fill_update_round_rect(dc, &no_button,
                           g_update_hot_button == UPDATE_BUTTON_NO ? RGB(73, 73, 73) : RGB(58, 58, 58),
                           RGB(95, 95, 95), popup_scale(GetParent(window), 6));
    fill_update_round_rect(dc, &yes_button,
                           g_update_hot_button == UPDATE_BUTTON_YES ? RGB(176, 213, 230) : RGB(156, 198, 217),
                           RGB(190, 224, 238), popup_scale(GetParent(window), 6));
    draw_update_text(dc, L"No, not now", no_button, g_update_font,
                     RGB(245, 245, 245), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    draw_update_text(dc, L"Yes, update", yes_button, g_update_font,
                     RGB(25, 45, 55), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paint_update_progress(HWND window, HDC dc) {
    static const wchar_t *steps[] = {
        L"Preparing update", L"Downloading update", L"Verifying SHA-256",
        L"Installing and restarting"
    };
    RECT client;
    RECT card = update_card_rect(window);
    RECT title, status, track, fill;
    int margin = popup_scale(GetParent(window), 18);
    int index;
    int row_top;
    wchar_t percentage[32];
    GetClientRect(window, &client);
    {
        HBRUSH dim = CreateSolidBrush(RGB(15, 15, 15));
        FillRect(dc, &client, dim);
        DeleteObject(dim);
    }
    fill_update_round_rect(dc, &card, RGB(45, 45, 45), RGB(100, 100, 100),
                           popup_scale(GetParent(window), 12));
    title.left = card.left + margin;
    title.right = card.right - margin;
    title.top = card.top + margin;
    title.bottom = title.top + popup_scale(GetParent(window), 36);
    status = title;
    status.top = title.bottom + popup_scale(GetParent(window), 5);
    status.bottom = status.top + popup_scale(GetParent(window), 42);
    draw_update_text(dc, L"Updating Persistent Calculator", title,
                     g_update_title_font, RGB(248, 248, 248),
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_update_text(dc, g_update_status, status, g_update_font,
                     RGB(224, 224, 224), DT_LEFT | DT_TOP | DT_WORDBREAK);

    track.left = card.left + margin;
    track.right = card.right - margin;
    track.top = status.bottom + popup_scale(GetParent(window), 10);
    track.bottom = track.top + popup_scale(GetParent(window), 12);
    fill_update_round_rect(dc, &track, RGB(70, 70, 70), RGB(70, 70, 70),
                           popup_scale(GetParent(window), 6));
    fill = track;
    fill.right = fill.left + ((fill.right - fill.left) * g_update_progress) / 100;
    if (fill.right > fill.left)
        fill_update_round_rect(dc, &fill, RGB(156, 198, 217), RGB(156, 198, 217),
                               popup_scale(GetParent(window), 6));
    _snwprintf(percentage, _countof(percentage), L"%d%%", g_update_progress);
    percentage[_countof(percentage) - 1] = L'\0';
    {
        RECT percent_rect = {track.left, track.bottom + popup_scale(GetParent(window), 2),
                             track.right, track.bottom + popup_scale(GetParent(window), 23)};
        draw_update_text(dc, percentage, percent_rect, g_update_small_font,
                         RGB(180, 185, 186), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    row_top = track.bottom + popup_scale(GetParent(window), 31);
    for (index = 0; index < (int)_countof(steps); ++index) {
        RECT marker = {card.left + margin, row_top,
                       card.left + margin + popup_scale(GetParent(window), 24),
                       row_top + popup_scale(GetParent(window), 32)};
        RECT row = {marker.right, row_top, card.right - margin,
                    row_top + popup_scale(GetParent(window), 32)};
        const wchar_t *symbol = index < g_update_stage ? L"✓" :
                                index == g_update_stage ? L"●" : L"○";
        COLORREF color = index <= g_update_stage ? RGB(220, 230, 232) : RGB(125, 130, 131);
        draw_update_text(dc, symbol, marker, g_update_font,
                         index < g_update_stage ? RGB(156, 198, 217) : color,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_update_text(dc, steps[index], row, g_update_font, color,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        row_top += popup_scale(GetParent(window), 36);
    }
}

static void start_update_download(HWND owner);

static LRESULT CALLBACK update_popup_proc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_TIMER:
            if (wparam == UPDATE_POPUP_TIMER) {
                KillTimer(window, UPDATE_POPUP_TIMER);
                ShowWindow(window, SW_HIDE);
            }
            return 0;
        case UPDATE_UI_MESSAGE: {
            UpdateUiPayload *payload = (UpdateUiPayload *)lparam;
            if (payload) {
                g_update_ui_mode = payload->mode;
                g_update_progress = payload->progress;
                g_update_stage = payload->stage;
                wcsncpy(g_update_status, payload->status, _countof(g_update_status) - 1);
                g_update_status[_countof(g_update_status) - 1] = L'\0';
                if (payload->mode == UPDATE_UI_PROMPT) {
                    g_pending_release = payload->release;
                    InterlockedExchange(&g_update_activity, 0);
                }
                HeapFree(GetProcessHeap(), 0, payload);
                KillTimer(window, UPDATE_POPUP_TIMER);
                layout_update_popup(window);
                ShowWindow(window, SW_SHOWNOACTIVATE);
                if (g_update_ui_mode == UPDATE_UI_NOTICE)
                    SetTimer(window, UPDATE_POPUP_TIMER, UPDATE_POPUP_MILLISECONDS, NULL);
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }
        case WM_NCHITTEST:
            return g_update_ui_mode == UPDATE_UI_NOTICE ? HTTRANSPARENT : HTCLIENT;
        case WM_MOUSEMOVE: {
            int hot = update_prompt_hit(window, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track);
            if (hot != g_update_hot_button) {
                g_update_hot_button = hot;
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            g_update_hot_button = UPDATE_BUTTON_NONE;
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            int button = update_prompt_hit(window, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            if (button == UPDATE_BUTTON_NO) {
                g_update_hot_button = UPDATE_BUTTON_NONE;
                ShowWindow(window, SW_HIDE);
            } else if (button == UPDATE_BUTTON_YES) {
                g_update_hot_button = UPDATE_BUTTON_NONE;
                g_update_ui_mode = UPDATE_UI_PROGRESS;
                g_update_stage = UPDATE_STAGE_PREPARE;
                g_update_progress = 3;
                wcscpy(g_update_status, L"Preparing the verified download…");
                layout_update_popup(window);
                InvalidateRect(window, NULL, FALSE);
                start_update_download(GetParent(window));
            }
            return 0;
        }
        case WM_SETCURSOR:
            if (g_update_ui_mode == UPDATE_UI_PROMPT &&
                g_update_hot_button != UPDATE_BUTTON_NONE) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            if (g_update_ui_mode == UPDATE_UI_PROMPT) paint_update_prompt(window, dc);
            else if (g_update_ui_mode == UPDATE_UI_PROGRESS) paint_update_progress(window, dc);
            else paint_update_notice(window, dc);
            EndPaint(window, &paint);
            return 0;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static int ensure_popup(HWND owner) {
    WNDCLASSEXW window_class;
    HINSTANCE instance = GetModuleHandleW(NULL);
    if (g_update_popup && IsWindow(g_update_popup)) {
        if (GetParent(g_update_popup) != owner) SetParent(g_update_popup, owner);
        return 1;
    }
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = update_popup_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = NULL;
    window_class.lpszClassName = UPDATE_POPUP_CLASS;
    RegisterClassExW(&window_class);
    g_update_popup = CreateWindowExW(WS_EX_NOACTIVATE,
                                     UPDATE_POPUP_CLASS, L"",
                                     WS_CHILD | WS_CLIPSIBLINGS,
                                     0, 0, 300, 58, owner, NULL, instance, NULL);
    return g_update_popup != NULL;
}

static void show_update_notice(HWND owner, const wchar_t *status) {
    if (!ensure_popup(owner)) return;
    g_update_ui_mode = UPDATE_UI_NOTICE;
    g_update_progress = 0;
    g_update_stage = 0;
    wcsncpy(g_update_status, status, _countof(g_update_status) - 1);
    g_update_status[_countof(g_update_status) - 1] = L'\0';
    layout_update_popup(g_update_popup);
    KillTimer(g_update_popup, UPDATE_POPUP_TIMER);
    ShowWindow(g_update_popup, SW_SHOWNOACTIVATE);
    SetWindowPos(g_update_popup, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                 SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    SetTimer(g_update_popup, UPDATE_POPUP_TIMER, UPDATE_POPUP_MILLISECONDS, NULL);
    RedrawWindow(g_update_popup, NULL, NULL,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

static int post_update_payload(const UpdateUiPayload *source) {
    UpdateUiPayload *copy;
    HWND popup = g_update_popup;
    if (!popup || !IsWindow(popup)) return 0;
    copy = (UpdateUiPayload *)HeapAlloc(GetProcessHeap(), 0, sizeof(*copy));
    if (!copy) return 0;
    *copy = *source;
    if (!PostMessageW(popup, UPDATE_UI_MESSAGE, 0, (LPARAM)copy)) {
        HeapFree(GetProcessHeap(), 0, copy);
        return 0;
    }
    return 1;
}

static void post_update_notice(const wchar_t *status) {
    UpdateUiPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.mode = UPDATE_UI_NOTICE;
    wcsncpy(payload.status, status, _countof(payload.status) - 1);
    post_update_payload(&payload);
}

static int post_update_prompt(const UpdateReleaseInfo *release) {
    UpdateUiPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.mode = UPDATE_UI_PROMPT;
    payload.release = *release;
    _snwprintf(payload.status, _countof(payload.status),
               L"Persistent Calculator %hs is available. You are using %ls. Would you like to update now?",
               release->version, PERSISTENT_CALCULATOR_VERSION_W);
    payload.status[_countof(payload.status) - 1] = L'\0';
    return post_update_payload(&payload);
}

static void post_update_progress(int stage, int progress, const wchar_t *status) {
    UpdateUiPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.mode = UPDATE_UI_PROGRESS;
    payload.stage = stage;
    payload.progress = progress < 0 ? 0 : progress > 100 ? 100 : progress;
    wcsncpy(payload.status, status, _countof(payload.status) - 1);
    post_update_payload(&payload);
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
    char *json = NULL;
    size_t json_size = 0;
    UpdateReleaseInfo release;
    wchar_t status[192];
    (void)parameter;

    if (!http_get_memory(UPDATE_API_URL, &json, &json_size) || json_size == 0) {
        post_update_notice(L"Update check unavailable. The calculator remains fully usable.");
        goto done;
    }
    if (!update_parse_release_json(json, &release)) {
        post_update_notice(L"The release information could not be verified safely.");
        goto done;
    }
    if (update_compare_versions(release.version, PERSISTENT_CALCULATOR_VERSION_A) <= 0) {
        _snwprintf(status, _countof(status),
                   L"Persistent Calculator is up to date — version %ls.",
                   PERSISTENT_CALCULATOR_VERSION_W);
        status[_countof(status) - 1] = L'\0';
        post_update_notice(status);
        goto done;
    }

    if (json) HeapFree(GetProcessHeap(), 0, json);
    if (!post_update_prompt(&release))
        InterlockedExchange(&g_update_activity, 0);
    return 0;

done:
    if (json) HeapFree(GetProcessHeap(), 0, json);
    InterlockedExchange(&g_update_activity, 0);
    return 0;
}

static DWORD WINAPI update_download_thread(void *parameter) {
    UpdateDownloadTask *task = (UpdateDownloadTask *)parameter;
    HWND owner;
    UpdateReleaseInfo release;
    wchar_t download_url[2048];
    wchar_t current_path[UPDATE_PATH_CAPACITY];
    wchar_t temporary_directory[UPDATE_PATH_CAPACITY];
    wchar_t download_path[UPDATE_PATH_CAPACITY];
    char actual_sha256[65];
    DWORD path_length;
    if (!task) {
        InterlockedExchange(&g_update_activity, 0);
        return 0;
    }
    owner = task->owner;
    release = task->release;
    HeapFree(GetProcessHeap(), 0, task);

    post_update_progress(UPDATE_STAGE_PREPARE, 5,
                         L"Preparing the verified download…");
    if (update_compare_versions(release.version,
                                PERSISTENT_CALCULATOR_VERSION_A) <= 0 ||
        !utf8_to_wide(release.download_url, download_url,
                      _countof(download_url))) {
        post_update_notice(L"The update information was no longer valid.");
        goto done;
    }
    path_length = GetModuleFileNameW(NULL, current_path, _countof(current_path));
    if (!path_length || path_length >= _countof(current_path) ||
        !(path_length = GetTempPathW(_countof(temporary_directory), temporary_directory)) ||
        path_length >= _countof(temporary_directory)) {
        post_update_notice(L"Windows could not prepare the update location.");
        goto done;
    }
    if (_snwprintf(download_path, _countof(download_path),
                   L"%lsPersistentCalculator-Update-%lu.exe",
                   temporary_directory, GetCurrentProcessId()) < 0) {
        post_update_notice(L"Windows could not prepare the update location.");
        goto done;
    }
    DeleteFileW(download_path);
    post_update_progress(UPDATE_STAGE_DOWNLOAD, 10, L"Downloading update… 0%");
    if (!http_download_file(download_url, download_path)) {
        DeleteFileW(download_path);
        post_update_notice(L"The update download failed. Nothing was installed.");
        goto done;
    }
    post_update_progress(UPDATE_STAGE_VERIFY, 82,
                         L"Verifying the downloaded update…");
    if (!sha256_file(download_path, actual_sha256) ||
        strcmp(actual_sha256, release.sha256) != 0) {
        DeleteFileW(download_path);
        post_update_notice(L"Update cancelled because SHA-256 verification failed.");
        goto done;
    }
    post_update_progress(UPDATE_STAGE_INSTALL, 95,
                         L"Update verified. Starting the installer…");
    if (!launch_update_helper(download_path, current_path)) {
        DeleteFileW(download_path);
        post_update_notice(L"The verified update could not be started.");
        goto done;
    }
    post_update_progress(UPDATE_STAGE_INSTALL, 100,
                         L"Installation started. Restarting now…");
    Sleep(350);
    PostMessageW(owner, WM_CLOSE, 0, 0);

done:
    InterlockedExchange(&g_update_activity, 0);
    return 0;
}

static void start_update_check(HWND owner) {
    HANDLE thread;
    if (InterlockedCompareExchange(&g_update_activity, 1, 0) != 0) {
        show_update_notice(owner, L"An update operation is already running.");
        return;
    }
    thread = CreateThread(NULL, 0, update_check_thread, owner, 0, NULL);
    if (!thread) {
        InterlockedExchange(&g_update_activity, 0);
        show_update_notice(owner, L"Windows could not start the update check.");
        return;
    }
    CloseHandle(thread);
}

static void start_update_download(HWND owner) {
    HANDLE thread;
    UpdateDownloadTask *task;
    if (InterlockedCompareExchange(&g_update_activity, 1, 0) != 0) {
        post_update_notice(L"An update operation is already running.");
        return;
    }
    task = (UpdateDownloadTask *)HeapAlloc(GetProcessHeap(), 0, sizeof(*task));
    if (!task) {
        InterlockedExchange(&g_update_activity, 0);
        post_update_notice(L"Windows could not prepare the update.");
        return;
    }
    task->owner = owner;
    task->release = g_pending_release;
    thread = CreateThread(NULL, 0, update_download_thread, task, 0, NULL);
    if (!thread) {
        HeapFree(GetProcessHeap(), 0, task);
        InterlockedExchange(&g_update_activity, 0);
        post_update_notice(L"Windows could not start the update.");
        return;
    }
    CloseHandle(thread);
}

void updater_initialize(HWND owner) {
    refresh_update_font(owner);
    show_update_notice(owner, L"Checking GitHub securely for updates…");
    start_update_check(owner);
}

void updater_check_now(HWND owner) {
    show_update_notice(owner, L"Checking GitHub securely for updates…");
    start_update_check(owner);
}

int updater_is_modal(void) {
    return g_update_popup && IsWindow(g_update_popup) &&
           IsWindowVisible(g_update_popup) &&
           g_update_ui_mode != UPDATE_UI_NOTICE;
}

void updater_owner_resized(HWND owner) {
    if (!g_update_popup || !IsWindow(g_update_popup) || GetParent(g_update_popup) != owner) return;
    refresh_update_font(owner);
    layout_update_popup(g_update_popup);
    InvalidateRect(g_update_popup, NULL, FALSE);
}

void updater_shutdown(void) {
    if (g_update_font) {
        DeleteObject(g_update_font);
        g_update_font = NULL;
    }
    if (g_update_title_font) {
        DeleteObject(g_update_title_font);
        g_update_title_font = NULL;
    }
    if (g_update_small_font) {
        DeleteObject(g_update_small_font);
        g_update_small_font = NULL;
    }
    g_update_dpi = 0;
    g_update_ui_mode = UPDATE_UI_NOTICE;
    memset(&g_pending_release, 0, sizeof(g_pending_release));
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
