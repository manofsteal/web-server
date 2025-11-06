#!/usr/bin/env python3
"""
Simple WebSocket client to test the C++ WebSocket server example.
Tests both the echo route (/) and chat route (/chat).
"""

import asyncio
import websockets
import sys


# This intend to used to test websocket_server_example.cpp

async def test_echo_route():
    """Test the echo route at ws://localhost:8765/"""
    uri = "ws://localhost:8765/"
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


async def test_chat_route():
    """Test the chat route at ws://localhost:8765/chat"""
    uri = "ws://localhost:8765/chat"
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


async def interactive_mode(route="/"):
    """Interactive mode to send custom messages"""
    uri = f"ws://localhost:8765{route}"
    print(f"\n=== Interactive Mode: {uri} ===")
    print("Type messages to send. Press Ctrl+C or type 'quit' to exit.\n")

    try:
        async with websockets.connect(uri) as websocket:
            print("✓ Connected! Start typing messages...\n")

            while True:
                # Read user input
                message = await asyncio.get_event_loop().run_in_executor(
                    None, input, "You: "
                )

                if message.lower() in ['quit', 'exit', 'q']:
                    print("Closing connection...")
                    break

                # Send message
                await websocket.send(message)

                # Receive response
                response = await websocket.recv()
                print(f"Server: {response}\n")

    except KeyboardInterrupt:
        print("\nExiting...")
    except ConnectionRefusedError:
        print("✗ Connection refused. Is the server running on port 8765?")
        sys.exit(1)
    except Exception as e:
        print(f"✗ Error: {e}")
        sys.exit(1)


async def main():
    """Main function to run all tests"""
    if len(sys.argv) > 1:
        if sys.argv[1] == "--interactive" or sys.argv[1] == "-i":
            route = sys.argv[2] if len(sys.argv) > 2 else "/"
            await interactive_mode(route)
            return
        elif sys.argv[1] == "--help" or sys.argv[1] == "-h":
            print("Usage:")
            print("  python websocket_client_test.py           # Run all automated tests")
            print("  python websocket_client_test.py -i [/]    # Interactive mode for echo route")
            print("  python websocket_client_test.py -i /chat  # Interactive mode for chat route")
            return

    # Run automated tests
    print("=" * 60)
    print("WebSocket Client Test Suite")
    print("=" * 60)

    await test_echo_route()
    await asyncio.sleep(0.5)  # Small delay between tests

    await test_chat_route()

    print("\n" + "=" * 60)
    print("All tests completed!")
    print("=" * 60)
    print("\nTip: Run with -i flag for interactive mode:")
    print("  python websocket_client_test.py -i      # Echo server")
    print("  python websocket_client_test.py -i /chat # Chat server")


if __name__ == "__main__":
    asyncio.run(main())
