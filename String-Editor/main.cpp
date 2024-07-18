#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <random>
#include <thread>
#include <limits>

namespace fs = std::filesystem;

void RandomizeConsoleTitle(const std::string& process) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 51);

    std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string title;
    for (int i = 0; i < 20; ++i) {
        title += alphabet[dis(gen)];
    }

    std::string fullTitle = "v92d - " + process + " - " + title;
    SetConsoleTitleA(fullTitle.c_str());
}

std::string GetProcessName(const std::wstring& filePath) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(fs::path(filePath).filename().wstring());
}

void FindAndReplaceInFile(const std::wstring& filePath, const std::vector<std::pair<std::string, std::string>>& replacements) {
    std::ifstream fileIn(filePath, std::ios::binary);
    if (!fileIn.is_open()) {
        std::cerr << "Failed to open file: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(filePath) << std::endl;
        return;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(fileIn)), std::istreambuf_iterator<char>());
    fileIn.close();

    for (const auto& [searchString, replaceString] : replacements) {
        auto it = std::search(buffer.begin(), buffer.end(), searchString.begin(), searchString.end());
        while (it != buffer.end()) {
            std::copy(replaceString.begin(), replaceString.end(), it);
            it = std::search(it + replaceString.size(), buffer.end(), searchString.begin(), searchString.end());
        }
    }

    std::ofstream fileOut(filePath, std::ios::binary | std::ios::trunc);
    if (!fileOut.is_open()) {
        std::cerr << "Failed to open file for writing: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(filePath) << std::endl;
        return;
    }

    fileOut.write(buffer.data(), buffer.size());
    fileOut.close();
}

bool IsValidFileName(const std::wstring& fileName) {
    return true;
}

std::wstring GetNewFileName() {
    std::wstring newFileName;
    char choice;

    std::cout << "Continue editing or save the file? (c / s): ";
    std::cin >> choice;
    std::string dummy;
    std::getline(std::cin, dummy);

    if (choice == 's') {
        std::wcout << L"Enter new file name to save changes: ";
        std::getline(std::wcin >> std::ws, newFileName);
    }

    return newFileName;
}

void SaveChangesToFile(const std::wstring& originalFilePath, const std::wstring& newFileName, const std::vector<std::pair<std::string, std::string>>& replacements) {
    if (!IsValidFileName(newFileName)) {
        std::wcerr << L"Invalid file name: " << newFileName << std::endl;
        return;
    }

    std::wstring directory = fs::current_path().wstring();
    std::wstring newFilePath = directory + L"\\" + newFileName;

    std::wcout << L"Saving changes to file: " << newFilePath << std::endl;

    fs::copy_file(originalFilePath, newFilePath, fs::copy_options::overwrite_existing);

    FindAndReplaceInFile(newFilePath, replacements);

    std::wcout << L"File saved successfully." << std::endl;
}

void SetConsoleTransparency(int transparency) {
    HWND consoleWindow = GetConsoleWindow();
    LONG style = GetWindowLong(consoleWindow, GWL_EXSTYLE);
    SetWindowLong(consoleWindow, GWL_EXSTYLE, style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(consoleWindow, 0, (255 * transparency) / 100, LWA_ALPHA);
}

void SetConsoleSize(int width, int height) {
    HWND consoleWindow = GetConsoleWindow();
    RECT rect;
    GetWindowRect(consoleWindow, &rect);
    MoveWindow(consoleWindow, rect.left, rect.top, width, height, TRUE);
}

int main() {
    SetConsoleSize(500, 400);
    SetConsoleTransparency(80);
    LPWSTR* argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc < 2) {
        std::cerr << "No file dropped!" << std::endl;
        Sleep(5000);
        return 1;
    }

    std::wstring filePath = argv[1];
    LocalFree(argv);

    std::string processName = GetProcessName(filePath);

    std::thread titleThread([processName]() {
        while (true) {
            RandomizeConsoleTitle(processName);
            Sleep(100);
        }
        });
    titleThread.detach();

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(filePath.c_str(), nullptr, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        std::cerr << "Failed to create process: " << GetLastError() << std::endl;
        return 1;
    }

    std::vector<std::pair<std::string, std::string>> replacements;
    std::string searchString;
    std::string replaceString;
    bool continueEditing = true;

    while (continueEditing) {
        std::cout << "Enter string to search: ";
        std::getline(std::cin, searchString);

        std::cout << "Enter replacement string: ";
        std::getline(std::cin, replaceString);

        replacements.push_back({ searchString, replaceString });

        std::wstring newFileName = GetNewFileName();
        if (!newFileName.empty()) {
            SaveChangesToFile(filePath, newFileName, replacements);
            continueEditing = false;
        }
        else {
            std::cout << "Continuing without saving changes." << std::endl;
        }
    }

    TerminateProcess(pi.hProcess, 0);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
