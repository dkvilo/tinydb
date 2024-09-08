import concurrent.futures
import time
import threading
import random
import argparse
import redis

key_counter = 1
counter_lock = threading.Lock()

def get_next_key():
    global key_counter
    with counter_lock:
        key = key_counter
        key_counter += 1
        return key

def send_redis_command(client_id, num_operations, redis_client, operation_type):
    print(f"Client {client_id}: Connected to the Redis server.")
    for _ in range(num_operations):
        if operation_type == 'set':
            key = get_next_key()
            value = f"m_value_{key}"
            redis_client.set(key, value)
        elif operation_type == 'get':
            with counter_lock:
                if key_counter > 1:
                    key = random.randint(1, key_counter - 1)
                    redis_client.get(key)
        else:  # operation_type == 'all'
            command_type = random.choice(['set', 'get'])
            if command_type == 'set':
                key = get_next_key()
                value = f"m_value_{key}"
                redis_client.set(key, value)
            else:
                with counter_lock:
                    if key_counter > 1:
                        key = random.randint(1, key_counter - 1)
                        redis_client.get(key)

    print(f"Client {client_id}: Finished sending {num_operations} operations.")


def perform_stress_test(num_clients, num_operations, host, port, operation_type):
    redis_client = redis.Redis(host=host, port=port)
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_clients) as executor:
        futures = []
        operations_per_client = num_operations // num_clients
        
        for client_id in range(num_clients):
            futures.append(executor.submit(send_redis_command, client_id, operations_per_client, redis_client, operation_type))

        for future in concurrent.futures.as_completed(futures):
            future.result()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Perform a Redis stress test.')
    parser.add_argument('--host', type=str, default='127.0.0.1', help='Redis server host to connect to')
    parser.add_argument('--port', type=int, default=6379, help='Redis server port to connect to')
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
