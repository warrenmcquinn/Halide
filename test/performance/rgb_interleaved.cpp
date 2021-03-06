#include <Halide.h>
#include <stdio.h>
#include "clock.h"
#include <memory>

using namespace Halide;

int main(int argc, char **argv) {
    ImageParam src(UInt(8), 3);
    Func dst;
    Var x, y, c;

    dst(x, y, c) = src(x, y, c);

    src.set_stride(0, 3);
    src.set_stride(2, 1);
    src.set_extent(2, 3);

    // This is the default format for Halide, but made explicit for illustration.
    dst.output_buffer().set_stride(0, 1);
    dst.output_buffer().set_extent(2, 3);

    dst.reorder(c, x, y).unroll(c);
    dst.vectorize(x, 16);

    // Run test many times to avoid timing jitter
    const int iterations = 20;

    // Allocate two 16 megapixel, 3 channel, 8-bit images -- input and output
    const int32_t buffer_side_length = (1 << 12);
    const int32_t buffer_size = buffer_side_length * buffer_side_length;

    uint8_t *src_storage(new uint8_t[buffer_size * 3]);
    uint8_t *dst_storage(new uint8_t[buffer_size * 3]);

    buffer_t src_buffer;
    buffer_t dst_buffer;

    // Setup src to be RGB interleaved, with no extra padding between channels or rows.
    memset(&src_buffer, 0, sizeof(src_buffer));
    src_buffer.host = src_storage;
    src_buffer.extent[0] = buffer_side_length;
    src_buffer.stride[0] = 3;
    src_buffer.extent[1] = buffer_side_length;
    src_buffer.stride[1] = src_buffer.stride[0] * src_buffer.extent[0];
    src_buffer.extent[2] = 3;
    src_buffer.stride[2] = 1;
    src_buffer.elem_size = 1;

    // Setup dst to be planar, with no extra padding between channels or rows.
    memset(&dst_buffer, 0, sizeof(dst_buffer));
    dst_buffer.host = dst_storage;
    dst_buffer.extent[0] = buffer_side_length;
    dst_buffer.stride[0] = 1;
    dst_buffer.extent[1] = buffer_side_length;
    dst_buffer.stride[1] = dst_buffer.stride[0] * dst_buffer.extent[0];
    dst_buffer.extent[2] = 3;
    dst_buffer.stride[2] = dst_buffer.stride[1] * dst_buffer.extent[1];
    dst_buffer.elem_size = 1;

    Image<uint8_t> src_image(&src_buffer, "src_image");
    Image<uint8_t> dst_image(&dst_buffer, "dst_image");

    for (int32_t x = 0; x < buffer_side_length; x++) {
        for (int32_t y = 0; y < buffer_side_length; y++) {
	  src_image(x, y, 0) = 0;
          src_image(x, y, 1) = 128;
          src_image(x, y, 2) = 255;
        }
    }
    memset(dst_storage, 0, buffer_size);

    src.set(src_image);

    dst.compile_jit();

    // Warm up caches, etc.
    dst.realize(dst_image);

    double t1 = currentTime();

    for (int i = 0; i < iterations; i++)
        dst.realize(dst_image);

    double t2 = currentTime();

    printf("Interleaved to planar bandwidth %.3e byte/s.\n", (buffer_size / (t2 - t1)) * 1000 * iterations);

    for (int32_t x = 0; x < buffer_side_length; x++) {
        for (int32_t y = 0; y < buffer_side_length; y++) {
            assert(dst_image(x, y, 0) == 0);
            assert(dst_image(x, y, 1) == 128);
            assert(dst_image(x, y, 2) == 255);
        }
    }

    // Setup a semi-planar output case.
    memset(&dst_buffer, 0, sizeof(dst_buffer));
    dst_buffer.host = dst_storage;
    dst_buffer.extent[0] = buffer_side_length;
    dst_buffer.stride[0] = 1;
    dst_buffer.extent[1] = buffer_side_length;
    dst_buffer.stride[1] = dst_buffer.stride[0] * dst_buffer.extent[0] * 3;
    dst_buffer.extent[2] = 3;
    dst_buffer.stride[2] = dst_buffer.extent[0];
    dst_buffer.elem_size = 1;

    memset(dst_storage, 0, buffer_size);

    double t3 = currentTime();

    for (int i = 0; i < iterations; i++)
        dst.realize(dst_image);

    double t4 = currentTime();

    for (int32_t x = 0; x < buffer_side_length; x++) {
        for (int32_t y = 0; y < buffer_side_length; y++) {
            assert(dst_image(x, y, 0) == 0);
            assert(dst_image(x, y, 1) == 128);
            assert(dst_image(x, y, 2) == 255);
        }
    }

    printf("Interleaved to semi-planar bandwidth %.3e byte/s.\n", (buffer_size / (t4 - t3)) * 1000 * iterations);

    delete[] src_storage;
    delete[] dst_storage;

    printf("Success!\n");
    return 0;
}
