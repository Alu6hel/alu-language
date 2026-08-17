fn matmul(n: usize, a: &[f32], b: &[f32], c: &mut [f32]) {
    for i in 0..n {
        for j in 0..n {
            let mut sum = 0.0;
            for k in 0..n {
                sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

fn main() {
    let n = 512;
    let a = vec![1.0; n * n];
    let b = vec![2.0; n * n];
    let mut c = vec![0.0; n * n];
    
    matmul(n, &a, &b, &mut c);
    
    println!("Matmul result[0] = {}", c[0]);
}
