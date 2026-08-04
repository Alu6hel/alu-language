#include <metal_stdlib>
using namespace metal;

kernel void compute_main(texture2d<float, access::read> imgInput [[texture(0)]],
                         texture2d<float, access::write> imgOutput [[texture(1)]],
                         uint2 pos [[thread_position_in_grid]]) {
    if (pos.x >= imgInput.get_width() || pos.y >= imgInput.get_height()) return;
    float4 pixel = imgInput.read(pos);
    // Auto-translated ALU transformations go here
    imgOutput.write(pixel, pos);
}
