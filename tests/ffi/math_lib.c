#include "math_lib.h"
#include <stdio.h>

int add_numbers(int a, int b) {
    return a + b;
}

double multiply_floats(double a, double b) {
    return a * b;
}

int compute_point_sum(Point2D p) {
    return p.x + p.y;
}

MathStatus safe_divide(int a, int b, int* outResult) {
    if (b == 0) {
        return MATH_ERR_DIV_ZERO;
    }
    if (outResult) {
        *outResult = a / b;
    }
    return MATH_OK;
}

void reset_sensor(Sensor* s, int new_id, double new_reading) {
    if (s) {
        s->id = new_id;
        s->reading = new_reading;
    }
}
