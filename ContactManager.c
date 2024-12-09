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
    char date[11]; // Expected format: "YYYY-MM-DD" or empty
} Contact;

// ------------------------------------------
//              Global Definitions
// ------------------------------------------
#define MAX_CONTACTS 1000

// RSA keys (small and insecure, for demonstration only)
static const int RSA_n = 3233;   // Example modulus (61*53)
static const int RSA_e = 17;     // Public exponent
static const int RSA_d = 2753;   // Private exponent

// Menu and Control IDs
enum {
    IDM_ADD = 1,
    IDM_EDIT,
    IDM_DELETE,
    IDM_SAVE,
    IDM_LOAD,
    IDM_SORT_NAME,
    IDM_SORT_PHONE,
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

// Global array of contacts
static Contact g_contacts[MAX_CONTACTS];
static int g_contactCount = 0;     // Current count of contacts

static HWND g_hMainWnd = NULL;     // Handle to the main window
static HWND g_hListView = NULL;    // Handle to the ListView control

// If g_editIndex is -1, we are adding a new contact.
// Otherwise, we are editing the contact at g_editIndex.
static int g_editIndex = -1;

// Forward declarations of functions
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
void SortContactsByPhone();
int  CompareContactsByName(const void *a, const void *b);
int  CompareContactsByPhone(const void *a, const void *b);

void ShowError(const char *msg);
void ShowInfo(const char *msg);
void PerformSearch();
void ClearSearchFilter();

// RSA and modular exponentiation
int RSA_EncryptChar(int m);
int RSA_DecryptChar(int c);
int modExp(int base, int exp, int mod);

// ------------------------------------------
//                 WinMain
// ------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls for ListView support
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    // Set up and register the main window class
    WNDCLASS wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;                // Our window procedure
    wc.hInstance = hInstance;
    wc.lpszClassName = "ContactManagerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    // Create the main application window
    g_hMainWnd = CreateWindow("ContactManagerClass", "Contact Management System",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              700, 500, NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    // Show and update the main window
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    // Main message loop for the application
    MSG msg;
    while (GetMessage(&msg, NULL, 0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// ------------------------------------------
//               Window Procedure
// ------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HMENU hMenu;
    switch(msg) {
        case WM_CREATE: {
            // Create and set up the menu
            hMenu = CreateMenu();
            if (!hMenu) {
                ShowError("Failed to create menu.");
                PostQuitMessage(1);
                break;
            }

            // File menu with various contact operations
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, IDM_ADD,         "Add Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_EDIT,        "Edit Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_DELETE,      "Delete Contact");
            AppendMenu(hFileMenu, MF_STRING, IDM_SORT_NAME,   "Sort by Name");
            AppendMenu(hFileMenu, MF_STRING, IDM_SORT_PHONE,  "Sort by Phone");
            AppendMenu(hFileMenu, MF_STRING, IDM_SAVE,        "Save (RSA Encrypted)");
            AppendMenu(hFileMenu, MF_STRING, IDM_LOAD,        "Load (RSA Decrypted)");
            AppendMenu(hFileMenu, MF_STRING, IDM_EXIT,        "Exit");
            AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFileMenu, "Menu");
            SetMenu(hwnd, hMenu);

            // Create UI elements for searching contacts by name
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

            // Create the ListView control to display contacts
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

            // Initialize the columns in the ListView
            InitializeListViewColumns(g_hListView);
            break;
        }

        case WM_SIZE: {
            // Adjust the ListView size when the main window is resized
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(g_hListView, 10, 40, width - 20, height - 50, TRUE);
            break;
        }

        case WM_COMMAND: {
            // Handle menu and button commands
            switch(LOWORD(wParam)) {
                case IDM_ADD:
                    // Show dialog to add a new contact
                    g_editIndex = -1;
                    ShowAddContactDialog(hwnd);
                    break;
                case IDM_EDIT: {
                    // Show dialog to edit the currently selected contact
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
                    // Delete the currently selected contact
                    if (g_contactCount == 0) {
                        ShowInfo("No contacts to delete.");
                    } else {
                        DeleteSelectedContact(g_hListView);
                    }
                    break;
                case IDM_SAVE:
                    // Save all contacts to file (RSA encrypted)
                    if (g_contactCount == 0) {
                        ShowInfo("No contacts to save.");
                    } else {
                        SaveContactsRSA("contacts.txt");
                    }
                    break;
                case IDM_LOAD:
                    // Load contacts from file (RSA decrypted)
                    LoadContactsRSA("contacts.txt");
                    DisplayContacts(g_hListView, NULL);
                    break;
                case IDM_SORT_NAME:
                    // Sort contacts by name
                    if (g_contactCount > 1) {
                        SortContactsByName();
                        DisplayContacts(g_hListView, NULL);
                    } else {
                        ShowInfo("Not enough contacts to sort.");
                    }
                    break;
                case IDM_SORT_PHONE:
                    // Sort contacts by phone
                    if (g_contactCount > 1) {
                        SortContactsByPhone();
                        DisplayContacts(g_hListView, NULL);
                    } else {
                        ShowInfo("Not enough contacts to sort.");
                    }
                    break;
                case IDM_EXIT:
                    // Exit the application
                    PostQuitMessage(0);
                    break;
                case IDC_SEARCH_BUTTON:
                    // Perform a search by name substring
                    PerformSearch();
                    break;
                case IDC_CLEAR_BUTTON:
                    // Clear the search filter and show all contacts
                    ClearSearchFilter();
                    break;
            }
            break;
        }

        case WM_DESTROY:
            // Window destroyed, quit the application
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ------------------------------------------
//   Initialize the columns of the ListView
// ------------------------------------------
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

// ------------------------------------------
// Display all contacts in the ListView
// If 'filter' is provided and not empty, only display contacts whose names contain the filter string.
// ------------------------------------------
void DisplayContacts(HWND hListView, const char *filter) {
    ListView_DeleteAllItems(hListView);

    LVITEM itemInfo;
    ZeroMemory(&itemInfo, sizeof(itemInfo));
    itemInfo.mask = LVIF_TEXT;

    for (int i = 0; i < g_contactCount; i++) {
        // Apply the filter if provided
        if (filter && filter[0] != '\0') {
            if (strstr(g_contacts[i].name, filter) == NULL) {
                continue;
            }
        }

        // Insert the contact into the ListView
        itemInfo.iItem = ListView_GetItemCount(hListView);
        itemInfo.iSubItem = 0;
        itemInfo.pszText = g_contacts[i].name;
        int insertedIndex = ListView_InsertItem(hListView, &itemInfo);

        ListView_SetItemText(hListView, insertedIndex, 1, g_contacts[i].phone);
        ListView_SetItemText(hListView, insertedIndex, 2, g_contacts[i].email);
        ListView_SetItemText(hListView, insertedIndex, 3, g_contacts[i].date);
    }
}

// ------------------------------------------
// Show the dialog for adding a new contact
// ------------------------------------------
void ShowAddContactDialog(HWND hwnd) {
    g_editIndex = -1;
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADD_DIALOG), hwnd, ContactDlgProc);
}

// ------------------------------------------
// Show the dialog for editing an existing contact
// ------------------------------------------
void ShowEditContactDialog(HWND hwnd, int index) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADD_DIALOG), hwnd, ContactDlgProc);
}

// ------------------------------------------
// Dialog procedure for the Add/Edit Contact dialog
// Handles input validation and updates global contact array
// ------------------------------------------
INT_PTR CALLBACK ContactDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static char nameBuffer[100], phoneBuffer[30], emailBuffer[100], dateBuffer[11];

    switch(message) {
        case WM_INITDIALOG:
            // If editing, populate the fields with existing data
            if (g_editIndex != -1) {
                SetDlgItemText(hDlg, 1001, g_contacts[g_editIndex].name);
                SetDlgItemText(hDlg, 1002, g_contacts[g_editIndex].phone);
                SetDlgItemText(hDlg, 1003, g_contacts[g_editIndex].email);
                SetDlgItemText(hDlg, 1004, g_contacts[g_editIndex].date);
            } else {
                // If adding, clear the fields
                SetDlgItemText(hDlg, 1001, "");
                SetDlgItemText(hDlg, 1002, "");
                SetDlgItemText(hDlg, 1003, "");
                SetDlgItemText(hDlg, 1004, "");
            }
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                // User clicked OK, retrieve data
                GetDlgItemText(hDlg, 1001, nameBuffer, 100);
                GetDlgItemText(hDlg, 1002, phoneBuffer, 30);
                GetDlgItemText(hDlg, 1003, emailBuffer, 100);
                GetDlgItemText(hDlg, 1004, dateBuffer, 11);

                // Trim trailing whitespace from all fields
                for (int i=(int)strlen(nameBuffer)-1; i>=0 && isspace((unsigned char)nameBuffer[i]); i--) nameBuffer[i]=0;
                for (int i=(int)strlen(phoneBuffer)-1; i>=0 && isspace((unsigned char)phoneBuffer[i]); i--) phoneBuffer[i]=0;
                for (int i=(int)strlen(emailBuffer)-1; i>=0 && isspace((unsigned char)emailBuffer[i]); i--) emailBuffer[i]=0;
                for (int i=(int)strlen(dateBuffer)-1; i>=0 && isspace((unsigned char)dateBuffer[i]); i--) dateBuffer[i]=0;

                // Validate inputs
                if (!ValidateName(nameBuffer)) {
                    MessageBox(hDlg, "Invalid name! Must not be empty.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }
                if (!ValidatePhone(phoneBuffer)) {
                    MessageBox(hDlg, "Invalid phone! Must be digits, '+' and '-' only and follow basic rules.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }
                if (!ValidateEmail(emailBuffer)) {
                    MessageBox(hDlg, "Invalid email! Must contain '@' if not empty.", "Error", MB_OK|MB_ICONERROR);
                    return (INT_PTR)TRUE;
                }

                // If all good, add or update the contact
                if (g_editIndex == -1) {
                    // Add a new contact
                    AddNewContact(nameBuffer, phoneBuffer, emailBuffer, dateBuffer);
                } else {
                    // Update existing contact
                    UpdateExistingContact(g_editIndex, nameBuffer, phoneBuffer, emailBuffer, dateBuffer);
                }

                DisplayContacts(g_hListView, NULL);
                EndDialog(hDlg, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                // User clicked cancel
                EndDialog(hDlg, IDCANCEL);
            }
            return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

// ------------------------------------------
// Add a new contact to the global array
// ------------------------------------------
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

// ------------------------------------------
// Update an existing contact at the specified index
// ------------------------------------------
void UpdateExistingContact(int index, const char *name, const char *phone, const char *email, const char *date) {
    if (index < 0 || index >= g_contactCount) return;

    strncpy(g_contacts[index].name,  name,  100); g_contacts[index].name[99] = '\0';
    strncpy(g_contacts[index].phone, phone, 30);  g_contacts[index].phone[29] = '\0';
    strncpy(g_contacts[index].email, email, 100); g_contacts[index].email[99] = '\0';
    strncpy(g_contacts[index].date,  date,  11);  g_contacts[index].date[10] = '\0';
}

// ------------------------------------------
// Delete the currently selected contact
// ------------------------------------------
void DeleteSelectedContact(HWND hListView) {
    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (selected == -1) {
        ShowInfo("No contact selected!");
        return;
    }

    int response = MessageBox(g_hMainWnd, "Are you sure you want to delete this contact?", "Confirm", MB_YESNO|MB_ICONQUESTION);
    if (response == IDYES) {
        // Shift all contacts after the deleted one forward
        for (int i = selected; i < g_contactCount - 1; i++) {
            g_contacts[i] = g_contacts[i+1];
        }
        g_contactCount--;
        DisplayContacts(hListView, NULL);
    }
}

// ------------------------------------------
// Save all contacts to a file, RSA encrypted
// ------------------------------------------
void SaveContactsRSA(const char *filename) {
    // Serialize all contacts into a pipe-delimited text
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

    // Encrypt each character using RSA and write the integer value to the file
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

// ------------------------------------------
// Load contacts from file, RSA decrypted
// ------------------------------------------
void LoadContactsRSA(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        ShowInfo("No file found to load.");
        return;
    }

    // Read and decrypt the contents of the file
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

    // Parse the decrypted data
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

        // Only add if we have a valid name
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

        contactLine = strtok_r(NULL, "\n", &lineContext);
    }

    // If we loaded any contacts successfully, replace the current list
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

// ------------------------------------------
// Validate the contact's name: must not be empty
// ------------------------------------------
int ValidateName(const char *name) {
    return (strlen(name) > 0);
}

// ------------------------------------------
// Validate the phone number
// Conditions:
// - Allowed chars: digits, '+', '-'
// - If starts with '+', must start with '+44' (UK) or '+60' (Malaysia)
// This is a basic check and can be enhanced as needed.
// ------------------------------------------
int ValidatePhone(const char *phone) {
    for (int i = 0; i < (int)strlen(phone); i++) {
        if (!isdigit((unsigned char)phone[i]) && phone[i] != '+' && phone[i] != '-') {
            return 0;
        }
    }

    // If starts with '+', check the country code
    if (phone[0] == '+') {
        if (strncmp(phone, "+44", 3) != 0 && strncmp(phone, "+60", 3) != 0) {
            // Must be either +44 or +60 if starting with '+'
            return 0;
        }
    }
    return 1;
}

// ------------------------------------------
// Validate the email
// Conditions:
// - Allow empty email
// - If not empty, must contain '@'
// ------------------------------------------
int ValidateEmail(const char *email) {
    if (email[0] == '\0') return 1;
    return (strchr(email, '@') != NULL);
}

// ------------------------------------------
// Comparison function for qsort to sort by name
// ------------------------------------------
int CompareContactsByName(const void *a, const void *b) {
    const Contact *c1 = (const Contact*)a;
    const Contact *c2 = (const Contact*)b;
#ifdef _MSC_VER
    return _stricmp(c1->name, c2->name);
#else
    return strcasecmp(c1->name, c2->name);
#endif
}

// ------------------------------------------
// Comparison function for qsort to sort by phone
// ------------------------------------------
int CompareContactsByPhone(const void *a, const void *b) {
    const Contact *c1 = (const Contact*)a;
    const Contact *c2 = (const Contact*)b;
#ifdef _MSC_VER
    return _stricmp(c1->phone, c2->phone);
#else
    return strcasecmp(c1->phone, c2->phone);
#endif
}

// ------------------------------------------
// Sort the global contacts by name
// ------------------------------------------
void SortContactsByName() {
    qsort(g_contacts, g_contactCount, sizeof(Contact), CompareContactsByName);
}

// ------------------------------------------
// Sort the global contacts by phone
// ------------------------------------------
void SortContactsByPhone() {
    qsort(g_contacts, g_contactCount, sizeof(Contact), CompareContactsByPhone);
}

// ------------------------------------------
// Show an error message box
// ------------------------------------------
void ShowError(const char *msg) {
    MessageBox(g_hMainWnd, msg, "Error", MB_OK | MB_ICONERROR);
}

// ------------------------------------------
// Show an info message box
// ------------------------------------------
void ShowInfo(const char *msg) {
    MessageBox(g_hMainWnd, msg, "Info", MB_OK | MB_ICONINFORMATION);
}

// ------------------------------------------
// Perform a search based on the text entered in the search box
// Displays only those contacts whose name contains the search query
// ------------------------------------------
void PerformSearch() {
    char query[256];
    GetWindowText(GetDlgItem(g_hMainWnd, IDC_SEARCH_EDIT), query, sizeof(query));
    DisplayContacts(g_hListView, query);
}

// ------------------------------------------
// Clear the search filter and show all contacts
// ------------------------------------------
void ClearSearchFilter() {
    SetWindowText(GetDlgItem(g_hMainWnd, IDC_SEARCH_EDIT), "");
    DisplayContacts(g_hListView, NULL);
}

// ------------------------------------------
// RSA Encryption: Encrypt a single character using RSA
// ------------------------------------------
int RSA_EncryptChar(int m) {
    return modExp(m, RSA_e, RSA_n);
}

// ------------------------------------------
// RSA Decryption: Decrypt a single character using RSA
// ------------------------------------------
int RSA_DecryptChar(int c) {
    return modExp(c, RSA_d, RSA_n);
}

// ------------------------------------------
// Modular exponentiation function used for RSA
// (base^exp) % mod efficiently computed
// ------------------------------------------
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

