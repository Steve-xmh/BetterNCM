use std::{
    io::{BufRead, BufReader, ErrorKind, Read, Write},
    net::{TcpListener, TcpStream},
    sync::{atomic::AtomicBool, Mutex},
    thread::JoinHandle,
};

trait TcpStreamExt: Write {
    fn write_ok_response(&mut self) {
        let _ = self.write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    }

    fn write_not_found_response(&mut self) {
        let _ = self.write_all("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n".as_bytes());
    }

    fn write_text_response(&mut self, content: &str) {
        let length = content.len();
        let _ = self.write_all(format!("HTTP/1.1 200 OK\r\nContent-Length: {length}\r\nContent-Type: text/plain\r\n\r\n{content}").as_bytes());
    }

    fn write_json_response(&mut self, content: &str) {
        let length = content.len();
        let _ = self.write_all(format!("HTTP/1.1 200 OK\r\nContent-Length: {length}\r\nContent-Type: application/json\r\n\r\n{content}").as_bytes());
    }
}

impl TcpStreamExt for TcpStream {}

static SERVER_SHUTDOWN: AtomicBool = AtomicBool::new(false);
static SERVER_THREAD: Mutex<Option<JoinHandle<()>>> = Mutex::new(None);

pub fn init_server() {
    let handle = std::thread::spawn(move || match TcpListener::bind("127.0.0.1:3248") {
        Ok(listener) => {
            println!("Server is started!");
            for stream in listener.incoming() {
                if SERVER_SHUTDOWN.load(std::sync::atomic::Ordering::Relaxed) {
                    break;
                }
                match stream {
                    Ok(stream) => {
                        handle_stream(stream);
                    }
                    Err(e) => {
                        println!("[WARN] Connect error: {}", e);
                    }
                }
            }
            println!("Server is stopped!");
        }
        Err(e) if e.kind() == ErrorKind::AddrInUse => {
            println!("Server already started");
        }
        Err(e) => {
            println!("[ERROR] Can't create server: {}", e);
        }
    });
    if let Ok(mut thread) = SERVER_THREAD.lock() {
        *thread = Some(handle);
    }
}

fn handle_stream(mut stream: TcpStream) {
    println!("[Server] Got TcpStream");
    let mut req_data: Vec<u8> = Vec::with_capacity(4096);
    let mut method = "UNKNOWN".to_string();
    let mut path = "UNKNOWN".to_string();
    let mut body_position = None;
    let mut content_length = None;
    let mut buf = [0u8; 16];
    while let Ok(buf_len) = stream.read(&mut buf) {
        if let Some(body_position) = body_position {
            let content_length = content_length.unwrap_or(0);
            if req_data.len() >= body_position + content_length {
                handle_request(
                    &mut stream,
                    &method,
                    &path,
                    &req_data[body_position..body_position + content_length],
                );
                return;
            }
        } else {
            let mut headers = [httparse::EMPTY_HEADER; 256];
            let mut req = httparse::Request::new(&mut headers);
            req_data.extend(&buf[0..buf_len]);
            match req.parse(&req_data) {
                Ok(a) => {
                    if a.is_complete() {
                        path = req.path.map(ToOwned::to_owned).unwrap_or_default();
                        method = req.method.map(ToOwned::to_owned).unwrap_or_default();
                        content_length = Some(0);

                        if let Some(header) = req
                            .headers
                            .iter()
                            .find(|x| x.name.eq_ignore_ascii_case("content-length"))
                        {
                            if let Ok(value) = std::str::from_utf8(header.value) {
                                if let Ok(len) = str::parse::<usize>(value) {
                                    content_length = Some(len)
                                }
                            }
                        }

                        body_position = Some(a.unwrap());
                        let content_length = content_length.unwrap_or(0);
                        if content_length == 0 {
                            let body_position = body_position.unwrap_or(0);
                            handle_request(
                                &mut stream,
                                &method,
                                &path,
                                &req_data[body_position..body_position + content_length],
                            );
                            return;
                        }
                    }
                }
                Err(err) => {
                    println!("Error on parsing http header: {:?}", err);
                    stream.write_not_found_response();
                    return;
                }
            }
        }
    }
}

fn handle_request(stream: &mut TcpStream, method: &str, path: &str, body: &[u8]) {
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

pub fn uninit_server() {
    SERVER_SHUTDOWN.store(true, std::sync::atomic::Ordering::Relaxed);
    if let Ok(mut thread) = SERVER_THREAD.lock() {
        if let Some(thread) = thread.take() {
            // Manually send an empty request to wake up server and stop.
            let _ = TcpStream::connect("127.0.0.1:3248");
            let _ = thread.join();
        }
    }
}
