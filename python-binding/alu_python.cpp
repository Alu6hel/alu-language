#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <string>
#include <stdexcept>
#include <iostream>

namespace py = pybind11;

// Forward declare the C functions from std/image_backend.cpp
extern "C" {
    char* image_load(char* filename, int* w_ptr, int* h_ptr, int* c_ptr, int req_comp);
    int image_save_jpg(char* filename, int w, int h, int comp, char* data, int quality);
    void image_grayscale(char* data, int w, int h, int c);
    void image_free(char* data);
}

// Python wrapper class for Alu Image
class AluImage {
private:
    char* data;
    int width, height, channels;

public:
    AluImage(const std::string& filename) {
        data = image_load(const_cast<char*>(filename.c_str()), &width, &height, &channels, 0);
        if (!data) {
            throw std::runtime_error("Failed to load image");
        }
    }

    ~AluImage() {
        if (data) {
            image_free(data);
        }
    }

    void grayscale() {
        image_grayscale(data, width, height, channels);
    }

    bool save_jpg(const std::string& filename, int quality) {
        int result = image_save_jpg(const_cast<char*>(filename.c_str()), width, height, channels, data, quality);
        return result != 0;
    }

    // Expose memory as numpy array for OpenCV/Pillow interoperability
    py::array_t<uint8_t> as_numpy() {
        return py::array_t<uint8_t>(
            {height, width, channels},
            {width * channels, channels, 1},
            reinterpret_cast<uint8_t*>(data)
        );
    }
};

PYBIND11_MODULE(alu_python, m) {
    m.doc() = "Alu Image Processing Engine C++ Bindings";

    py::class_<AluImage>(m, "AluImage")
        .def(py::init<const std::string &>())
        .def("grayscale", &AluImage::grayscale)
        .def("save_jpg", &AluImage::save_jpg, py::arg("filename"), py::arg("quality") = 90)
        .def("as_numpy", &AluImage::as_numpy);
}
