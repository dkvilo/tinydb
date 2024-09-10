mod common;

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;
use redis::Commands;

fn send_redis_command(client_id: usize, num_operations: usize, mut redis_client: redis::Connection, operation_type: String, key_counter: Arc<Mutex<usize>>) {
    println!("Client {}: Connected to the Redis server (crate).", client_id);

    for _ in 0..num_operations {
        match operation_type.as_str() {
            "set" => {
                let key = common::get_next_key();
                let value = format!("m_value_{}", key);
                let _: () = redis_client.set(key, value).expect("Failed to execute SET command");
            }
            "get" => {
                if let Some(key) = common::get_random_key(&key_counter) {
                    let _: Option<String> = redis_client.get(key).expect("Failed to execute GET command");
                }
            }
            _ => {
                let command_type = if rand::random() { "set" } else { "get" };
                if command_type == "set" {
                    let key = common::get_next_key();
                    let value = format!("m_value_{}", key);
                    let _: () = redis_client.set(key, value).expect("Failed to execute SET command");
                } else if let Some(key) = common::get_random_key(&key_counter) {
                    let _: Option<String> = redis_client.get(key).expect("Failed to execute GET command");
                }
            }
        }
    }

    println!("Client {}: Finished sending {} operations.", client_id, num_operations);
}
fn main() {
    let config = common::parse_arguments();
    println!("Connecting to Redis at {}:{}", config.host, config.port);

    let client = redis::Client::open(format!("redis://{}:{}/", config.host, config.port))
        .expect("Failed to create Redis client");

    let key_counter = Arc::new(Mutex::new(common::get_next_key()));

    let start_time = Instant::now();
    let handles: Vec<_> = (0..config.num_clients).map(|client_id| {
        let redis_conn = client.get_connection().expect("Failed to connect to Redis");
        let key_counter = Arc::clone(&key_counter);
        let operation_type = config.operation_type.clone();

        thread::spawn(move || {
            send_redis_command(client_id, config.num_operations / config.num_clients, redis_conn, operation_type, key_counter);
        })
    }).collect();

    for handle in handles {
        handle.join().unwrap();
    }

    let elapsed_time = start_time.elapsed().as_secs_f64();
    println!("Stress test completed in {:.2} seconds", elapsed_time);
}


