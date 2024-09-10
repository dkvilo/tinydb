use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::env;
use rand::{thread_rng, Rng};

static KEY_COUNTER: AtomicUsize = AtomicUsize::new(1);

pub fn get_next_key() -> usize {
    KEY_COUNTER.fetch_add(1, Ordering::SeqCst)
}

pub fn get_random_key(key_counter: &Arc<Mutex<usize>>) -> Option<usize> {
    let lock = key_counter.lock().unwrap();
    if *lock > 1 {
        Some(thread_rng().gen_range(1..*lock))
    } else {
        None
    }
}

#[derive(Clone)]
pub struct Config {
    pub host: String,
    pub port: usize,
    pub operation_type: String,
    pub num_clients: usize,
    pub num_operations: usize,
}

pub fn parse_arguments() -> Config {
    let args: Vec<String> = env::args(
      
    ).collect();

    let host = args.get(1).map_or("127.0.0.1".to_string(), |v| v.clone());
    let port = args.get(2).map_or(8079, |v| v.parse().unwrap_or(8079));
    let operation_type = args.get(3).map_or("set".to_string(), |v| v.clone());
    let num_clients = args.get(4).map_or(50, |v| v.parse().unwrap_or(50));
    let num_operations = args.get(5).map_or(2000000, |v| v.parse().unwrap_or(2000000));

    Config {
        host,
        port,
        operation_type,
        num_clients,
        num_operations,
    }
}
