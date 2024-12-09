Contact Management System - ReadMe
==================================

1. Overview
-----------
This Contact Management System is a C-based application for managing a list of contacts. It allows adding, editing, deleting, searching, sorting, and persisting contact information. Data can be saved to and loaded from a text file (contacts.txt), with RSA encryption to protect sensitive information.

2. Requirements
---------------
- Windows Operating System.
- A C compiler with Windows API support (MinGW).
- ComCtl32 library for GUI controls.

3. How to Compile
-----------------
Use the provided build command to compile:



For MinGW: 

gcc -o ContactManager.exe ContactManager.c Resource.o -lcomctl32 -lgdi32 -luser32 -lole32 -lshell32 -o3

Ensure that the Resource Script (containing the dialog resource) is included. Put  .rc file in the same folder, compile it as well and link it.

In the terminal copy and paste:

windres Resource.rc -O coff -o Resource.o


4. How to Run
-------------
After successful compilation, run:
    ContactManager.exe

A window will open with a menu and a search bar. 

5. Usage Instructions
---------------------
- "File" menu > "Add Contact": Add a new contact.  
- "File" menu > "Edit Contact": Select a contact in the list and edit its details.  
- "File" menu > "Delete Contact": Select a contact and delete it.  
- "File" menu > "Sort by Name": Sorts all contacts alphabetically by name.  
- "File" menu > "Sort by Phone": Sorts all contacts by phone number.  
- "File" menu > "Save (RSA Encrypted)": Saves all contacts to contacts.txt with RSA encryption.  
- "File" menu > "Load (RSA Decrypted)": Loads contacts from contacts.txt, decrypting them.  
- Search box: Type a name substring and click "Go" to filter contacts by name. Click "Clear" to reset.

6. Input Validation
-------------------
- Names must not be empty.
- Phones must contain digits and optional '+' or '-', and if starting with '+', must be +44 or +60.
- Emails must contain '@' if not empty.

7. Encryption
-------------
Contacts are encrypted with a small RSA example (not secure in production). On save, each character is encrypted and written as an integer. On load, it is decrypted.

8. Additional Resources
-----------------------
- RSA concept reference:
    - Cracking Codes with Python - Chapter 22-24
    -  https://simple.wikipedia.org/wiki/RSA_(algorithm)
- Win32 GUI programming: https://docs.microsoft.com/en-us/windows/win32/apiindex/windows-api-list

9. Support
----------
For any issues, please contact the project group members at their provided emails.




