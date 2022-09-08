use std::net::TcpStream;
use super::TcpStreamExt;

pub fn handle_request(stream: &mut TcpStream, method: &str, path: &str, body: &[u8]) {
    if let Some(path) = path.strip_prefix("/api") {
        return match path {
            "/app/version" => {
                stream.write_text_response("1.0.0");
            }
            "/utils/show_console" => {
                unsafe {
                    let hwnd = winapi::um::wincon::GetConsoleWindow();
                    if hwnd.is_null() {
                        winapi::um::consoleapi::AllocConsole();
                    } else {
                        winapi::um::winuser::ShowWindow(hwnd, winapi::um::winuser::SW_SHOW);
                    }
                }
                stream.write_ok_response();
            }
            "/utils/hide_console" => {
                unsafe {
                    let hwnd = winapi::um::wincon::GetConsoleWindow();
                    winapi::um::wincon::FreeConsole();
                    if !hwnd.is_null() {
                        winapi::um::winuser::ShowWindow(hwnd, winapi::um::winuser::SW_HIDE);
                    }
                }
                stream.write_ok_response();
            }
            other => {
                if let Some(dir_path) = other.strip_prefix("/fs/read_dir?path=") {
                    if let Ok(dir_path) = urlencoding::decode(dir_path) {
                        if let Ok(read_dir) = std::fs::read_dir(dir_path.into_owned()) {
                            let mut result = Vec::with_capacity(64);

                            for entry in read_dir.flatten() {
                                result.push(tinyjson::JsonValue::String(
                                    entry.path().to_string_lossy().to_string(),
                                ));
                            }

                            let result = tinyjson::JsonValue::Array(result);
                            if let Ok(result) = result.stringify() {
                                stream.write_json_response(&result);
                                return;
                            }
                        }
                    }
                } else if let Some(dir_path) = other.strip_prefix("/fs/mkdir?path=") {
                    if let Ok(dir_path) = urlencoding::decode(dir_path) {
                        let _ = std::fs::create_dir_all(dir_path.into_owned());
                        stream.write_ok_response();
                        return
                    }
                } else if let Some(dir_path) = other.strip_prefix("/fs/exists?path=") {
                    if let Ok(dir_path) = urlencoding::decode(dir_path) {
                        let _ = std::fs::create_dir_all(dir_path.into_owned());
                        stream.write_ok_response();
                        return
                    }
                } else if let Some(dir_path) = other.strip_prefix("/fs/remove?path=") {
                    if let Ok(dir_path) = urlencoding::decode(dir_path) {
                        let dir_path = dir_path.into_owned();
                        let dir_path = std::path::Path::new(&dir_path);
                        if dir_path.is_dir() {
                            let _ = std::fs::remove_dir_all(dir_path);
                        } else {
                            let _ = std::fs::remove_file(dir_path);
                        }
                        stream.write_ok_response();
                        return
                    }
                } else if let Some(file_path) = other.strip_prefix("/fs/read_file_text?path=") {
                    if let Ok(file_path) = urlencoding::decode(file_path) {
                        if let Ok(data) = std::fs::read_to_string(file_path.into_owned()) {
                            stream.write_text_response(&data);
                            return;
                        }
                    }
                } else if let Some(file_path) = other.strip_prefix("/fs/write_file_text?path=") {
                    if method == "POST" {
                        if let Ok(file_path) = urlencoding::decode(file_path) {
                            if std::fs::write(file_path.into_owned(), body).is_ok() {
                                stream.write_ok_response();
                                return;
                            }
                        }
                    }
                }
                println!("[WARN] Unknown path {}", other);
                stream.write_not_found_response();
            }
        };
    }
}
