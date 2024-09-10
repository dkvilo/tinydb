use std::io::{BufReader, BufRead, Write};
use std::net::TcpStream;
use std::str;

pub struct TinyDBClient {
    stream: TcpStream,
}

impl TinyDBClient {
    pub fn new(address: &str) -> Result<Self, std::io::Error> {
        let stream = TcpStream::connect(address)?;
        Ok(TinyDBClient { stream })
    }

    fn send_command(&mut self, command: &str) -> Result<String, std::io::Error> {
        self.stream.write_all(command.as_bytes())?;
        self.stream.write_all(b"\n")?;
        self.stream.flush()?;

        let mut reader = BufReader::new(&self.stream);
        let mut response = String::new();
        reader.read_line(&mut response)?;

        Ok(response.trim().to_string())
    }

    pub fn set(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("SET {} {}", key, value))
    }

    pub fn get(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("GET {}", key))
    }

    pub fn incr(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("INCR {}", key))
    }

    pub fn append(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("APPEND {} {}", key, value))
    }

    pub fn strlen(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("STRLEN {}", key))
    }

    pub fn export(&mut self, filename: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("EXPORT {}", filename))
    }

    pub fn rpush(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("RPUSH {} {}", key, value))
    }

    pub fn lpush(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("LPUSH {} {}", key, value))
    }

    pub fn rpop(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("RPOP {}", key))
    }

    pub fn lpop(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("LPOP {}", key))
    }

    pub fn llen(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("LLEN {}", key))
    }

    pub fn lrange(&mut self, key: &str, start: i32, stop: i32) -> Result<String, std::io::Error> {
        self.send_command(&format!("LRANGE {} {} {}", key, start, stop))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_set_get() {
        let mut client = TinyDBClient::new("127.0.0.1:8079").unwrap();
        assert_eq!(client.set("test_key", "test_value").unwrap(), "Ok");
        assert_eq!(client.get("test_key").unwrap(), "test_value");
    }
    
    #[test]
    fn test_list() {
        let mut client = TinyDBClient::new("127.0.0.1:8079").unwrap();
        assert_eq!(client.lpush("test_list", "1").unwrap(), "Ok");
        assert_eq!(client.rpush("test_list", "2").unwrap(), "Ok");
    }
}