/* Tiny portable launcher: C, no C++ runtime, no extra DLLs.
 * Starts runtime\engine.exe. GCC/Qt DLLs live next to the engine. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

static void skipArg0(const wchar_t *cl, wchar_t *extra, DWORD extraChars)
{
    DWORD i = 0;
    extra[0] = 0;
    while (cl[i] == L' ' || cl[i] == L'\t')
        ++i;
    if (cl[i] == L'"') {
        ++i;
        while (cl[i] && cl[i] != L'"')
            ++i;
        if (cl[i] == L'"')
            ++i;
    } else {
        while (cl[i] && cl[i] != L' ' && cl[i] != L'\t')
            ++i;
    }
    while (cl[i] == L' ' || cl[i] == L'\t')
        ++i;
    if (!cl[i])
        return;
    lstrcpynW(extra, cl + i, (int)extraChars);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show)
{
    wchar_t exe[MAX_PATH];
    wchar_t root[MAX_PATH];
    wchar_t runtime[MAX_PATH];
    wchar_t child[MAX_PATH];
    wchar_t qtPlug[MAX_PATH];
    wchar_t qtPlat[MAX_PATH];
    wchar_t extra[32768];
    wchar_t cmd[32768];
    wchar_t pathEnv[32768];
    wchar_t oldPath[32768];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD n;
    DWORD code = 1;
    wchar_t *slash;

    (void)inst;
    (void)prev;
    (void)cmdLine;
    (void)show;

    if (!GetModuleFileNameW(NULL, exe, MAX_PATH))
        return 1;
    lstrcpynW(root, exe, MAX_PATH);
    slash = wcsrchr(root, L'\\');
    if (!slash)
        slash = wcsrchr(root, L'/');
    if (slash)
        *slash = 0;
    else
        lstrcpyW(root, L".");

    lstrcpyW(runtime, root);
    lstrcatW(runtime, L"\\runtime");
    lstrcpyW(child, runtime);
    lstrcatW(child, L"\\engine.exe");

    if (GetFileAttributesW(child) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL,
                    L"runtime\\engine.exe not found.\n"
                    L"Не найден runtime\\engine.exe.\n\n"
                    L"Unpack the whole archive. Run Arbuz.exe from the folder root —\n"
                    L"not from runtime and not as a lone file.\n\n"
                    L"Распакуйте весь архив целиком. Запускайте Arbuz.exe из корня папки —\n"
                    L"не из runtime и не отдельным файлом.",
                    L"Arbuz",
                    MB_ICONERROR | MB_OK);
        return 1;
    }

    SetEnvironmentVariableW(L"ARBUZ_PORTABLE_ROOT", root);
    n = GetEnvironmentVariableW(L"PATH", oldPath, 32768);
    lstrcpyW(pathEnv, runtime);
    if (n > 0 && n < 32768) {
        lstrcatW(pathEnv, L";");
        lstrcatW(pathEnv, oldPath);
    }
    SetEnvironmentVariableW(L"PATH", pathEnv);
    lstrcpyW(qtPlug, runtime);
    lstrcatW(qtPlug, L"\\qt-plugins");
    SetEnvironmentVariableW(L"QT_PLUGIN_PATH", qtPlug);
    lstrcpyW(qtPlat, qtPlug);
    lstrcatW(qtPlat, L"\\platforms");
    SetEnvironmentVariableW(L"QT_QPA_PLATFORM_PLUGIN_PATH", qtPlat);

    extra[0] = 0;
    skipArg0(GetCommandLineW(), extra, 32768);
    lstrcpyW(cmd, L"\"");
    lstrcatW(cmd, child);
    lstrcatW(cmd, L"\"");
    if (extra[0]) {
        lstrcatW(cmd, L" ");
        lstrcatW(cmd, extra);
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(child, cmd, NULL, NULL, FALSE, 0, NULL, root, &si, &pi)) {
        MessageBoxW(NULL,
                    L"Could not start runtime\\engine.exe.\n"
                    L"Не удалось запустить runtime\\engine.exe.",
                    L"Arbuz", MB_ICONERROR | MB_OK);
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}
#else
int main(void)
{
    return 0;
}
#endif
