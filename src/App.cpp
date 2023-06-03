#include "pch.h"
#include "App.h"
#include <Windows.h>
#include "dwmapi.h"
#include "CommDlg.h"
#include <PluginManager.h>
#include "utils/Interprocess.hpp"

#define WIN32_LEAN_AND_MEAN

using namespace util;

namespace fs = std::filesystem;

const std::string version = "1.2.1";

extern BNString datapath;

nlohmann::json config;
std::mutex configMutex;


std::string App::readConfig(const std::string& key, const std::string& def) {
	std::lock_guard<std::mutex> lock(configMutex);

	auto it = config.find(key);
	if (it == config.end()) {
		return def;
	}
	return it.value().get<std::string>();
}

void App::writeConfig(const std::string& key, const std::string& value) {
	std::lock_guard<std::mutex> lock(configMutex);
	config[key] = value;
	std::ofstream file(datapath + L"\\config.json");
	file << config;
}

#define checkApiKey                                                                                  \
	if (!req.has_header("BETTERNCM_API_KEY") || req.get_header_value("BETTERNCM_API_KEY") != apiKey) \
	{                                                                                                \
		res.status = 401;                                                                            \
		return;                                                                                      \
	}

std::thread* App::create_server(const std::string& apiKey) {
	if (this->server_thread) return server_thread;
	this->httpServer = new httplib::Server();
	this->server_port = this->httpServer->bind_to_any_port("127.0.0.1");
	server_thread = new std::thread([=] {
		auto* svr = this->httpServer;
		svr->Get("/api/fs/read_dir", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			if (path[1] != ':') {
				path = datapath + L"/" + path;
			}

			vector<string> paths;

			for (const auto& entry : fs::directory_iterator(static_cast<wstring>(path)))
				paths.push_back(BNString(entry.path().wstring()).utf8());

			res.set_content(static_cast<nlohmann::json>(paths).dump(), "application/json");
		});

		svr->Get("/api/fs/read_file_text", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			if (path[1] != ':') {
				path = datapath + L"/" + path;
			}


			std::ifstream t(path);
			std::stringstream buffer;
			buffer << t.rdbuf();
			res.set_content(buffer.str(), "text/plain");
		});

		svr->Get("/api/fs/read_file", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString file_path = req.get_param_value("path");

			if (file_path[1] != ':') {
				file_path = datapath + L"/" + file_path;
			}

			std::ifstream file(file_path, std::ios::binary);

			res.set_header("Content-Type", "application/octet-stream");

			res.body.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		});

		svr->Get("/api/fs/mount_file", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;


			BNString file_path = req.get_param_value("path");

			if (file_path[1] != ':') {
				file_path = datapath + L"/" + file_path;
			}

			auto ext = fs::path(static_cast<wstring>(file_path)).extension().string();
			auto mountPoint = "/mounted_file/" + random_string(48) + ext;
			auto port = this->server_port;
			svr->Get(mountPoint, [=](const httplib::Request& req, httplib::Response& res) {
				const size_t DATA_CHUNK_SIZE = 65536;
				std::filebuf* pbuf;
				auto filestr = new std::ifstream();
				long size;

				filestr->open(file_path, std::ios::binary);
				pbuf = filestr->rdbuf();
				size = pbuf->pubseekoff(0, std::ios::end, std::ios::in);


				res.set_header("Content-Disposition", "inline;");
				res.set_content_provider(
					size, guessMimeType(ext),
					[pbuf](size_t offset, size_t length, httplib::DataSink& sink) {
						auto data = new char[DATA_CHUNK_SIZE];
						const auto d = data;
						auto out_len = min(static_cast<size_t>(length), DATA_CHUNK_SIZE);

						pbuf->pubseekpos(offset);
						pbuf->sgetn(data, out_len);

						auto ret = sink.write(&d[0], out_len);
						delete[] data;
						return true;
					}, [filestr](bool s) {
						filestr->close();
						delete filestr;
					});
			});

			res.set_content("http://localhost:" + std::to_string(port) + mountPoint, "text/plain");
		});

		svr->Get("/api/fs/mount_dir", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			const size_t SIZE_PER_TIME = 10000;

			BNString file_path = req.get_param_value("path");

			if (file_path[1] != ':') {
				file_path = datapath + L"/" + file_path;
			}

			auto mountPoint = "/mounted_dir/" + random_string(48);
			svr->set_mount_point(mountPoint, file_path);
			res.set_content("http://localhost:" + std::to_string(this->server_port) + mountPoint, "text/plain");
		});


		svr->Get("/api/fs/unzip_file", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			BNString dest = req.get_param_value("dest");

			if (path[1] != ':') {
				path = datapath + L"/" + path;
			}

			if (dest[1] != ':') {
				dest = datapath + L"/" + dest;
			}

			res.set_content(to_string(zip_extract(path.utf8().c_str(), dest.utf8().c_str(), nullptr, nullptr)),
			                "text/plain");
		});

		svr->Get("/api/fs/rename", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			BNString dest = req.get_param_value("dest");

			if (path[1] != ':') {
				path = datapath + L"/" + path;
			}

			if (dest[1] != ':') {
				dest = datapath + L"/" + dest;
			}

			fs::rename(static_cast<wstring>(path), static_cast<wstring>(dest));

			res.set_content("ok", "text/plain");
		});


		svr->Get("/api/fs/mkdir", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			if (path[1] != ':') {
				fs::create_directories(datapath + L"/" + path);
				res.status = 200;
			}
			else {
				fs::create_directories(static_cast<wstring>(path));
				res.status = 200;
			}
		});

		svr->Get("/api/fs/exists", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			if (path[1] != ':') {
				res.set_content(fs::exists(datapath + L"/" + path) ? "true" : "false", "text/plain");
			}
			else {
				res.set_content(fs::exists(static_cast<wstring>(path)) ? "true" : "false", "text/plain");
			}
		});

		svr->Post("/api/fs/write_file_text", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			std::string path = req.get_param_value("path");

			if (path[1] != ':') {
				write_file_text_utf8(datapath.utf8() + "/" + path, req.body);
				res.status = 200;
			}
			else {
				write_file_text_utf8(path, req.body);
				res.status = 200;
			}
		});

		svr->Post("/api/fs/write_file", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			if (path[1] != ':') {
				auto file = req.get_file_value("file");
				ofstream ofs(datapath + L"/" + path, ios::binary);
				ofs << file.content;

				res.status = 200;
			}
			else {
				auto file = req.get_file_value("file");
				ofstream ofs(path, ios::binary);
				ofs << file.content;

				res.status = 200;
			}
		});

		svr->Get("/api/fs/remove", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			using namespace std;
			namespace fs = std::filesystem;

			BNString path = req.get_param_value("path");

			if (path[1] != ':') {
				fs::remove_all(datapath + L"/" + path);
				res.status = 200;
			}
			else {
				fs::remove_all(static_cast<wstring>(path));
				res.status = 200;
			}
		});


		svr->Post("/api/app/exec", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			BNString cmd = req.body;
			exec(cmd, false, req.has_param("_showWindow"));
			res.status = 200;
		});

		svr->Post("/api/app/exec_ele", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			BNString cmd = req.body;
			exec(cmd, true, req.has_param("_showWindow"));
			res.status = 200;
		});

		svr->Get("/api/app/datapath", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			res.set_content(datapath.utf8(), "text/plain");
		});

		svr->Get("/api/app/ncmpath", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			res.set_content(getNCMPath().utf8(), "text/plain");
		});

		svr->Get("/api/app/version", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			res.set_content(version, "text/plain");
		});

		svr->Get("/api/app/read_config", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			res.set_content(readConfig(req.get_param_value("key"), req.get_param_value("default")), "text/plain");
		});

		svr->Get("/api/app/write_config", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			writeConfig(req.get_param_value("key"), req.get_param_value("value"));
			res.status = 200;
		});

		svr->Get("/api/app/reload_plugin", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			PluginManager::extractPackedPlugins();
			res.status = 200;
		});

		svr->Get("/api/app/get_succeeded_hijacks", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			std::shared_lock<std::shared_timed_mutex> guard(succeeded_hijacks_lock);
			res.set_content(static_cast<nlohmann::json>(succeeded_hijacks).dump(), "application/json");
		});

		svr->Get("/api/app/show_console", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			ShowWindow(GetConsoleWindow(), req.has_param("hide") ? SW_HIDE : SW_SHOW);
			res.status = 200;
		});

		svr->Get("/api/app/set_rounded_corner", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
#ifdef _X86
			bool enable = req.get_param_value("enable") == "true";
			HWND ncmWin = FindWindow(L"OrpheusBrowserHost", nullptr);
			DWM_WINDOW_CORNER_PREFERENCE preference = enable ? DWMWCP_ROUND : DWMWCP_DONOTROUND;

			DwmSetWindowAttribute(ncmWin, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

			HWND ncmShadow = nullptr;
			while (ncmShadow = FindWindowEx(nullptr, ncmShadow, L"OrpheusShadow", nullptr))
				SetLayeredWindowAttributes(ncmShadow, 0, 0, LWA_ALPHA);

#endif
			res.status = 200;
		});

		svr->Get("/api/app/bg_screenshot", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			HWND ncmWin = FindWindow(L"OrpheusBrowserHost", nullptr);
			SetWindowDisplayAffinity(ncmWin, WDA_EXCLUDEFROMCAPTURE);
			ScreenCapturePart screenCapturePart;
			res.set_content(screenCapturePart.getData(), screenCapturePart.getDataSize(), "application/octet-stream");
			SetWindowDisplayAffinity(ncmWin, WDA_NONE);
		});

		svr->Get("/api/app/is_light_theme", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;

			// based on https://stackoverflow.com/questions/51334674/how-to-detect-windows-10-light-dark-mode-in-win32-application

			// The value is expected to be a REG_DWORD, which is a signed 32-bit little-endian
			auto buffer = std::vector<char>(4);
			auto cbData = static_cast<DWORD>(buffer.size() * sizeof(char));
			auto result = RegGetValueW(
				HKEY_CURRENT_USER,
				L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
				L"AppsUseLightTheme",
				RRF_RT_REG_DWORD, // expected value type
				nullptr,
				buffer.data(),
				&cbData);

			if (result != ERROR_SUCCESS) {
				throw std::runtime_error("Error: error_code=" + std::to_string(result));
			}

			// convert bytes written to our buffer to an int, assuming little-endian
			auto i = buffer[3] << 24 |
				buffer[2] << 16 |
				buffer[1] << 8 |
				buffer[0];

			res.set_content(i == 1 ? "true" : "false", "plain/text");
		});


		svr->Get("/api/app/open_file_dialog", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			TCHAR szBuffer[MAX_PATH] = {0};
			OPENFILENAME ofn = {0};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			auto filter = s2ws(req.get_param_value("filter"));
			ofn.lpstrFilter = filter.c_str();
			auto initialDir = s2ws(req.get_param_value("initialDir"));
			ofn.lpstrInitialDir = initialDir.c_str();
			ofn.lpstrFile = szBuffer;
			ofn.nMaxFile = sizeof(szBuffer) / sizeof(*szBuffer);
			ofn.nFilterIndex = 0;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
			BOOL bSel = GetOpenFileName(&ofn);

			BNString path = std::wstring(szBuffer);
			res.set_content(path.utf8(), "text/plain");
		});

		// ���ز������������ʽ�ļ�
		svr->Get("/api/internal/framework.css", [&](const httplib::Request& req, httplib::Response& res) {
			checkApiKey;
			res.set_content(load_string_resource(L"framework.css"), "text/css");
		});

		svr->set_mount_point("/local", datapath.utf8());


		svr->listen_after_bind();
	});
	return server_thread;
}


void App::parseConfig() {
	std::lock_guard<std::mutex> lock(configMutex);
	if (fs::exists(datapath + L"\\config.json")) {
		try {
			config = nlohmann::json::parse(read_to_string(datapath + L"\\config.json"));
		}
		catch (std::exception e) {
			std::wcout << L"[BetterNCM] ���������ļ�ʧ�ܣ���ʹ��Ĭ�������ļ�\n\n";
		}
	}
}

App::App() {

	std::cout << "BetterNCM v" << version << " running on NCM " << getNCMExecutableVersion() << std::endl;

	parseConfig();
	
	PluginManager::extractPackedPlugins();
	PluginManager::loadAll();

	if (readConfig("cc.microblock.betterncm.single-process", "false") == "true")
		for (auto& plugin : PluginManager::getAllPlugins()) {
			using pt = NCMProcessType;
			plugin->loadNativePluginDll(static_cast<pt>(Main | GpuProcess | Renderer));
		}
	else {
		for (auto& plugin : PluginManager::getAllPlugins()) {
			plugin->loadNativePluginDll(Main);
		}
	}

	auto apiKey = random_string(64);

	server_thread = create_server(apiKey);


	EasyCEFHooks::onKeyEvent = [](_cef_client_t* client, struct _cef_browser_t* browser,
	                              const struct _cef_key_event_t* event) {
		if (event->type == KEYEVENT_KEYUP &&
#if _DEBUG
			event->windows_key_code == 122 // DEBUG????????????F11
#else
			event->windows_key_code == 123
#endif
		) {
			auto cef_browser_host = browser->get_host(browser);
			auto id = browser->get_identifier(browser);
			auto frame = browser->get_focused_frame(browser);
			if (frame && BNString(cefFromCEFUserFreeTakeOwnership(frame->get_url(frame)).ToWString()).startsWith(
				L"devtools://")) {
				cef_browser_host->close_browser(cef_browser_host, false);
			}
			else if (cef_browser_host->has_dev_tools(cef_browser_host)) {
				HWND hwnd = FindWindow(nullptr, (L"BetterNCM DevTools #" + std::to_wstring(id)).c_str());
				if (hwnd)
					DestroyWindow(hwnd);
			}
			else {
				CefWindowInfo windowInfo{};
				CefBrowserSettings settings{};
				CefPoint point{};
				windowInfo.SetAsPopup(nullptr, "BetterNCM DevTools #" + std::to_string(id));
				cef_browser_host->show_dev_tools(cef_browser_host, &windowInfo, client, &settings, &point);
			}
		}
	};

	EasyCEFHooks::onCommandLine = [&](struct _cef_command_line_t* command_line) {
		auto append = [&](BNString s) {
			CefString str = s;
			command_line->append_switch(command_line, str.GetStruct());
		};
		

		append("disable-web-security");

		if (readConfig("cc.microblock.betterncm.ignore-certificate-errors", "false") == "true")
			append("ignore-certificate-errors");

		if (readConfig("cc.microblock.betterncm.single-process", "false") == "true")
			append("single-process");

		if (readConfig("cc.microblock.betterncm.experimental.optimize-memory", "false") == "true") {
			append("process-per-site");
			append("enable-low-end-device-mode");
			append("enable-low-res-tiling");
		}
	};

	EasyCEFHooks::onLoadStart = [=](_cef_browser_t* browser, _cef_frame_t* frame) {
		CefString url;
		if (frame) url = cefFromCEFUserFreeTakeOwnership(frame->get_url(frame));

		if (frame->is_main(frame) && frame->is_valid(frame)) {
			if (BNString(url.ToWString()).startsWith(L"devtools://")) {
				auto cef_browser_host = browser->get_host(browser);
				auto hwnd = browser->get_host(browser)->get_window_handle(cef_browser_host);
				SetLayeredWindowAttributes(hwnd, NULL, NULL, NULL);
				return;
			}


			std::wstring url = frame->get_url(frame)->str;

			EasyCEFHooks::executeJavaScript(frame,
			                                R"(
(location.pathname==="/pub/app.html")&&!(function fixNCMReloadPosition() {
	let oChannelCall = channel.call;
	channel.call = (name,...args) => {
		if (name === "winhelper.setWindowPosition" && window.screenX !== 0) return; 
		oChannelCall(name,...args);
	};
})();
)", "betterncm://betterncm/fix_window_position.js");

			EasyCEFHooks::executeJavaScript(frame,
			                                "const BETTERNCM_API_KEY='" + apiKey + "'; " +
			                                "const BETTERNCM_API_PORT = " + std::to_string(this->server_port) + ";" +
			                                "const BETTERNCM_API_PATH = 'http://localhost:" + std::to_string(
				                                this->server_port) + "/api'; " +
			                                "const BETTERNCM_FILES_PATH = 'http://localhost:" + std::to_string(
				                                this->server_port) + "/local';" +
			                                "console.log('BetterNCM API Initialized on',BETTERNCM_API_PORT);" +
			                                load_string_resource(L"framework.js") + "\n\n" +
											R"(betterncm.app.readConfig("cc.microblock.pluginmarket.source","https://gitee.com/microblock/BetterNCMPluginsMarketData/raw/master/").then(betterncm_native.app.auto_update))",
			                                "betterncm://betterncm/framework.js");

			EasyCEFHooks::executeJavaScript(frame,
			                                R"(
betterncm.utils.waitForElement('.g-sd').then(ele=>{
    new MutationObserver(()=>{
        document.querySelector('.g-sd').style.display='';
    }).observe(ele,{attributes:true})
})
)", "betterncm://betterncm/fix_nav_disappear.js");

			

			if (readConfig("cc.microblock.betterncm.cpp_side_inject_feature_disabled", "false") != "true") {
				for (const auto &plugin : PluginManager::getAllPlugins()) {
					auto script = plugin->getStartupScript();
					if (script.has_value())
						EasyCEFHooks::executeJavaScript(
							frame, script.value(),
							std::string("file://") + plugin->manifest.slug + "/startup_script.js");
				}
			}
		}
	};

	EasyCEFHooks::onHijackRequest = [=](std::string url) -> std::function<std::wstring(std::wstring)> {
		if (readConfig("cc.microblock.betterncm.cpp_side_inject_feature_disabled", "false") == "true")return nullptr;
		std::vector<PluginHijackAction> hijack_actions;
		auto ncmVersion = getNCMExecutableVersion();

		for (const auto& plugin : PluginManager::getAllPlugins()) {
			for(const auto& [version_req, hijacks_actions]: plugin->manifest.hijacks) {
				if(semver::range::satisfies(ncmVersion,version_req))
				for (const auto& [hijack_url, actions] : hijacks_actions) {
					if (url.starts_with(hijack_url)) {
						for (const auto& action : actions) {
							hijack_actions.push_back(
								PluginHijackAction{
									action,
									plugin->manifest.slug,
									hijack_url
								}
							);
						}

					}
				}
			}
		}

		std::function<std::wstring(std::wstring)> processor = nullptr;

		if (pystring::startswith(url, "orpheus://orpheus/pub/app.html"))
			hijack_actions.push_back(
				PluginHijackAction{
					HijackActionReplace{
						"splash_screen",
						"<body ",
						R"(

<div id=loadingMask style="position: absolute; inset: 0px; background: linear-gradient(54deg, rgb(49, 16, 37), rgb(25, 37, 64)); z-index: 1000; display: flex; justify-content: center; align-items: center; pointer-events: none; opacity: 1;">
<div><svg fill="#ffffffcc"><use xlink:href="orpheus://orpheus/style/res/svg/topbar.sp.svg#logo_white"></use></svg></div></div><body )",
						
					},
					"betterncm",
					"orpheus://orpheus/pub/app.html",
					
				}
		);

		if (hijack_actions.size())
			processor = [=](std::wstring code) {
				for (const auto& hijack : hijack_actions) {
					auto previousCode = code;
					try {
						if(const auto& act= std::get_if<HijackActionRegex>(&hijack.action)) {
							const std::wregex hijack_regex{ utf8_to_wstring(act->from) };
							code = std::regex_replace(code, hijack_regex, utf8_to_wstring(act->to));
						}

						if (const auto& act = std::get_if<HijackActionReplace>(&hijack.action)) {
							std::cout<<" - from: "<< act->from << "\n - to: " << act->to << "\n - len: " << code.length();
							code = wreplaceAll(
								code, 
								utf8_to_wstring(act->from),
								utf8_to_wstring(act->to)
							);
							std::cout << "->" << code.length() << std::endl;
						}

						if (const auto& act = std::get_if<HijackActionAppend>(&hijack.action)) {
							code += utf8_to_wstring(act->code);
						}

						if (const auto& act = std::get_if<HijackActionPrepend>(&hijack.action)) {
							code = utf8_to_wstring(act->code) + code;
					 	}
						 
						if(previousCode!=code)
							std::visit([&hijack, this](const auto& value) {
								std::lock_guard<std::shared_timed_mutex> guard(succeeded_hijacks_lock);
								succeeded_hijacks.push_back(hijack.plugin_slug + "::" + value.id);
							}, hijack.action);
						
					}
					catch (std::exception& e) {
						std::cout << "Failed to hijack: " << e.what() << std::endl;
					}
				}
				return code;
			};

		return processor;
	};

	EasyCEFHooks::onAddCommandLine = [&](const std::string& arg) {
		bool remove = false;

		if (readConfig("cc.microblock.betterncm.remove-disable-gpu", "false") == "true") {
			remove = remove || pystring::index(arg, "disable-gpu") != -1;
		}

		if (readConfig("cc.microblock.betterncm.disable-logging", "true") == "true") {
			remove = remove || pystring::index(arg, "log-file") != -1;
		}

		return !remove;
	};

	EasyCEFHooks::InstallHooks();
}

App::~App() {
	HANDLE hThread = server_thread->native_handle();
	if (WaitForSingleObject(hThread, 4000) == WAIT_TIMEOUT)
		TerminateThread(hThread, 0);
	server_thread->detach();
	delete server_thread;
	httpServer = nullptr;

	if (fs::exists(datapath.utf8() + "/plugins_runtime"))
		fs::remove_all(datapath.utf8() + "/plugins_runtime");
}
