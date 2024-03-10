import filecmp
import subprocess
import time

# Step 1: Run TCP_Sender and TCP_Receiver concurrently
sender_command = "./TCP_Sender -ip 127.0.0.1 -p 6002 -algo reno"
receiver_command = "./TCP_Receiver -p 6002 -algo reno"

# Start sender and receiver as separate processes

receiver_process = subprocess.Popen(receiver_command, shell=True)
time.sleep(2)  # Adjust as needed
sender_process = subprocess.Popen(sender_command, shell=True)

# Optional: Add a delay to allow sender and receiver to run
time.sleep(10)  # Adjust as needed

# Step 2: Compare files
if filecmp.cmp("numbers.txt", "received.txt"):
    print("Comparison successful. The files are the same.")
else:
    print("Comparison failed. The files are different.")

# Optional: Wait for sender and receiver to complete
sender_process.wait()
receiver_process.wait()