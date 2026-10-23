#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

int AppMain(int argc, char** argv, bool console);
std::string WideToUtf8(const std::wstring& w);

int wmain(int, wchar_t**) {
  SetConsoleOutputCP(CP_UTF8);
  int argc = 0;
  LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::string> args;
  for (int i = 0; i < argc; i++) args.push_back(WideToUtf8(wargv[i]));
  LocalFree(wargv);
  std::vector<char*> argv;
  for (auto& a : args) argv.push_back(a.data());
  argv.push_back(nullptr);
  return AppMain(argc, argv.data(), true);
}
