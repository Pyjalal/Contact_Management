# Contact_Management
How to Compile:

In the terminal paste:
windres Resource.rc -O coff -o Resource.o

followed by

gcc -o ContactManager.exe ContactManager.c Resource.o -lcomctl32 -lgdi32 -luser32 -lole32 -lshell32

