#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "Comctl32.lib")

// ------------------------------------------
//                  Structures
// ------------------------------------------
typedef struct {
    char name[100];
    char phone[30];
    char email[100];
    char date[11]; // Date format is not strictly validated in this example
} Contact;

// ------------------------------------------
//              Global Definitions
// ------------------------------------------
#define MAX_CONTACTS 1000

// RSA keys (small and insecure, for demonstration only)
static const int RSA_n = 3233;   // 61*53
static const int RSA_e = 17;     // public exponent
static const int RSA_d = 2753;   // private exponent

// Menu IDs
enum {
    IDM_ADD = 1,
    IDM_DELETE,
    IDM_SAVE,
    IDM_LOAD,
    IDM_SORT_NAME,
    IDM_EXIT
};

// Control IDs
#define IDC_MAIN_LISTVIEW  100
#define IDC_SEARCH_LABEL   110
#define IDC_SEARCH_EDIT    111
#define IDC_SEARCH_BUTTON  112
#define IDC_CLEAR_BUTTON   113

// Dialog IDs (Add Contact)
#define IDD_ADD_DIALOG     101
// Dialog controls
// 1001 - Name Edit
// 1002 - Phone Edit
// 1003 - Email Edit
// 1004 - Date Edit

// ------------------------------------------
//              Global Variables
// ------------------------------------------
static Contact g_contacts[MAX_CONTACTS];
static int g_contact_count = 0;

static HWND g_hMainWnd = NULL;
static HWND g_hListView = NULL;
static HWND g_hSearchLabel = NULL;
static HWND g_hSearchEdit = NULL;
static HWND g_hSearchButton = NULL;
static HWND g_hClearButton = NULL;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AddDlgProc(HWND, UINT, WPARAM, LPARAM);

void InitListViewColumns(HWND hListView);
void DisplayContacts(HWND hListView, const char *filter);
void AddContactDialog(HWND hwnd);
void AddContact(const char *name, const char *phone, const char *email, const char *date);
void DeleteSelectedContact(HWND hListView);
void SaveEncrypted(const char *filename);
void LoadEncrypted(const char *filename);
int  ValidateName(const char *name);
int  ValidatePhone(const char *phone);
int  ValidateEmail(const char *email);
void SortByName();
int  RSA_EncryptChar(int m);
int  RSA_DecryptChar(int c);
int  modExp(int base, int exp, int mod);
void ShowError(const char *msg);
void ShowInfo(const char *msg);
void PerformSearch();
void ClearSearchFilter();

// ------------------------------------------
//                 WinMain
// ------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {0};
    wc.style = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "ContactManagerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    g_hMainWnd = CreateWindow("ContactManagerClass", "Contact Management System",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              700, 500, NULL, NULL, hInst, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(g_hMainWnd, nShowCmd);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// ------------------------------------------
//              Window Procedure
// ------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HMENU hMenu;
    switch(msg) {
        case WM_CREATE: {
            // Create Menu
            hMenu = CreateMenu();
            if (!hMenu) {
                ShowError("Failed to create menu.");
                PostQuitMessage(1);
                break;
            }

            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, IDM_ADD,       "Add Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_DELETE,    "Delete Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_SORT_NAME, "Sort by Name");
            AppendMenu(hFileMenu, MF_STRING, IDM_SAVE,      "Save (Encrypted)");
            AppendMenu(hFileMenu, MF_STRING, IDM_LOAD,      "Load (Decrypted)");
            AppendMenu(hFileMenu, MF_STRING, IDM_EXIT,      "Exit");
            AppendMenu(hMenu, MF_STRING|MF_POPUP, (UINT_PTR)hFileMenu, "File");
            SetMenu(hwnd, hMenu);

            // Create Search Toolbar (label, edit box, and buttons)
            g_hSearchLabel = CreateWindow("STATIC", "Search:", 
                                          WS_CHILD | WS_VISIBLE,
                                          10, 10, 50, 20,
                                          hwnd, (HMENU)IDC_SEARCH_LABEL,
                                          GetModuleHandle(NULL), NULL);

            g_hSearchEdit = CreateWindow("EDIT", "",
                                         WS_CHILD | WS_VISIBLE | WS_BORDER,
                                         70, 10, 200, 20,
                                         hwnd, (HMENU)IDC_SEARCH_EDIT,
                                         GetModuleHandle(NULL), NULL);

            g_hSearchButton = CreateWindow("BUTTON", "Go",
                                           WS_CHILD | WS_VISIBLE,
                                           280, 10, 40, 20,
                                           hwnd, (HMENU)IDC_SEARCH_BUTTON,
                                           GetModuleHandle(NULL), NULL);

            g_hClearButton = CreateWindow("BUTTON", "Clear",
                                          WS_CHILD | WS_VISIBLE,
                                          330, 10, 50, 20,
                                          hwnd, (HMENU)IDC_CLEAR_BUTTON,
                                          GetModuleHandle(NULL), NULL);

            // Create ListView
            g_hListView = CreateWindow(WC_LISTVIEW, "",
                                       WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                       10, 40, 660, 400,
                                       hwnd, (HMENU)IDC_MAIN_LISTVIEW, GetModuleHandle(NULL), NULL);
            if (!g_hListView) {
                ShowError("Failed to create ListView.");
                PostQuitMessage(1);
                break;
            }

            InitListViewColumns(g_hListView);
            break;
        }

        case WM_SIZE: {
            // Resizing controls according to window size
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // Keep the search bar at the top
            MoveWindow(g_hSearchLabel, 10, 10, 50, 20, TRUE);
            MoveWindow(g_hSearchEdit, 70, 10, 200, 20, TRUE);
            MoveWindow(g_hSearchButton, 280, 10, 40, 20, TRUE);
            MoveWindow(g_hClearButton, 330, 10, 50, 20, TRUE);

            // Adjust ListView height
            MoveWindow(g_hListView, 10, 40, width - 20, height - 50, TRUE);
            break;
        }

        case WM_COMMAND: {
            switch(LOWORD(wParam)) {
                case IDM_ADD:
                    AddContactDialog(hwnd);
                    break;
                case IDM_DELETE:
                    if (g_contact_count == 0) {
                        ShowInfo("No contacts to delete.");
                    } else {
                        DeleteSelectedContact(g_hListView);
                    }
                    break;
                case IDM_SAVE:
                    if (g_contact_count == 0) {
                        ShowInfo("No contacts to save.");
                    } else {
                        SaveEncrypted("contacts.enc");
                    }
                    break;
                case IDM_LOAD:
                    LoadEncrypted("contacts.enc");
                    DisplayContacts(g_hListView, NULL); // Display all
                    break;
                case IDM_SORT_NAME:
                    if (g_contact_count > 1) {
                        SortByName();
                        DisplayContacts(g_hListView, NULL);
                    } else {
                        ShowInfo("Not enough contacts to sort.");
                    }
                    break;
                case IDM_EXIT:
                    PostQuitMessage(0);
                    break;
                case IDC_SEARCH_BUTTON:
                    PerformSearch();
                    break;
                case IDC_CLEAR_BUTTON:
                    ClearSearchFilter();
                    break;
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ------------------------------------------
//         ListView Initialization
// ------------------------------------------
void InitListViewColumns(HWND hListView) {
    LVCOLUMN lvCol;
    ZeroMemory(&lvCol, sizeof(lvCol));
    lvCol.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;

    lvCol.pszText = "Name";
    lvCol.cx = 150;
    ListView_InsertColumn(hListView, 0, &lvCol);

    lvCol.pszText = "Phone";
    lvCol.cx = 100;
    ListView_InsertColumn(hListView, 1, &lvCol);

    lvCol.pszText = "Email";
    lvCol.cx = 200;
    ListView_InsertColumn(hListView, 2, &lvCol);

    lvCol.pszText = "Date";
    lvCol.cx = 100;
    ListView_InsertColumn(hListView, 3, &lvCol);
}

// ------------------------------------------
//    Display contacts (optionally filtered)
// ------------------------------------------
void DisplayContacts(HWND hListView, const char *filter) {
    ListView_DeleteAllItems(hListView);

    LVITEM lvItem;
    ZeroMemory(&lvItem, sizeof(lvItem));
    lvItem.mask = LVIF_TEXT;

    for (int i = 0; i < g_contact_count; i++) {
        if (filter && filter[0] != '\0') {
            // If filter is provided, skip contacts not matching the name
            if (strstr(g_contacts[i].name, filter) == NULL) {
                continue; 
            }
        }

        lvItem.iItem = ListView_GetItemCount(hListView);
        lvItem.iSubItem = 0;
        lvItem.pszText = g_contacts[i].name;
        int idx = ListView_InsertItem(hListView, &lvItem);

        ListView_SetItemText(hListView, idx, 1, g_contacts[i].phone);
        ListView_SetItemText(hListView, idx, 2, g_contacts[i].email);
        ListView_SetItemText(hListView, idx, 3, g_contacts[i].date);
    }
}

// ------------------------------------------
//  Add Contact Dialog Box and related code
// ------------------------------------------
void AddContactDialog(HWND hwnd) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADD_DIALOG), hwnd, AddDlgProc);
}

INT_PTR CALLBACK AddDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char name[100], phone[30], email[100], date[11];
                GetDlgItemText(hDlg, 1001, name, 100);
                GetDlgItemText(hDlg, 1002, phone, 30);
                GetDlgItemText(hDlg, 1003, email, 100);
                GetDlgItemText(hDlg, 1004, date, 11);

                // Trim whitespace
                for (int i=(int)strlen(name)-1; i>=0 && isspace((unsigned char)name[i]); i--) name[i]=0;
                for (int i=(int)strlen(phone)-1; i>=0 && isspace((unsigned char)phone[i]); i--) phone[i]=0;
                for (int i=(int)strlen(email)-1; i>=0 && isspace((unsigned char)email[i]); i--) email[i]=0;
                for (int i=(int)strlen(date)-1; i>=0 && isspace((unsigned char)date[i]); i--) date[i]=0;

                // Validate input
                if (!ValidateName(name)) {
                    MessageBox(hDlg, "Invalid name! Must not be empty.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }
                if (!ValidatePhone(phone)) {
                    MessageBox(hDlg, "Invalid phone! Digits, '+' and '-' only.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }
                if (!ValidateEmail(email)) {
                    MessageBox(hDlg, "Invalid email! Must contain '@'.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }

                // Add the contact
                AddContact(name, phone, email, date);

                // Refresh display
                DisplayContacts(g_hListView, NULL);

                EndDialog(hDlg, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
            }
            return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

// ------------------------------------------
//      Add a contact to the global array
// ------------------------------------------
void AddContact(const char *name, const char *phone, const char *email, const char *date) {
    if (g_contact_count >= MAX_CONTACTS) {
        ShowError("Contact list is full!");
        return;
    }
    Contact c;
    strncpy(c.name, name, 100);    c.name[99] = '\0';
    strncpy(c.phone, phone, 30);   c.phone[29] = '\0';
    strncpy(c.email, email, 100);  c.email[99] = '\0';
    strncpy(c.date, date, 11);     c.date[10] = '\0';

    g_contacts[g_contact_count++] = c;
}

// ------------------------------------------
//     Delete selected contact from ListView
// ------------------------------------------
void DeleteSelectedContact(HWND hListView) {
    int sel = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (sel == -1) {
        ShowInfo("No contact selected!");
        return;
    }

    int res = MessageBox(g_hMainWnd, "Are you sure you want to delete this contact?", "Confirm", MB_YESNO|MB_ICONQUESTION);
    if (res == IDYES) {
        for (int i = sel; i < g_contact_count-1; i++) {
            g_contacts[i] = g_contacts[i+1];
        }
        g_contact_count--;
        DisplayContacts(hListView, NULL);
    }
}

// ------------------------------------------
//        Save all contacts encrypted
// ------------------------------------------
void SaveEncrypted(const char *filename) {
    char buffer[100000];
    buffer[0] = '\0';

    for (int i = 0; i < g_contact_count; i++) {
        char line[512];
        snprintf(line, sizeof(line), "%s,%s,%s,%s\n",
                 g_contacts[i].name, g_contacts[i].phone, g_contacts[i].email, g_contacts[i].date);
        if (strlen(buffer) + strlen(line) < sizeof(buffer)) {
            strcat(buffer, line);
        } else {
            ShowError("Contact data too large to save!");
            return;
        }
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        ShowError("Failed to open file for writing.");
        return;
    }

    for (size_t i = 0; i < strlen(buffer); i++) {
        int c = (unsigned char)buffer[i];
        int enc = RSA_EncryptChar(c);
        if (fwrite(&enc, sizeof(enc), 1, f) != 1) {
            ShowError("File write error.");
            fclose(f);
            return;
        }
    }
    fclose(f);
    ShowInfo("Contacts saved and encrypted!");
}

// ------------------------------------------
//          Load all contacts decrypted
// ------------------------------------------
void LoadEncrypted(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        ShowInfo("No encrypted file found to load.");
        return;
    }

    g_contact_count = 0;
    char buffer[100000];
    int pos = 0;

    int enc;
    while (fread(&enc, sizeof(enc), 1, f) == 1) {
        int dec = RSA_DecryptChar(enc);
        if (pos < (int)sizeof(buffer)-1) {
            buffer[pos++] = (char)dec;
        } else {
            ShowError("Buffer overflow while loading data!");
            fclose(f);
            return;
        }
    }
    buffer[pos] = '\0';
    fclose(f);

    char *context = NULL;
#ifdef _MSC_VER
    // For MSVC, use strtok_s
    char *line = strtok_s(buffer, "\n", &context);
#else
    // For other compilers, use POSIX strtok_r
    char *line = strtok_r(buffer, "\n", &context);
#endif

    while (line) {
        Contact c;
        char *token_ctx = NULL;

#ifdef _MSC_VER
        char *token = strtok_s(line, ",", &token_ctx);
#else
        char *token = strtok_r(line, ",", &token_ctx);
#endif

        if (!token) { 
#ifdef _MSC_VER
            line = strtok_s(NULL, "\n", &context); 
#else
            line = strtok_r(NULL, "\n", &context);
#endif
            continue; 
        }
        strncpy(c.name, token, 100); c.name[99] = '\0';

#ifdef _MSC_VER
        token = strtok_s(NULL, ",", &token_ctx);
#else
        token = strtok_r(NULL, ",", &token_ctx);
#endif
        if (!token) { 
#ifdef _MSC_VER
            line = strtok_s(NULL, "\n", &context); 
#else
            line = strtok_r(NULL, "\n", &context);
#endif
            continue; 
        }
        strncpy(c.phone, token, 30); c.phone[29] = '\0';

#ifdef _MSC_VER
        token = strtok_s(NULL, ",", &token_ctx);
#else
        token = strtok_r(NULL, ",", &token_ctx);
#endif
        if (!token) { 
#ifdef _MSC_VER
            line = strtok_s(NULL, "\n", &context); 
#else
            line = strtok_r(NULL, "\n", &context);
#endif
            continue; 
        }
        strncpy(c.email, token, 100); c.email[99] = '\0';

#ifdef _MSC_VER
        token = strtok_s(NULL, ",", &token_ctx);
#else
        token = strtok_r(NULL, ",", &token_ctx);
#endif
        if (!token) { 
#ifdef _MSC_VER
            line = strtok_s(NULL, "\n", &context); 
#else
            line = strtok_r(NULL, "\n", &context);
#endif
            continue; 
        }
        strncpy(c.date, token, 11); c.date[10] = '\0';

        if (g_contact_count < MAX_CONTACTS) {
            g_contacts[g_contact_count++] = c;
        } else {
            ShowError("Too many contacts loaded!");
            break;
        }

#ifdef _MSC_VER
        line = strtok_s(NULL, "\n", &context);
#else
        line = strtok_r(NULL, "\n", &context);
#endif
    }

    ShowInfo("Contacts loaded and decrypted!");
}

// ------------------------------------------
//                Validation
// ------------------------------------------
int ValidateName(const char *name) {
    return (strlen(name) > 0);
}

int ValidatePhone(const char *phone) {
    for (int i = 0; i < (int)strlen(phone); i++) {
        if (!isdigit((unsigned char)phone[i]) && phone[i] != '+' && phone[i] != '-')
            return 0;
    }
    return 1;
}

int ValidateEmail(const char *email) {
    return (strchr(email, '@') != NULL);
}

// ------------------------------------------
//                Sorting
// ------------------------------------------
int name_cmp(const void *a, const void *b) {
    const Contact *c1 = (const Contact*)a;
    const Contact *c2 = (const Contact*)b;
#ifdef _MSC_VER
    return _stricmp(c1->name, c2->name);
#else
    return strcasecmp(c1->name, c2->name);
#endif
}

void SortByName() {
    qsort(g_contacts, g_contact_count, sizeof(Contact), name_cmp);
}

// ------------------------------------------
//                RSA Functions
// ------------------------------------------
int RSA_EncryptChar(int m) {
    return modExp(m, RSA_e, RSA_n);
}
int RSA_DecryptChar(int c) {
    return modExp(c, RSA_d, RSA_n);
}

int modExp(int base, int exp, int mod) {
    long long result = 1;
    long long b = base % mod;
    long long e = exp;
    while (e > 0) {
        if (e & 1) result = (result * b) % mod;
        b = (b * b) % mod;
        e >>= 1;
    }
    return (int)result;
}

// ------------------------------------------
//          Helper Functions
// ------------------------------------------
void ShowError(const char *msg) {
    MessageBox(g_hMainWnd, msg, "Error", MB_OK|MB_ICONERROR);
}

void ShowInfo(const char *msg) {
    MessageBox(g_hMainWnd, msg, "Info", MB_OK|MB_ICONINFORMATION);
}

// ------------------------------------------
//          Search Functions
// ------------------------------------------
void PerformSearch() {
    char query[256];
    GetWindowText(g_hSearchEdit, query, sizeof(query));

    // Filter the displayed contacts by the query
    DisplayContacts(g_hListView, query);
}

void ClearSearchFilter() {
    SetWindowText(g_hSearchEdit, "");
    DisplayContacts(g_hListView, NULL);
}


