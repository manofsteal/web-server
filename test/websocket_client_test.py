#!/usr/bin/env python3
"""
Simple WebSocket client to test the C++ WebSocket server example.
Tests both the echo route (/) and chat route (/chat).

Can test both standalone WebSocket server (port 8765) and unified server (port 8080).
"""

import asyncio
import websockets
import sys
import argparse


# This intend to used to test websocket_server_example.cpp (port 8765)
# or unified_server_example.cpp (port 8080)

async def test_echo_route(port=8765, path="/"):
    """Test the echo route"""
    uri = f"ws://localhost:{port}{path}"
    print(f"\n=== Testing Echo Route: {uri} ===")

    try:
        async with websockets.connect(uri) as websocket:
            print("✓ Connected to echo server")

            # Test 1: Send text message
            test_message = "Hello, WebSocket Echo Server!"
            print(f"→ Sending text: {test_message}")
            await websocket.send(test_message)

            response = await websocket.recv()
            print(f"← Received: {response}")

            if response == test_message:
                print("✓ Echo test passed!")
            else:
                print(f"✗ Echo test failed! Expected: {test_message}, Got: {response}")

            # Test 2: Send binary message
            binary_data = b"Binary test data \x00\x01\x02\x03"
            print(f"\n→ Sending binary data ({len(binary_data)} bytes)")
            await websocket.send(binary_data)

            binary_response = await websocket.recv()
            print(f"← Received binary data ({len(binary_response)} bytes)")

            if binary_response == binary_data:
                print("✓ Binary echo test passed!")
            else:
                print(f"✗ Binary echo test failed!")

            # Test 3: Multiple messages
            print("\n→ Sending multiple messages...")
            for i in range(3):
                msg = f"Message #{i+1}"
                await websocket.send(msg)
                response = await websocket.recv()
                print(f"  {i+1}. Sent: {msg} → Received: {response}")

            print("✓ Multiple message test completed!")

    except ConnectionRefusedError:
        print("✗ Connection refused. Is the server running on port 8765?")
        sys.exit(1)
    except Exception as e:
        print(f"✗ Error: {e}")
        sys.exit(1)


async def test_chat_route(port=8765, path="/chat"):
    """Test the chat route"""
    uri = f"ws://localhost:{port}{path}"
    print(f"\n=== Testing Chat Route: {uri} ===")

    try:
        async with websockets.connect(uri) as websocket:
            print("✓ Connected to chat server")

            # Send some chat messages
            messages = [
                "Hello!",
                "How are you?",
                "This is a test message"
            ]

            for msg in messages:
                print(f"→ Sending: {msg}")
                await websocket.send(msg)

                response = await websocket.recv()
                print(f"← Received: {response}")

                expected = f"Chat response: {msg}"
                if response == expected:
                    print(f"✓ Chat response correct!")
                else:
                    print(f"✗ Unexpected response! Expected: {expected}")

    except ConnectionRefusedError:
        print("✗ Connection refused. Is the server running on port 8765?")
        sys.exit(1)
    except Exception as e:
        print(f"✗ Error: {e}")
        sys.exit(1)


async def test_multiple_connections(port=8765, path="/", num_connections=10, messages_per_conn=5):
    """Test multiple concurrent WebSocket connections"""
    uri = f"ws://localhost:{port}{path}"
    print(f"\n=== Testing Multiple Connections: {uri} ===")
    print(f"Creating {num_connections} concurrent connections...")
    print(f"Each connection will send {messages_per_conn} messages\n")

    import time
    start_time = time.time()

    async def handle_single_connection(conn_id):
        """Handle a single connection"""
        try:
            async with websockets.connect(uri) as websocket:
                print(f"✓ Connection {conn_id}: Connected")

                messages_sent = 0
                messages_received = 0

                for i in range(messages_per_conn):
                    msg = f"Connection-{conn_id} Message-{i+1}"
                    await websocket.send(msg)
                    messages_sent += 1

                    response = await websocket.recv()
                    messages_received += 1

                    # Verify echo response
                    if response == msg:
                        print(f"  Connection {conn_id}: Message {i+1}/{messages_per_conn} ✓")
                    else:
                        print(f"  Connection {conn_id}: Message {i+1}/{messages_per_conn} ✗ (Expected: {msg}, Got: {response})")

                print(f"✓ Connection {conn_id}: Completed ({messages_sent} sent, {messages_received} received)")
                return True

        except Exception as e:
            print(f"✗ Connection {conn_id}: Error - {e}")
            return False

    # Create all connections concurrently
    tasks = [handle_single_connection(i+1) for i in range(num_connections)]
    results = await asyncio.gather(*tasks, return_exceptions=True)

    # Calculate statistics
    elapsed_time = time.time() - start_time
    successful = sum(1 for r in results if r is True)
    failed = num_connections - successful
    total_messages = num_connections * messages_per_conn

    print(f"\n{'='*60}")
    print("Multiple Connection Test Results:")
    print(f"{'='*60}")
    print(f"  Total connections:      {num_connections}")
    print(f"  Successful:             {successful}")
    print(f"  Failed:                 {failed}")
    print(f"  Messages per connection: {messages_per_conn}")
    print(f"  Total messages sent:    {total_messages}")
    print(f"  Total messages received: {successful * messages_per_conn}")
    print(f"  Time elapsed:           {elapsed_time:.2f}s")
    print(f"  Messages per second:    {total_messages / elapsed_time:.2f}")
    print(f"{'='*60}")

    if successful == num_connections:
        print("✓ All connections completed successfully!")
    else:
        print(f"✗ {failed} connection(s) failed")


async def main():
    """Main function to run all tests"""
    parser = argparse.ArgumentParser(
        description='WebSocket client test suite for C++ WebSocket server',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s                           # Test standalone server (port 8765, path /)
  %(prog)s -p 8080                    # Test on port 8080
  %(prog)s -p 8080 --path /ws/echo    # Test on port 8080 with path /ws/echo
  %(prog)s -m                         # Test 10 concurrent connections
  %(prog)s -m -c 50 -n 10             # Test 50 connections, 10 messages each
  %(prog)s -m -c 20 -p 8080           # Test 20 connections on port 8080
  %(prog)s -m -c 100 -p 9876          # Test 100 connections on port 9876
        '''
    )

    parser.add_argument('-p', '--port', type=int, default=8765, metavar='PORT',
                        help='Server port (default: 8765)')
    parser.add_argument('--path', type=str, default='/', metavar='PATH',
                        help='WebSocket path (default: /)')
    parser.add_argument('-m', '--multi', action='store_true',
                        help='Test multiple concurrent connections')
    parser.add_argument('-c', '--connections', type=int, default=10, metavar='N',
                        help='Number of concurrent connections (default: 10, use with -m)')
    parser.add_argument('-n', '--messages', type=int, default=5, metavar='M',
                        help='Messages per connection (default: 5, use with -m)')

    args = parser.parse_args()

    # Use specified port and path
    port = args.port
    echo_path = args.path
    chat_path = "/chat" if args.path == "/" else args.path

    # Run appropriate test mode
    if args.multi:
        # Multiple connection test
        print(f"Testing on port {port}")
        await test_multiple_connections(port, echo_path, args.connections, args.messages)
    else:
        # Run automated tests
        print("=" * 60)
        print(f"WebSocket Client Test Suite - Port {port}")
        print("=" * 60)

        await test_echo_route(port, echo_path)
        await asyncio.sleep(0.5)  # Small delay between tests

        await test_chat_route(port, chat_path)

        print("\n" + "=" * 60)
        print("All tests completed!")
        print("=" * 60)
        print("\nTip: Run with -m flag for multiple connection test:")
        print(f"  python websocket_client_test.py -m -c 20")
        print("\nOr test on different port:")
        print(f"  python websocket_client_test.py -p 8080 --path /ws/echo")


if __name__ == "__main__":
    asyncio.run(main())
