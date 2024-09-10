mod common;

use std::net::TcpStream;
use std::io::{Write, Read};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

fn send_db_command(
    port: usize,
    host: String,
    client_id: usize,
    num_operations: usize,
    operation_type: String,
    key_counter: Arc<Mutex<usize>>,
) {
    let addr = format!("{}:{}", host, port);

    if let Ok(mut stream) = TcpStream::connect(addr) {
        println!("Client {}: Connected to custom database.", client_id);

        for _ in 0..num_operations {
            match operation_type.as_str() {
                "set" => {
                    let key = common::get_next_key();
                    let value = format!("m_value_{}", key);
                    let command = format!("SET {} {}\n", key, value);
                    
                    if stream.write_all(command.as_bytes()).is_ok() {
                        let mut buffer = [0; 512];
                        if let Ok(_) = stream.read(&mut buffer) {
                            // println!("Response: {}", String::from_utf8_lossy(&buffer));
                        }
                    }
                }
                "get" => {
                    if let Some(key) = common::get_random_key(&key_counter) {
                        let command = format!("GET {}\n", key);
                        if stream.write_all(command.as_bytes()).is_ok() {
                            let mut buffer = [0; 512]; 
                            if let Ok(_) = stream.read(&mut buffer) {
                                // println!("Response: {}", String::from_utf8_lossy(&buffer));
                            }
                        }
                    }
                }
                _ => {
                    let command_type = if rand::random() { "set" } else { "get" };
                    if command_type == "set" {
                        let key = common::get_next_key();
                        let value = format!("m_value_{}", key);
                        let command = format!("SET {} {}\n", key, value);

                        if stream.write_all(command.as_bytes()).is_ok() {
                            let mut buffer = [0; 512];
                            if let Ok(_) = stream.read(&mut buffer) {
                            }
                        }
                    } else if let Some(key) = common::get_random_key(&key_counter) {
                        let command = format!("GET {}\n", key);
                        if stream.write_all(command.as_bytes()).is_ok() {
                            let mut buffer = [0; 512];
                            if let Ok(_) = stream.read(&mut buffer) {
                            }
                        }
                    }
                }
            }
        }
        println!("Client {}: Finished sending {} operations to custom database.", client_id, num_operations);
    } else {
        println!("Client {}: Failed to connect to custom database.", client_id);
    }
}


fn main() {
  let config = common::parse_arguments();
  let key_counter = Arc::new(Mutex::new(common::get_next_key()));

  let start_time = Instant::now();
  let handles: Vec<_> = (0..config.num_clients).map(|client_id| {
      let key_counter = Arc::clone(&key_counter);
      let operation_type = config.operation_type.clone();

      let host_value = config.host.clone();
      thread::spawn(move || {
          send_db_command(
            config.port, host_value,
            client_id, config.num_operations / config.num_clients, operation_type, key_counter);
      })
  }).collect();

  for handle in handles {
      handle.join().unwrap();
  }

  let elapsed_time = start_time.elapsed().as_secs_f64();
  println!("Stress test (custom database) completed in {:.2} seconds", elapsed_time);
}

