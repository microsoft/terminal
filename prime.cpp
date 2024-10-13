#include <iostream>

bool isPrime(int num) {
    if (num <= 1) {
        return false; // 0 and 1 are not prime
    }

    for (int i = 2; i * i <= num; i++) {
        if (num % i == 0) {
            return false; // Divisible by i, not prime
        }
    }

    return true; // Not divisible by any number from 2 to sqrt(num), prime
}

int main() {
    int number;

    std::cout << "Enter a number: ";
    std::cin >> number;

    if (isPrime(number)) {
        std::cout << number << " is a prime number." << std::endl;
    } else {
        std::cout << number << " is not a prime number." << std::endl;
    }

    return 0;
}
