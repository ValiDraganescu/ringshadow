// stb_image_write (public domain, ThirdParty/) built once, its warnings off,
// behind one function.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "../ThirdParty/stb_image_write.h"
#pragma clang diagnostic pop

int acWritePng(const char* path, int w, int h, const unsigned char* rgb) {
    return stbi_write_png(path, w, h, 3, rgb, w * 3);
}
