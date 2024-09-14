/*
    This is for testing, we will not use text format for sending commands and receiving response from server,
    we need to design binary protocol that will be faster to send and process,
    for example command packet could look like this:

    | Command ID  | Key Length | Key              | Value Length |     Value          |
    |-------------|------------|------------------|--------------|--------------------|
    | 1 byte      | 4 bytes    | Variable         | 4 bytes      |    Variable        |

    | Byte | Length | Name   |
    |------|--------|--------|
    | 0x01 | 4      | SET\0  |
    | 0x02 | 4      | GET\0  |
    
    etc ...

    Response packet example:

    | Response Type| Data Length  | Response Data|
    |--------------|--------------|--------------|
    | 1 byte       | 4 bytes      | Variable     |

    - David
*/
#![allow(dead_code)]

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

    fn escape_value(value: &str) -> String {
        if value.contains(' ') || value.contains('"') {
            format!("\"{}\"", value.replace('"', "\\\""))
        } else {
            value.to_string()
        }
    }

    pub fn set(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        let escaped_value = Self::escape_value(value);
        self.send_command(&format!("SET {} {}", key, escaped_value))
    }

    pub fn get(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("GET {}", key))
    }

    pub fn incr(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("INCR {}", key))
    }

    pub fn append(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        let escaped_value = Self::escape_value(value);
        self.send_command(&format!("APPEND {} {}", key, escaped_value))
    }

    pub fn strlen(&mut self, key: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("STRLEN {}", key))
    }

    pub fn export(&mut self, filename: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("EXPORT {}", filename))
    }

    pub fn rpush(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        let escaped_value = Self::escape_value(value);
        self.send_command(&format!("RPUSH {} {}", key, escaped_value))
    }

    pub fn lpush(&mut self, key: &str, value: &str) -> Result<String, std::io::Error> {
        let escaped_value = Self::escape_value(value);
        self.send_command(&format!("LPUSH {} {}", key, escaped_value))
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
        let response = self.send_command(&format!("LRANGE {} {} {}", key, start, stop))?;
        Ok(format!("{}", response))
    }

    pub fn subscribe(&mut self, channel: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("SUB {}", channel))
    }

    pub fn unsubscribe(&mut self, channel: &str) -> Result<String, std::io::Error> {
        self.send_command(&format!("UNSUB {}", channel))
    }

    pub fn publish(&mut self, channel: &str, message: &str) -> Result<String, std::io::Error> {
        let escaped_message = Self::escape_value(message);
        self.send_command(&format!("PUB {} {}", channel, escaped_message))
    }
}
