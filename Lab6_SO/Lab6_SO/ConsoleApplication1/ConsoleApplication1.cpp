#include <windows.h>
#include <process.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

#define NUM_PROCESSES 10
#define NUMBERS_PER_PROCESS 1000
#define MAX_NUMBER 10000

HANDLE hMutex;

void SafePrint(const std::string& message, HANDLE mutex) {
    WaitForSingleObject(mutex, INFINITE);
    std::cout << message << std::endl;
    ReleaseMutex(mutex);
}

void FindPrimesInRange(HANDLE readPipe, HANDLE writePipe) {
    int start, end;
    DWORD bytesRead, bytesWritten;
    ReadFile(readPipe, &start, sizeof(int), &bytesRead, nullptr);
    ReadFile(readPipe, &end, sizeof(int), &bytesRead, nullptr);

    std::ostringstream primeStream;
    for (int num = start; num <= end; ++num) {
        bool isPrime = (num > 1);
        for (int i = 2; i * i <= num && isPrime; ++i) {
            if (num % i == 0) {
                isPrime = false;
            }
        }
        if (isPrime) {
            primeStream << num << " ";
        }
    }

    std::string primes = primeStream.str();
    WriteFile(writePipe, primes.c_str(), primes.size() + 1, &bytesWritten, nullptr);
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    _endthread();
}

int main() {
    hMutex = CreateMutex(nullptr, FALSE, nullptr);

    // Pipe-uri pentru comunicarea între procese
    std::vector<HANDLE> parentToChildWrite(NUM_PROCESSES);
    std::vector<HANDLE> childToParentRead(NUM_PROCESSES);
    std::vector<HANDLE> parentToChildRead(NUM_PROCESSES);
    std::vector<HANDLE> childToParentWrite(NUM_PROCESSES);
    std::vector<HANDLE> threads(NUM_PROCESSES);

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        HANDLE readPipe, writePipe;

        // Pipe parent-to-child
        CreatePipe(&readPipe, &writePipe, nullptr, 0);
        parentToChildWrite[i] = writePipe;
        parentToChildRead[i] = readPipe;

        // Pipe child-to-parent
        CreatePipe(&readPipe, &writePipe, nullptr, 0);
        childToParentWrite[i] = writePipe;
        childToParentRead[i] = readPipe;
    }

    // Lansare procese copil
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        threads[i] = (HANDLE)_beginthread(
            [](void* params) {
                HANDLE* pipes = (HANDLE*)params;
                FindPrimesInRange(pipes[0], pipes[1]);
            },
            0,
            new HANDLE[2]{ parentToChildRead[i], childToParentWrite[i] });
    }

    // Împărțirea intervalelor și comunicarea cu procesele copil
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        int rangeStart = i * NUMBERS_PER_PROCESS + 1;
        int rangeEnd = (i == NUM_PROCESSES - 1) ? MAX_NUMBER : rangeStart + NUMBERS_PER_PROCESS - 1;

        DWORD bytesWritten;
        WriteFile(parentToChildWrite[i], &rangeStart, sizeof(int), &bytesWritten, nullptr);
        WriteFile(parentToChildWrite[i], &rangeEnd, sizeof(int), &bytesWritten, nullptr);
        CloseHandle(parentToChildWrite[i]); // Închidem capătul de scriere al pipe-ului părinte către copil
    }

    // Procesul principal colectează și afișează rezultatele
    SafePrint("Procesul principal începe afișarea rezultatelor...", hMutex);
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        WaitForSingleObject(threads[i], INFINITE);

        char buffer[1024] = { 0 };
        DWORD bytesRead;
        if (ReadFile(childToParentRead[i], buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            SafePrint("Rezultate de la procesul " + std::to_string(i + 1) + ": " + buffer, hMutex);
        }
        CloseHandle(childToParentRead[i]); // Închidem capătul de citire al pipe-ului copil către părinte
    }

    // Curățarea resurselor
    CloseHandle(hMutex);
    SafePrint("Procesul principal a terminat afișarea rezultatelor.", hMutex);

    return 0;
}
