#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "Comctl32.lib")

// ------------------------------------------
//               Data Structures
// ------------------------------------------
typedef struct {
    char name[100];
    char phone[30];
    char email[100];
    char date[11]; // "YYYY-MM-DD" or empty
} Contact;

// ------------------------------------------
//              Global Definitions
// ------------------------------------------
#define MAX_CONTACTS 1000

// RSA keys (small, insecure, demonstration only)
static const int RSA_n = 3233;   // for example 61*53
static const int RSA_e = 17;     // public exponent
static const int RSA_d = 2753;   // private exponent

// Menu and Control IDs
enum {
    IDM_ADD = 1,
    IDM_EDIT,
    IDM_DELETE,
    IDM_SAVE,
    IDM_LOAD,
    IDM_SORT_NAME,
    IDM_EXIT
};

#define IDC_MAIN_LISTVIEW  100
#define IDC_SEARCH_LABEL   110
#define IDC_SEARCH_EDIT    111
#define IDC_SEARCH_BUTTON  112
#define IDC_CLEAR_BUTTON   113
#define IDD_ADD_DIALOG     101

// ------------------------------------------
//              Global Variables
// ------------------------------------------
static Contact g_contacts[MAX_CONTACTS];
static int g_contactCount = 0;

static HWND g_hMainWnd = NULL;
static HWND g_hListView = NULL;

// If -1, adding a new contact; otherwise editing existing contact
static int g_editIndex = -1;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ContactDlgProc(HWND, UINT, WPARAM, LPARAM);

void InitializeListViewColumns(HWND hListView);
void DisplayContacts(HWND hListView, const char *filter);
void ShowAddContactDialog(HWND hwnd);
void ShowEditContactDialog(HWND hwnd, int index);
void AddNewContact(const char *name, const char *phone, const char *email, const char *date);
void UpdateExistingContact(int index, const char *name, const char *phone, const char *email, const char *date);
void DeleteSelectedContact(HWND hListView);
void SaveContactsRSA(const char *filename);
void LoadContactsRSA(const char *filename);
int  ValidateName(const char *name);
int  ValidatePhone(const char *phone);
int  ValidateEmail(const char *email);
void SortContactsByName();
int  CompareContactsByName(const void *a, const void *b);
void ShowError(const char *msg);
void ShowInfo(const char *msg);
void PerformSearch();
void ClearSearchFilter();

// RSA and modular exponentiation
int RSA_EncryptChar(int m);
int RSA_DecryptChar(int c);
int modExp(int base, int exp, int mod);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ContactManagerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    g_hMainWnd = CreateWindow("ContactManagerClass", "Contact Management System",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              700, 500, NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HMENU hMenu;
    switch(msg) {
        case WM_CREATE: {
            hMenu = CreateMenu();
            if (!hMenu) {
                ShowError("Failed to create menu.");
                PostQuitMessage(1);
                break;
            }

            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, IDM_ADD,       "Add Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_EDIT,      "Edit Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_DELETE,    "Delete Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_SORT_NAME, "Sort by Name");
            AppendMenu(hFileMenu, MF_STRING, IDM_SAVE,      "Save (RSA Encrypted)");
            AppendMenu(hFileMenu, MF_STRING, IDM_LOAD,      "Load (RSA Decrypted)");
            AppendMenu(hFileMenu, MF_STRING, IDM_EXIT,      "Exit");
            AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFileMenu, "File");
            SetMenu(hwnd, hMenu);

            CreateWindow("STATIC", "Search by name:",
                         WS_CHILD | WS_VISIBLE,
                         10, 10, 100, 20,
                         hwnd, (HMENU)IDC_SEARCH_LABEL,
                         GetModuleHandle(NULL), NULL);

            CreateWindow("EDIT", "",
                         WS_CHILD | WS_VISIBLE | WS_BORDER,
                         120, 10, 200, 20,
                         hwnd, (HMENU)IDC_SEARCH_EDIT,
                         GetModuleHandle(NULL), NULL);

            CreateWindow("BUTTON", "Go",
                         WS_CHILD | WS_VISIBLE,
                         330, 10, 40, 20,
                         hwnd, (HMENU)IDC_SEARCH_BUTTON,
                         GetModuleHandle(NULL), NULL);

            CreateWindow("BUTTON", "Clear",
                         WS_CHILD | WS_VISIBLE,
                         380, 10, 50, 20,
                         hwnd, (HMENU)IDC_CLEAR_BUTTON,
                         GetModuleHandle(NULL), NULL);

            g_hListView = CreateWindow(WC_LISTVIEW, "",
                                       WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                       10, 40, 660, 400,
                                       hwnd, (HMENU)IDC_MAIN_LISTVIEW, 
                                       GetModuleHandle(NULL), NULL);
            if (!g_hListView) {
                ShowError("Failed to create ListView.");
                PostQuitMessage(1);
                break;
            }

            InitializeListViewColumns(g_hListView);
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(g_hListView, 10, 40, width - 20, height - 50, TRUE);
            break;
        }

        case WM_COMMAND: {
            switch(LOWORD(wParam)) {
                case IDM_ADD:
                    g_editIndex = -1;
                    ShowAddContactDialog(hwnd);
                    break;
                case IDM_EDIT: {
                    int selected = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                    if (selected == -1) {
                        ShowInfo("No contact selected to edit.");
                    } else {
                        g_editIndex = selected;
                        ShowEditContactDialog(hwnd, selected);
                    }
                    break;
                }
                case IDM_DELETE:
                    if (g_contactCount == 0) {
                        ShowInfo("No contacts to delete.");
                    } else {
                        DeleteSelectedContact(g_hListView);
                    }
                    break;
                case IDM_SAVE:
                    if (g_contactCount == 0) {
                        ShowInfo("No contacts to save.");
                    } else {
                        SaveContactsRSA("contacts.txt");
                    }
                    break;
                case IDM_LOAD:
                    LoadContactsRSA("contacts.txt");
                    DisplayContacts(g_hListView, NULL);
                    break;
                case IDM_SORT_NAME:
                    if (g_contactCount > 1) {
                        SortContactsByName();
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

void InitializeListViewColumns(HWND hListView) {
    LVCOLUMN columnInfo;
    ZeroMemory(&columnInfo, sizeof(columnInfo));
    columnInfo.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    columnInfo.pszText = "Name";
    columnInfo.cx = 150;
    ListView_InsertColumn(hListView, 0, &columnInfo);

    columnInfo.pszText = "Phone";
    columnInfo.cx = 100;
    ListView_InsertColumn(hListView, 1, &columnInfo);

    columnInfo.pszText = "Email";
    columnInfo.cx = 200;
    ListView_InsertColumn(hListView, 2, &columnInfo);

    columnInfo.pszText = "Date";
    columnInfo.cx = 100;
    ListView_InsertColumn(hListView, 3, &columnInfo);
}

void DisplayContacts(HWND hListView, const char *filter) {
    ListView_DeleteAllItems(hListView);

    LVITEM itemInfo;
    ZeroMemory(&itemInfo, sizeof(itemInfo));
    itemInfo.mask = LVIF_TEXT;

    for (int i = 0; i < g_contactCount; i++) {
        if (filter && filter[0] != '\0') {
            if (strstr(g_contacts[i].name, filter) == NULL) {
                continue;
            }
        }

        itemInfo.iItem = ListView_GetItemCount(hListView);
        itemInfo.iSubItem = 0;
        itemInfo.pszText = g_contacts[i].name;
        int insertedIndex = ListView_InsertItem(hListView, &itemInfo);

        ListView_SetItemText(hListView, insertedIndex, 1, g_contacts[i].phone);
        ListView_SetItemText(hListView, insertedIndex, 2, g_contacts[i].email);
        ListView_SetItemText(hListView, insertedIndex, 3, g_contacts[i].date);
    }
}

void ShowAddContactDialog(HWND hwnd) {
    g_editIndex = -1;
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADD_DIALOG), hwnd, ContactDlgProc);
}

void ShowEditContactDialog(HWND hwnd, int index) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADD_DIALOG), hwnd, ContactDlgProc);
}

INT_PTR CALLBACK ContactDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static char nameBuffer[100], phoneBuffer[30], emailBuffer[100], dateBuffer[11];

    switch(message) {
        case WM_INITDIALOG:
            if (g_editIndex != -1) {
                SetDlgItemText(hDlg, 1001, g_contacts[g_editIndex].name);
                SetDlgItemText(hDlg, 1002, g_contacts[g_editIndex].phone);
                SetDlgItemText(hDlg, 1003, g_contacts[g_editIndex].email);
                SetDlgItemText(hDlg, 1004, g_contacts[g_editIndex].date);
            } else {
                SetDlgItemText(hDlg, 1001, "");
                SetDlgItemText(hDlg, 1002, "");
                SetDlgItemText(hDlg, 1003, "");
                SetDlgItemText(hDlg, 1004, "");
            }
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetDlgItemText(hDlg, 1001, nameBuffer, 100);
                GetDlgItemText(hDlg, 1002, phoneBuffer, 30);
                GetDlgItemText(hDlg, 1003, emailBuffer, 100);
                GetDlgItemText(hDlg, 1004, dateBuffer, 11);

                // Trim trailing whitespace
                for (int i=(int)strlen(nameBuffer)-1; i>=0 && isspace((unsigned char)nameBuffer[i]); i--) nameBuffer[i]=0;
                for (int i=(int)strlen(phoneBuffer)-1; i>=0 && isspace((unsigned char)phoneBuffer[i]); i--) phoneBuffer[i]=0;
                for (int i=(int)strlen(emailBuffer)-1; i>=0 && isspace((unsigned char)emailBuffer[i]); i--) emailBuffer[i]=0;
                for (int i=(int)strlen(dateBuffer)-1; i>=0 && isspace((unsigned char)dateBuffer[i]); i--) dateBuffer[i]=0;

                if (!ValidateName(nameBuffer)) {
                    MessageBox(hDlg, "Invalid name! Must not be empty.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }
                if (!ValidatePhone(phoneBuffer)) {
                    MessageBox(hDlg, "Invalid phone! Digits, '+' and '-' only.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }
                if (!ValidateEmail(emailBuffer)) {
                    MessageBox(hDlg, "Invalid email! Must contain '@' if not empty.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }

                if (g_editIndex == -1) {
                    AddNewContact(nameBuffer, phoneBuffer, emailBuffer, dateBuffer);
                } else {
                    UpdateExistingContact(g_editIndex, nameBuffer, phoneBuffer, emailBuffer, dateBuffer);
                }

                DisplayContacts(g_hListView, NULL);
                EndDialog(hDlg, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
            }
            return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

void AddNewContact(const char *name, const char *phone, const char *email, const char *date) {
    if (g_contactCount >= MAX_CONTACTS) {
        ShowError("Contact list is full!");
        return;
    }

    Contact newContact;
    strncpy(newContact.name,  name,  100); newContact.name[99] = '\0';
    strncpy(newContact.phone, phone, 30);  newContact.phone[29] = '\0';
    strncpy(newContact.email, email, 100); newContact.email[99] = '\0';
    strncpy(newContact.date,  date,  11);  newContact.date[10] = '\0';

    g_contacts[g_contactCount++] = newContact;
}

void UpdateExistingContact(int index, const char *name, const char *phone, const char *email, const char *date) {
    if (index < 0 || index >= g_contactCount) return;

    strncpy(g_contacts[index].name,  name,  100); g_contacts[index].name[99] = '\0';
    strncpy(g_contacts[index].phone, phone, 30);  g_contacts[index].phone[29] = '\0';
    strncpy(g_contacts[index].email, email, 100); g_contacts[index].email[99] = '\0';
    strncpy(g_contacts[index].date,  date,  11);  g_contacts[index].date[10] = '\0';
}

void DeleteSelectedContact(HWND hListView) {
    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (selected == -1) {
        ShowInfo("No contact selected!");
        return;
    }

    int response = MessageBox(g_hMainWnd, "Are you sure you want to delete this contact?", "Confirm", MB_YESNO|MB_ICONQUESTION);
    if (response == IDYES) {
        for (int i = selected; i < g_contactCount - 1; i++) {
            g_contacts[i] = g_contacts[i+1];
        }
        g_contactCount--;
        DisplayContacts(hListView, NULL);
    }
}

void SaveContactsRSA(const char *filename) {
    // Serialize all contacts into a pipe-delimited text buffer
    char buffer[100000];
    buffer[0] = '\0';

    for (int i = 0; i < g_contactCount; i++) {
        char line[512];
        // Format: Name|Phone|Email|Date\n
        snprintf(line, sizeof(line), "%s|%s|%s|%s\n",
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

    // Encrypt each character and write as int
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
    ShowInfo("Contacts saved (RSA encrypted) to contacts.txt!");
}

void LoadContactsRSA(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        ShowInfo("No file found to load.");
        return;
    }

    // Read and decrypt
    int enc;
    char buffer[100000];
    int pos = 0;
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

    Contact tempContacts[MAX_CONTACTS];
    int tempCount = 0;

    char *lineContext = NULL;
    char *contactLine = strtok_r(buffer, "\n", &lineContext);

    while (contactLine) {
        char *tokenContext = NULL;
        // Fields: Name|Phone|Email|Date
        char *tokenName  = strtok_r(contactLine, "|", &tokenContext);
        char *tokenPhone = strtok_r(NULL, "|", &tokenContext);
        char *tokenEmail = strtok_r(NULL, "|", &tokenContext);
        char *tokenDate  = strtok_r(NULL, "|", &tokenContext);

        // Name must exist and not be empty
        if (tokenName && tokenName[0] != '\0') {
            Contact c;
            strncpy(c.name, tokenName, 100); c.name[99] = '\0';

            if (tokenPhone && tokenPhone[0] != '\0') {
                strncpy(c.phone, tokenPhone, 30);
            } else {
                c.phone[0] = '\0';
            }
            c.phone[29] = '\0';

            if (tokenEmail && tokenEmail[0] != '\0') {
                strncpy(c.email, tokenEmail, 100);
            } else {
                c.email[0] = '\0';
            }
            c.email[99] = '\0';

            if (tokenDate && tokenDate[0] != '\0') {
                strncpy(c.date, tokenDate, 11);
            } else {
                c.date[0] = '\0';
            }
            c.date[10] = '\0';

            if (tempCount < MAX_CONTACTS) {
                tempContacts[tempCount++] = c;
            } else {
                ShowError("Too many contacts loaded!");
                break;
            }
        }
        // If no name, skip this line

        contactLine = strtok_r(NULL, "\n", &lineContext);
    }

    if (tempCount > 0) {
        g_contactCount = tempCount;
        for (int i = 0; i < tempCount; i++) {
            g_contacts[i] = tempContacts[i];
        }
        ShowInfo("Contacts loaded and decrypted from contacts.txt!");
    } else {
        ShowInfo("No valid contacts found in the file. Existing contacts remain unchanged.");
    }
}

int ValidateName(const char *name) {
    return (strlen(name) > 0);
}

int ValidatePhone(const char *phone) {
    for (int i = 0; i < (int)strlen(phone); i++) {
        if (!isdigit((unsigned char)phone[i]) && phone[i] != '+' && phone[i] != '-') {
            return 0;
        }
    }
    return 1;
}

int ValidateEmail(const char *email) {
    // Allow empty email
    if (email[0] == '\0') return 1;
    return (strchr(email, '@') != NULL);
}

int CompareContactsByName(const void *a, const void *b) {
    const Contact *c1 = (const Contact*)a;
    const Contact *c2 = (const Contact*)b;
#ifdef _MSC_VER
    return _stricmp(c1->name, c2->name);
#else
    return strcasecmp(c1->name, c2->name);
#endif
}

void SortContactsByName() {
    qsort(g_contacts, g_contactCount, sizeof(Contact), CompareContactsByName);
}

void ShowError(const char *msg) {
    MessageBox(g_hMainWnd, msg, "Error", MB_OK | MB_ICONERROR);
}

void ShowInfo(const char *msg) {
    MessageBox(g_hMainWnd, msg, "Info", MB_OK | MB_ICONINFORMATION);
}

void PerformSearch() {
    char query[256];
    GetWindowText(GetDlgItem(g_hMainWnd, IDC_SEARCH_EDIT), query, sizeof(query));
    DisplayContacts(g_hListView, query);
}

void ClearSearchFilter() {
    SetWindowText(GetDlgItem(g_hMainWnd, IDC_SEARCH_EDIT), "");
    DisplayContacts(g_hListView, NULL);
}

// ------------------------------------------
//              RSA Functions
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

