#!/bin/bash

echo "Building TinyDB..."
make

echo "Building authentication tests..."
make auth_test
make auth_integration_test

echo "Starting TinyDB server..."
./tinydb &
SERVER_PID=$!

sleep 2

echo "Running basic authentication tests..."
./test/auth_test

echo "Running authentication integration tests..."
./test/auth_integration_test

echo "Stopping TinyDB server..."
kill $SERVER_PID

echo "All authentication tests completed!" 