const net = require("net");

class TinyDBClient {
  constructor(host = "localhost", port = 8079) {
    this.host = host;
    this.port = port;
    this.client = new net.Socket();
    this.connected = false;
    this.queue = [];
    this.currentCallback = null;
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.client.connect(this.port, this.host, () => {
        this.connected = true;
        this.processQueue();
        resolve();
      });

      this.client.on("data", (data) => {
        if (this.currentCallback) {
          this.currentCallback(null, data.toString().trim());
          this.currentCallback = null;
          this.processQueue();
        }
      });

      this.client.on("error", (err) => {
        if (!this.connected) {
          reject(err);
        } else if (this.currentCallback) {
          this.currentCallback(err);
          this.currentCallback = null;
          this.processQueue();
        }
      });

      this.client.on("close", () => {
        this.connected = false;
      });
    });
  }

  disconnect() {
    return new Promise((resolve) => {
      this.client.end(() => {
        this.connected = false;
        resolve();
      });
    });
  }

  sendCommand(command, ...args) {
    return new Promise((resolve, reject) => {
      const callback = (err, result) => {
        if (err) reject(err);
        else resolve(result);
      };

      const cmd = [command, ...args].join(" ") + "\n";

      if (!this.connected || this.currentCallback) {
        this.queue.push({ cmd, callback });
      } else {
        this.currentCallback = callback;
        this.client.write(cmd);
      }
    });
  }

  processQueue() {
    if (this.queue.length > 0 && !this.currentCallback) {
      const { cmd, callback } = this.queue.shift();
      this.currentCallback = callback;
      this.client.write(cmd);
    }
  }

  set(key, value) {
    return this.sendCommand("SET", key, value);
  }

  get(key) {
    return this.sendCommand("GET", key);
  }

  append(key, value) {
    return this.sendCommand("APPEND", key, value);
  }

  strlen(key) {
    return this.sendCommand("STRLEN", key);
  }
}

module.exports = TinyDBClient;
