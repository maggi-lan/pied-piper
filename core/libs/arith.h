// arith.h -- minimal adaptive arithmetic coding interface
#ifndef ARITH_H
#define ARITH_H

#include <stddef.h>

size_t arithmetic_encode(const unsigned char* input, size_t input_len,
                         unsigned char* output, size_t output_capacity);

size_t arithmetic_decode(const unsigned char* input, size_t input_len,
                         unsigned char* output, size_t output_capacity);

#endif // ARITH_H
