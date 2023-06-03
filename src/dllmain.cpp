﻿#include "pch.h"
#include "pystring/pystring.h"
#include "EasyCEFHooks.h"
#include <Windows.h>
#include "App.h"
#include "resource.h"
#include "utils/utils.h"
#include <stdlib.h>
#include <PluginManager.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "Wininet.lib")

std::string script;

void message(const std::string& title, const std::string& text) {
	MessageBox(nullptr, util::s2ws(text).c_str(), util::s2ws(title).c_str(), 0);
}

HMODULE g_hModule = nullptr;
App* app;

extern BNString datapath;

NCMProcessType process_type = Undetected;
LONG WINAPI BNUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo);

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved) {
	if (dwReason == DLL_PROCESS_ATTACH) {
		g_hModule = hModule;
		SetUnhandledExceptionFilter(BNUnhandledExceptionFilter);

		try {
			if (!getenv("BETTERNCM_DISABLED_FLAG")) {
				if (util::get_command_line().includes(L"--type=renderer"))process_type = Renderer;
				else if (util::get_command_line().includes(L"--type=gpu-process"))process_type = GpuProcess;
				else if (util::get_command_line().includes(L"--type=utility"))process_type = Utility;
				else process_type = Main;
				namespace fs = std::filesystem;

				// Pick data folder
				if (getenv("BETTERNCM_PROFILE")) {
					datapath = util::getEnvironment("BETTERNCM_PROFILE");
				}
				else {
					datapath = "C:\\betterncm"; // 不再向前兼容
				}

				if (process_type == Main) {
					AllocConsole();
					freopen("CONOUT$", "w", stdout);
					ShowWindow(GetConsoleWindow(), SW_HIDE);

					std::wcout << L"Data folder picked: " << datapath << "\n";

					if (static_cast<int>(fs::status((std::wstring)datapath).permissions()) & static_cast<int>(
						std::filesystem::perms::owner_write)) {
						// Create data folder
						fs::create_directories(datapath + L"/plugins");
						
						// PluginMarket
						if (!fs::exists(datapath + L"/plugins/PluginMarket.plugin")) {
							HRSRC myResource = ::FindResource(hModule, MAKEINTRESOURCE(IDR_RCDATA1), RT_RCDATA);
							unsigned int myResourceSize = SizeofResource(hModule, myResource);
							HGLOBAL myResourceData = LoadResource(hModule, myResource);
							void* pMyBinaryData = LockResource(myResourceData);
							std::ofstream f(datapath + L"/plugins/PluginMarket.plugin", std::ios::out | std::ios::binary);
							f.write(static_cast<char*>(pMyBinaryData), myResourceSize);
							f.close();
						}

						// Inject NCM
						app = new App();
					}
					else {
						util::alert(L"BetterNCM访问数据目录失败！可能需要以管理员身份运行或更改数据目录。\n\nBetterNCM将不会运行");
					}
				}
				else if (process_type == Renderer) {
					EasyCEFHooks::InstallHooks();

					PluginManager::loadAll();
					SetUnhandledExceptionFilter(BNUnhandledExceptionFilter);
					for (auto& plugin : PluginManager::getAllPlugins()) {
						plugin->loadNativePluginDll(process_type);
					}
				}
				else {
					PluginManager::loadAll();
					SetUnhandledExceptionFilter(BNUnhandledExceptionFilter);
					for (auto& plugin : PluginManager::getAllPlugins()) {
						plugin->loadNativePluginDll(process_type);
					}
				}
			}

		}
		catch (std::exception& e) {
			util::alert("BetterNCM 崩溃了！\n\nBetterNCM 将不会运行\n网易云将有可能崩溃\n\n崩溃原因：" + std::string(e.what()));
		}


		DisableThreadLibraryCalls(hModule);

		return TRUE;
	}
	if (dwReason == DLL_PROCESS_DETACH) {
		if (app)
			delete app;

		if (!getenv("BETTERNCM_DISABLED_FLAG")) {
			EasyCEFHooks::UninstallHook();
		}
	}
	return TRUE;
}
