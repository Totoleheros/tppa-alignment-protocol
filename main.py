import serial
import time

def send_command(port, command):
    with serial.Serial(port, 9600, timeout=2) as ser:
        ser.write((command + "\r").encode())
        time.sleep(0.2)
        response = ser.read_all().decode().strip()
        print("Response:", response)
        return response

if __name__ == "__main__":
    # Exemple : envoyer une commande à un système connecté sur COM3
    port = "COM3"
    command = "AZ+0.5"
    send_command(port, command)
