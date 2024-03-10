with open("numbers.txt", "w") as file:
    target_size_MB = 2
    max_digits = 10  # Maximum number of digits in an integer
    numbers_to_write = (target_size_MB * 1024 * 1024) // (max_digits + 1)  # Considering a space after each number

    for i in range(1, numbers_to_write + 1):
        file.write(f"{i:>{max_digits}} ")

print("File 'numbers.txt' created successfully.")
