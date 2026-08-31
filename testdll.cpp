#include <windows.h>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: testdll.exe <dll>\n";
        return 1;
    }

    HMODULE h = LoadLibraryA(argv[1]);

    if (!h)
    {
        DWORD error = GetLastError();

        std::cout << "FAILED: " << argv[1] << "\n";
        std::cout << "Windows error: " << error << "\n";

        return error;
    }

    std::cout << "SUCCESS: " << argv[1] << " loaded.\n";

    FreeLibrary(h);
    return 0;
}