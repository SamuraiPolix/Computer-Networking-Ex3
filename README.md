# Socket Programming Project in C

### - Implementing a simple Reliable UDP (RUDP)
  - Designing packets (header and data)
  - Splitting large data into chunks
  - Adding reliability with a handshake to start connection, ACK packets, and checksum
  - Optimizing performance with high packet loss by adjusting timeout delays and max retries to resend packet
  - Simple API
### - Transferring large files over TCP or RUDP
### - Simulating packet loss in order to:
  - Compare TCP Reno and TCP Cubic
  - Compare TCP and RUDP

## Example runs:

### Part A - Tranferring a large file over TCP:

Choosing between TCP Reno and TCP Cubic

https://github.com/SamuraiPolix/Computer-Networking-Ex3/assets/52662032/29fef3bd-6532-42cb-a6af-4aaa16145aa9

https://github.com/SamuraiPolix/Computer-Networking-Ex3/assets/52662032/4a9f221e-1a50-47e6-a55f-6ed602c00164

### Part B - Tranferring a large file over our implemented RUDP:

*Will upload later


### Part C - Research

*Will upload later

___
Tested on Ubuntu 22.04.3 LTS. Verified memory leak-free by Valgrind.

![Memory leak-free](https://github.com/SamuraiPolix/Computer-Networking-Ex3/assets/52662032/4014563f-15ca-475d-86fe-ab9a40141b24)
