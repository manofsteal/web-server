#!/usr/bin/env python3
"""
Simple WebSocket Echo Server for testing
"""
import asyncio
import websockets
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

async def echo_handler(websocket, path):
    """Handle WebSocket connections and echo messages back"""
    client_addr = websocket.remote_address
    logger.info(f"Client connected from {client_addr}")
    
    try:
        async for message in websocket:
            logger.info(f"Received from {client_addr}: {message}")
            
            # Echo the message back
            await websocket.send(f"Echo: {message}")
            logger.info(f"Sent echo to {client_addr}: Echo: {message}")
            
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client {client_addr} disconnected")
    except Exception as e:
        logger.error(f"Error handling client {client_addr}: {e}")

async def main():
    """Start the WebSocket server"""
    host = "localhost"
    port = 8765
    
    logger.info(f"Starting WebSocket echo server on {host}:{port}")
    
    async with websockets.serve(echo_handler, host, port):
        logger.info(f"WebSocket server running on ws://{host}:{port}")
        logger.info("Waiting for connections...")
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")