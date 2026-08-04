#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <grpcpp/grpcpp.h>
#include "alu_image.grpc.pb.h"

extern "C" {
    char* image_load(char* filename, int* w_ptr, int* h_ptr, int* c_ptr, int req_comp);
    int image_save_jpg(char* filename, int w, int h, int comp, char* data, int quality);
    void image_grayscale(char* data, int w, int h, int c);
    void image_free(char* data);
}

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using alu::grpc::AluImageService;
using alu::grpc::ProcessImageRequest;
using alu::grpc::ProcessImageResponse;

// --- Observability Globals ---
std::atomic<uint64_t> metrics_total_requests{0};
std::atomic<uint64_t> metrics_failed_requests{0};
std::mutex metrics_mutex;
double metrics_latency_sum_ms = 0.0;

// Structured JSON Logging Macro
#define JSON_LOG(level, event, msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::ostringstream oss; \
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ"); \
        std::cout << "{\"timestamp\":\"" << oss.str() << "\", \"level\":\"" << level << "\", \"event\":\"" << event << "\", \"message\":\"" << msg << "\"}\n"; \
    } while(0)

// Temporary file helper for the stb_image backend
std::string write_temp_file(const std::string& data, const std::string& ext) {
    auto path = std::filesystem::temp_directory_path() / ("temp_img_" + std::to_string(std::rand()) + ext);
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), data.size());
    return path.string();
}

std::string read_temp_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    auto size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::string buffer(size, '\0');
    in.read(&buffer[0], size);
    return buffer;
}

class AluImageServiceImpl final : public AluImageService::Service {
    Status ProcessImage(ServerContext* context, const ProcessImageRequest* request,
                        ProcessImageResponse* response) override {
        
        auto start_time = std::chrono::high_resolution_clock::now();
        metrics_total_requests++;
        
        // --- 1. JWT Authentication (Application Layer Security) ---
        auto auth_header = context->client_metadata().find("authorization");
        if (auth_header == context->client_metadata().end()) {
            JSON_LOG("WARN", "auth_failed", "Missing authorization header");
            metrics_failed_requests++;
            return Status(grpc::StatusCode::UNAUTHENTICATED, "Missing authorization header");
        }
        
        std::string token(auth_header->second.data(), auth_header->second.length());
        if (token.find("Bearer ") != 0 || token.length() < 20) { // Simple mock JWT validation
            JSON_LOG("WARN", "auth_failed", "Invalid JWT Token");
            metrics_failed_requests++;
            return Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid JWT Token");
        }
        
        JSON_LOG("INFO", "auth_success", "JWT Authenticated Successfully");
        JSON_LOG("INFO", "grpc_request", "Received image processing request (" + std::to_string(request->image_data().size()) + " bytes)");
        
        std::string in_path = write_temp_file(request->image_data(), ".jpg");
        std::string out_path = in_path + "_out.jpg";

        int w, h, c;
        char* img_data = image_load(const_cast<char*>(in_path.c_str()), &w, &h, &c, 0);

        if (!img_data) {
            JSON_LOG("ERROR", "decode_failed", "Native Alu Engine failed to decode image");
            response->set_success(false);
            response->set_error_message("Native Alu Engine failed to decode image");
            std::filesystem::remove(in_path);
            metrics_failed_requests++;
            return Status::OK;
        }

        if (request->grayscale()) {
            image_grayscale(img_data, w, h, c);
        }

        int quality = request->jpeg_quality() > 0 ? request->jpeg_quality() : 90;
        int res = image_save_jpg(const_cast<char*>(out_path.c_str()), w, h, c, img_data, quality);
        
        image_free(img_data);
        
        if (res == 0) {
            JSON_LOG("ERROR", "encode_failed", "Native Alu Engine failed to encode image");
            response->set_success(false);
            response->set_error_message("Native Alu Engine failed to encode image");
            metrics_failed_requests++;
        } else {
            JSON_LOG("INFO", "process_success", "Image processed and encoded successfully");
            response->set_success(true);
            response->set_processed_image(read_temp_file(out_path));
        }

        std::filesystem::remove(in_path);
        std::filesystem::remove(out_path);

        auto end_time = std::chrono::high_resolution_clock::now();
        double latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        {
            std::lock_guard<std::mutex> lock(metrics_mutex);
            metrics_latency_sum_ms += latency_ms;
        }
        
        JSON_LOG("INFO", "request_complete", "Latency: " + std::to_string(latency_ms) + " ms");

        return Status::OK;
    }
};

std::string read_file(const std::string& filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (!in) {
        JSON_LOG("WARN", "file_read_error", "Could not open " + filename + ". Server may crash if mTLS is enforced.");
        return "";
    }
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return contents;
}

// --- OpenTelemetry / Prometheus Exporter Thread ---
void PrometheusExporterThread() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
#else
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        JSON_LOG("ERROR", "prometheus_bind_error", "Failed to bind metrics port 8080");
        return;
    }
    
    listen(server_fd, 3);
    JSON_LOG("INFO", "prometheus_ready", "Prometheus Metrics Exporter listening on :8080/metrics");
    
    while (true) {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) continue;
#else
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) continue;
#endif
        
        char buffer[1024] = {0};
#ifdef _WIN32
        recv(client_socket, buffer, 1024, 0);
#else
        read(client_socket, buffer, 1024);
#endif
        
        // Only respond to GET /metrics
        if (std::string(buffer).find("GET /metrics") != std::string::npos) {
            uint64_t total = metrics_total_requests.load();
            uint64_t failed = metrics_failed_requests.load();
            double latency = 0.0;
            {
                std::lock_guard<std::mutex> lock(metrics_mutex);
                latency = metrics_latency_sum_ms;
            }
            
            std::ostringstream body;
            body << "# HELP alu_requests_total Total number of image requests processed\n";
            body << "# TYPE alu_requests_total counter\n";
            body << "alu_requests_total " << total << "\n\n";
            
            body << "# HELP alu_requests_failed Total number of failed requests\n";
            body << "# TYPE alu_requests_failed counter\n";
            body << "alu_requests_failed " << failed << "\n\n";
            
            body << "# HELP alu_processing_duration_ms_sum Total latency of Alu native engine\n";
            body << "# TYPE alu_processing_duration_ms_sum counter\n";
            body << "alu_processing_duration_ms_sum " << latency << "\n";
            
            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\nContent-Length: " 
                                 + std::to_string(body.str().length()) + "\r\n\r\n" + body.str();
            
#ifdef _WIN32
            send(client_socket, response.c_str(), response.length(), 0);
            closesocket(client_socket);
#else
            send(client_socket, response.c_str(), response.length(), 0);
            close(client_socket);
#endif
        } else {
#ifdef _WIN32
            closesocket(client_socket);
#else
            close(client_socket);
#endif
        }
    }
}

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    AluImageServiceImpl service;

    // Start Prometheus Exporter Thread
    std::thread exporter_thread(PrometheusExporterThread);
    exporter_thread.detach();

    // --- 2. mTLS (Transport Layer Security) ---
    std::string ca_cert = read_file("ca_cert.pem");
    std::string server_key = read_file("server_key.pem");
    std::string server_cert = read_file("server_cert.pem");

    grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {server_key, server_cert};
    grpc::SslServerCredentialsOptions ssl_opts;
    
    ssl_opts.pem_root_certs = ca_cert;
    ssl_opts.pem_key_cert_pairs.push_back(pkcp);
    
    // Mandate that clients provide a valid certificate signed by our CA
    ssl_opts.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;
    
    auto creds = grpc::SslServerCredentials(ssl_opts);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    JSON_LOG("INFO", "server_start", "mTLS Enterprise Server listening on " + server_address);
    if (server) {
        server->Wait();
    }
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
