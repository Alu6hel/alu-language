fn run_sieve() -> i32 {
    let limit = 1000000;
    let mut is_prime = vec![true; limit];
    is_prime[0] = false;
    is_prime[1] = false;

    let mut p = 2;
    while p * p <= limit {
        if is_prime[p] {
            let mut i = p * p;
            while i < limit {
                is_prime[i] = false;
                i += p;
            }
        }
        p += 1;
    }

    let mut primes_count = 0;
    for j in 0..limit {
        if is_prime[j] {
            primes_count += 1;
        }
    }
    primes_count
}

fn main() {
    let passes = 10;
    let mut count = 0;
    for _ in 0..passes {
        count = run_sieve();
    }
    println!("Sieve passes: {}, Primes found: {}", passes, count);
}
