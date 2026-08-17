#include <iostream>
#include <vector>

void matmul(int n, const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& c) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int k = 0; k < n; k++) {
                sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

int main() {
    int n = 512;
    std::vector<float> a(n * n, 1.0f);
    std::vector<float> b(n * n, 2.0f);
    std::vector<float> c(n * n, 0.0f);
    
    matmul(n, a, b, c);
    
    std::cout << "Matmul result[0] = " << c[0] << std::endl;
    return 0;
}
