import socket
import concurrent.futures
import time
import threading
import random
import argparse

key_counter = 1
counter_lock = threading.Lock()

def get_next_key():
    global key_counter
    with counter_lock:
        key = key_counter
        key_counter += 1
        return key

def send_commands(client_id, num_operations, host, port, operation_type):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5) 
            s.connect((host, port))
            print(f"Client {client_id}: Connected to the server.")
            
            for _ in range(num_operations):
                if operation_type == 'set':
                    command_type = 'SET'
                    key = get_next_key()
                    value = f"m_value_{key}"
                    command = f"SET {key} {value}\n"
                elif operation_type == 'get':
                    with counter_lock:
                        if key_counter > 1:
                            key = random.randint(1, key_counter - 1)
                            command = f"GET {key}\n"
                        else:
                            continue
                else:  # operation_type == 'all'
                    command_type = random.choice(['SET', 'GET'])
                    if command_type == 'SET':
                        key = get_next_key()
                        value = f"m_value_{key}"
                        command = f"SET {key} {value}\n"
                    else:
                        with counter_lock:
                            if key_counter > 1:
                                key = random.randint(1, key_counter - 1)
                                command = f"GET {key}\n"
                            else:
                                continue
                s.sendall(command.encode())
                try:
                    response = s.recv(1024)
                    # print(f"Client {client_id} received: {response.decode().strip()}")
                except socket.timeout:
                    print(f"Client {client_id}: Timeout waiting for response.")

            print(f"Client {client_id}: Finished sending {num_operations} operations.")
    except Exception as e:
        print(f"Client {client_id}: Error - {e}")

def perform_stress_test(num_clients, num_operations, host, port, operation_type):
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_clients) as executor:
        futures = []
        operations_per_client = num_operations // num_clients
        
        for client_id in range(num_clients):
            futures.append(executor.submit(send_commands, client_id, operations_per_client, host, port, operation_type))

        for future in concurrent.futures.as_completed(futures):
            future.result()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Perform a stress test on a server.')
    parser.add_argument('--host', type=str, default='0.0.0.0', help='Server host to connect to')
    parser.add_argument('--port', type=int, default=8079, help='Server port to connect to')
    parser.add_argument('--op', type=str, choices=['set', 'get', 'all'], default='all', help='Operation type: set, get, or all')
    parser.add_argument('--client', type=int, default=50, help='Number of clients')
    parser.add_argument('--num-op', type=int, default=1000000, help='Number of operations to perform')

    args = parser.parse_args()

    start_time = time.time()
    perform_stress_test(
        num_clients=args.client, 
        num_operations=args.num_op, 
        host=args.host, 
        port=args.port, 
        operation_type=args.op
    )
    end_time = time.time()
    print(f"Stress test completed in {end_time - start_time:.2f} seconds")
