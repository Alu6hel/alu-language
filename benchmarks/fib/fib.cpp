#include <iostream>

int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int result = fib(40);
    std::cout << "Fibonacci(40) = " << result << std::endl;
    return 0;
}
