mod common;

use std::net::TcpStream;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;
use std::io::{Write, Read};

fn send_redis_resp_command(client_id: usize, num_operations: usize, host: &str, port: usize, operation_type: String, key_counter: Arc<Mutex<usize>>) {
    let addr = format!("{}:{}", host, port);
    if let Ok(mut stream) = TcpStream::connect(&addr) {
        println!("Client {}: Connected to the Redis server (RESP).", client_id);

        for _ in 0..num_operations {
            let command = match operation_type.as_str() {
                "set" => {
                    let key = common::get_next_key();
                    let value = format!("m_value_{}", key);
                    format!("*3\r\n$3\r\nSET\r\n${}\r\n{}\r\n${}\r\n{}\r\n", key.to_string().len(), key, value.len(), value)
                }
                "get" => {
                    if let Some(key) = common::get_random_key(&key_counter) {
                        format!("*2\r\n$3\r\nGET\r\n${}\r\n{}\r\n", key.to_string().len(), key)
                    } else {
                        continue;
                    }
                }
                _ => {
                    let command_type = if rand::random() { "set" } else { "get" };
                    if command_type == "set" {
                        let key = common::get_next_key();
                        let value = format!("m_value_{}", key);
                        format!("*3\r\n$3\r\nSET\r\n${}\r\n{}\r\n${}\r\n{}\r\n", key.to_string().len(), key, value.len(), value)
                    } else if let Some(key) = common::get_random_key(&key_counter) {
                        format!("*2\r\n$3\r\nGET\r\n${}\r\n{}\r\n", key.to_string().len(), key)
                    } else {
                        continue;
                    }
                }
            };

            if stream.write_all(command.as_bytes()).is_ok() {
                let mut buffer = [0; 1024];
                let _ = stream.read(&mut buffer);
            }
        }
        println!("Client {}: Finished sending {} operations.", client_id, num_operations);
    }
}

fn main() {
    let config = common::parse_arguments();
    let key_counter = Arc::new(Mutex::new(common::get_next_key()));

    let start_time = Instant::now();
    let handles: Vec<_> = (0..config.num_clients).map(|client_id| {
        let key_counter = Arc::clone(&key_counter);
        let operation_type = config.operation_type.clone();
        let host = config.host.clone();
        let port = config.port;

        thread::spawn(move || {
            send_redis_resp_command(client_id, config.num_operations / config.num_clients, &host, port, operation_type, key_counter);
        })
    }).collect();

    for handle in handles {
        handle.join().unwrap();
    }

    let elapsed_time = start_time.elapsed().as_secs_f64();
    println!("Stress test (RESP) completed in {:.2} seconds", elapsed_time);
}
