#include <iostream>
#include <vector>

int run_sieve() {
    int limit = 1000000;
    std::vector<bool> is_prime(limit, true);
    is_prime[0] = false;
    is_prime[1] = false;

    for (int p = 2; p * p <= limit; p++) {
        if (is_prime[p]) {
            for (int i = p * p; i < limit; i += p) {
                is_prime[i] = false;
            }
        }
    }

    int primes_count = 0;
    for (int j = 0; j < limit; j++) {
        if (is_prime[j]) primes_count++;
    }
    return primes_count;
}

int main() {
    int passes = 10;
    int count = 0;
    for (int i = 0; i < passes; i++) {
        count = run_sieve();
    }
    std::cout << "Sieve passes: " << passes << ", Primes found: " << count << std::endl;
    return 0;
}
