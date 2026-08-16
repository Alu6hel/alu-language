#ifndef MATH_LIB_H
#define MATH_LIB_H

#define MATH_PI 3
#define MAX_BUFFER_SIZE 1024
#define OPTION_FLAGS 255

typedef enum {
    MATH_OK = 0,
    MATH_ERR_DIV_ZERO = 1,
    MATH_ERR_OVERFLOW = 2
} MathStatus;

typedef struct Point2D {
    int x;
    int y;
} Point2D;

typedef struct Sensor {
    int id;
    double reading;
} Sensor;

#ifdef __cplusplus
extern "C" {
#endif

int add_numbers(int a, int b);
double multiply_floats(double a, double b);
int compute_point_sum(Point2D p);
MathStatus safe_divide(int a, int b, int* outResult);
void reset_sensor(Sensor* s, int new_id, double new_reading);

#ifdef __cplusplus
}
#endif

#endif // MATH_LIB_H
