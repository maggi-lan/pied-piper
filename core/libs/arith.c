#include "arith.h"
#include <stdlib.h>
#include <stdio.h>

#define N_SYMBOLS 256
#define TOP_VALUE 0xFFFFFFFF

static unsigned long low, high;
static unsigned long underflow_bits;

static unsigned char *out_buf;
static size_t out_pos;
static size_t out_capacity;

static unsigned int cum_freq[N_SYMBOLS + 1];
static unsigned int freq[N_SYMBOLS];
static int total_freq;

static void model_init() {
    for (int i = 0; i < N_SYMBOLS; i++) {
        freq[i] = 1;
        cum_freq[i] = N_SYMBOLS - i;
    }
    cum_freq[N_SYMBOLS] = 0;
    total_freq = N_SYMBOLS;
}

static void update_model(int sym) {
    if (total_freq >= 1 << 15) {
        total_freq = 0;
        for (int i = 0; i < N_SYMBOLS; i++) {
            freq[i] = (freq[i] + 1) >> 1;
            total_freq += freq[i];
        }
        cum_freq[N_SYMBOLS] = 0;
        for (int i = N_SYMBOLS - 1; i >= 0; i--) {
            cum_freq[i] = cum_freq[i + 1] + freq[i];
        }
    }
    freq[sym]++;
    total_freq++;
    for (int i = sym - 1; i >= 0; i--) {
        cum_freq[i]++;
    }
}

static void output_bit(int bit) {
    static unsigned char buffer = 0;
    static int bits_to_go = 8;

    buffer >>= 1;
    if (bit)
        buffer |= 0x80;
    bits_to_go--;

    if (bits_to_go == 0) {
        if (out_pos < out_capacity) {
            out_buf[out_pos++] = buffer;
        }
        bits_to_go = 8;
        buffer = 0;
    }
}

static void flush_bits() {
    for (int i = 0; i < 8; i++) {
        output_bit(0);
    }
}

static void encode_symbol(int sym) {
    unsigned long range = (unsigned long) (high - low) + 1;
    high = low + (range * cum_freq[sym]) / total_freq - 1;
    low = low + (range * cum_freq[sym + 1]) / total_freq;

    for (;;) {
        if (high < 0x80000000) {
            output_bit(0);
            while (underflow_bits > 0) {
                output_bit(1);
                underflow_bits--;
            }
        }
        else if (low >= 0x80000000) {
            output_bit(1);
            while (underflow_bits > 0) {
                output_bit(0);
                underflow_bits--;
            }
            low -= 0x80000000;
            high -= 0x80000000;
        }
        else if (low >= 0x40000000 && high < 0xC0000000) {
            underflow_bits++;
            low -= 0x40000000;
            high -= 0x40000000;
        }
        else
            break;
        low <<= 1;
        high = (high << 1) + 1;
    }
}

size_t arithmetic_encode(const unsigned char* input, size_t input_len,
                         unsigned char* output, size_t output_capacity) {
    low = 0;
    high = TOP_VALUE;
    underflow_bits = 0;
    out_buf = output;
    out_pos = 0;
    out_capacity = output_capacity;

    model_init();

    for (size_t i = 0; i < input_len; i++) {
        encode_symbol(input[i]);
        update_model(input[i]);
    }

    underflow_bits++;
    if (low < 0x40000000) {
        output_bit(0);
        while (underflow_bits-- > 0) output_bit(1);
    } else {
        output_bit(1);
        while (underflow_bits-- > 0) output_bit(0);
    }
    flush_bits();

    return out_pos;
}

static unsigned long code_value;
static unsigned long low_decode;
static unsigned long high_decode;

static size_t in_pos;
static const unsigned char *in_buf;
static size_t in_len;

static int input_bit() {
    static unsigned char buffer = 0;
    static int bits_left = 0;
    if (bits_left == 0) {
        if (in_pos < in_len) buffer = in_buf[in_pos++];
        else buffer = 0xFF;
        bits_left = 8;
    }
    int t = buffer & 1;
    buffer >>= 1;
    bits_left--;
    return t;
}

static void start_decoder(const unsigned char* input, size_t input_len) {
    in_buf = input;
    in_len = input_len;
    in_pos = 0;
    low_decode = 0;
    high_decode = TOP_VALUE;
    code_value = 0;

    for (int i = 0; i < 32; i++) {
        code_value = (code_value << 1) | input_bit();
    }
}

static int decode_symbol() {
    unsigned long range = (unsigned long)(high_decode - low_decode) + 1;
    unsigned long cum = ((code_value - low_decode + 1) * total_freq - 1) / range;

    int sym;
    for (sym = 0; cum_freq[sym] > cum; sym++);

    high_decode = low_decode + (range * cum_freq[sym]) / total_freq - 1;
    low_decode = low_decode + (range * cum_freq[sym + 1]) / total_freq;

    for (;;) {
        if (high_decode < 0x80000000) {
        }
        else if (low_decode >= 0x80000000) {
            code_value -= 0x80000000;
            low_decode -= 0x80000000;
            high_decode -= 0x80000000;
        }
        else if (low_decode >= 0x40000000 && high_decode < 0xC0000000) {
            code_value -= 0x40000000;
            low_decode -= 0x40000000;
            high_decode -= 0x40000000;
        }
        else
            break;
        low_decode <<= 1;
        high_decode = (high_decode << 1) + 1;
        code_value = (code_value << 1) | input_bit();
    }
    update_model(sym);
    return sym;
}

size_t arithmetic_decode(const unsigned char* input, size_t input_len,
                         unsigned char* output, size_t output_capacity) {
    model_init();
    start_decoder(input, input_len);
    size_t out_pos = 0;

    while (out_pos < output_capacity) {
        int sym = decode_symbol();
        if (out_pos < output_capacity) output[out_pos++] = (unsigned char)sym;
        else break;
    }
    return out_pos;
}
