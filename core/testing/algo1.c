#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../libs/stb_image_write.h"

typedef struct {
    unsigned char r, g, b;
} Pixel;

/* Chunk opcodes (each chunk starts with one opcode byte) */
#define OP_RLE   0x00  // [OP_RLE][run:1][R][G][B]
#define OP_RAW   0x01  // [OP_RAW][R][G][B]
#define OP_DIFF  0x02  // [OP_DIFF][packed_byte]
#define OP_LUMA  0x03  // [OP_LUMA][dg+32][(dr-dg+8)<<4 | (db-dg+8)]

/* utility: clamp 0..255 */
static inline unsigned char clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

/* convert stbi buffer (row-major contiguous) to Pixel array (RGB) */
Pixel* convert_pixels(unsigned char* data, int width, int height, int channels) {
    Pixel* pixels = malloc((size_t)width * height * sizeof(Pixel));
    if (!pixels) return NULL;
    for (int i = 0; i < width * height; ++i) {
        pixels[i].r = data[i * channels + 0];
        pixels[i].g = data[i * channels + 1];
        pixels[i].b = data[i * channels + 2];
    }
    return pixels;
}

/* ------------------ Chunk encoder but write to memory buffer ------------------
   Instead of writing chunks directly to a file, we'll write them into a dynamic
   byte buffer (vector-like). Then we can compute frequencies and do Huffman.
*/
typedef struct {
    unsigned char *buf;
    size_t size;
    size_t cap;
} ByteBuf;

static void bb_init(ByteBuf *b) {
    b->buf = NULL;
    b->size = 0;
    b->cap = 0;
}

static void bb_free(ByteBuf *b) {
    free(b->buf);
    b->buf = NULL;
    b->size = b->cap = 0;
}

static int bb_ensure(ByteBuf *b, size_t extra) {
    if (b->size + extra <= b->cap) return 1;
    size_t newcap = b->cap ? b->cap * 2 : 4096;
    while (newcap < b->size + extra) newcap *= 2;
    unsigned char *nb = realloc(b->buf, newcap);
    if (!nb) return 0;
    b->buf = nb;
    b->cap = newcap;
    return 1;
}

static int bb_push(ByteBuf *b, unsigned char c) {
    if (!bb_ensure(b, 1)) return 0;
    b->buf[b->size++] = c;
    return 1;
}

static int bb_write3(ByteBuf *b, unsigned char a, unsigned char c, unsigned char d) {
    if (!bb_ensure(b, 3)) return 0;
    b->buf[b->size++] = a;
    b->buf[b->size++] = c;
    b->buf[b->size++] = d;
    return 1;
}

static int bb_writeN(ByteBuf *b, const unsigned char *data, size_t n) {
    if (!bb_ensure(b, n)) return 0;
    memcpy(b->buf + b->size, data, n);
    b->size += n;
    return 1;
}

/* Build the chunk stream in memory */
void build_chunk_stream(Pixel* pixels, int total_pixels, ByteBuf *out_stream) {
    bb_init(out_stream);
    if (total_pixels <= 0) return;

    // first pixel as RAW
    Pixel prev = pixels[0];
    bb_push(out_stream, OP_RAW);
    bb_push(out_stream, prev.r);
    bb_push(out_stream, prev.g);
    bb_push(out_stream, prev.b);

    int run = 0; // counts repeats after first
    for (int i = 1; i < total_pixels; ++i) {
        Pixel cur = pixels[i];
        if (cur.r == prev.r && cur.g == prev.g && cur.b == prev.b) {
            run++;
            if (run == 255) {
                bb_push(out_stream, OP_RLE);
                bb_push(out_stream, (unsigned char)run);
                bb_push(out_stream, prev.r);
                bb_push(out_stream, prev.g);
                bb_push(out_stream, prev.b);
                run = 0;
            }
            continue;
        }
        if (run > 0) {
            bb_push(out_stream, OP_RLE);
            bb_push(out_stream, (unsigned char)run);
            bb_push(out_stream, prev.r);
            bb_push(out_stream, prev.g);
            bb_push(out_stream, prev.b);
            run = 0;
        }

        int dr = (int)cur.r - (int)prev.r;
        int dg = (int)cur.g - (int)prev.g;
        int db = (int)cur.b - (int)prev.b;

        if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
            unsigned char packed = (unsigned char)(((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2));
            bb_push(out_stream, OP_DIFF);
            bb_push(out_stream, packed);
        } else if (dg >= -32 && dg <= 31) {
            int dr_dg = dr - dg;
            int db_dg = db - dg;
            if (dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                bb_push(out_stream, OP_LUMA);
                bb_push(out_stream, (unsigned char)(dg + 32));
                unsigned char second = (unsigned char)(((dr_dg + 8) << 4) | ((db_dg + 8) & 0x0F));
                bb_push(out_stream, second);
            } else {
                bb_push(out_stream, OP_RAW);
                bb_push(out_stream, cur.r);
                bb_push(out_stream, cur.g);
                bb_push(out_stream, cur.b);
            }
        } else {
            bb_push(out_stream, OP_RAW);
            bb_push(out_stream, cur.r);
            bb_push(out_stream, cur.g);
            bb_push(out_stream, cur.b);
        }
        prev = cur;
    }

    if (run > 0) {
        bb_push(out_stream, OP_RLE);
        bb_push(out_stream, (unsigned char)run);
        bb_push(out_stream, prev.r);
        bb_push(out_stream, prev.g);
        bb_push(out_stream, prev.b);
        run = 0;
    }
}

/* ------------------ Huffman implementation (simple) ------------------ */
typedef struct HuffNode {
    uint32_t freq;
    int left;   // index in nodes array or -1
    int right;  // index or -1
    int symbol; // 0..255 for leaves, -1 for internal
} HuffNode;

/* Build Huffman tree from freq[256].
   Returns nodes array pointer and sets node_count and root index.
   Caller must free the returned array. */
HuffNode* build_huffman(const uint32_t freq[256], int *out_node_count, int *out_root) {
    // count symbols
    int symbol_count = 0;
    for (int i = 0; i < 256; ++i) if (freq[i] > 0) ++symbol_count;

    // special case: no symbols? return NULL
    if (symbol_count == 0) {
        *out_node_count = 0;
        *out_root = -1;
        return NULL;
    }

    // allocate worst-case nodes = 2*symbol_count - 1
    int max_nodes = 2 * symbol_count - 1;
    HuffNode *nodes = malloc(sizeof(HuffNode) * max_nodes);
    if (!nodes) return NULL;

    // initialize leaves
    int n = 0;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            nodes[n].freq = freq[i];
            nodes[n].left = -1;
            nodes[n].right = -1;
            nodes[n].symbol = i;
            ++n;
        }
    }

    // if only one leaf, create a dummy parent so we have a root with two children
    if (n == 1) {
        // create parent at index n
        nodes[n].freq = nodes[0].freq;
        nodes[n].left = 0;
        nodes[n].right = -1;
        nodes[n].symbol = -1;
        *out_node_count = n + 1;
        *out_root = n;
        return nodes;
    }

    int node_count = n;

    // Build tree by repeatedly merging two smallest-frequency nodes
    while (node_count < max_nodes) {
        // find two smallest among [0..node_count-1] that are not -1 marked (we keep them compact)
        // We'll select by scanning (n is small-ish)
        // find min1 and min2 indices
        int min1 = -1, min2 = -1;
        for (int i = 0; i < node_count; ++i) {
            if (nodes[i].freq == 0) continue; // shouldn't occur
            if (min1 == -1 || nodes[i].freq < nodes[min1].freq) {
                min2 = min1;
                min1 = i;
            } else if (min2 == -1 || nodes[i].freq < nodes[min2].freq) {
                min2 = i;
            }
        }
        if (min2 == -1) break; // should not happen

        // merge min1 and min2 into new node at index node_count
        nodes[node_count].freq = nodes[min1].freq + nodes[min2].freq;
        nodes[node_count].left = min1;
        nodes[node_count].right = min2;
        nodes[node_count].symbol = -1;

        // mark frequencies of min1/min2 as very large so they won't be picked again;
        // but easier: swap them out by moving last eligible to their positions to keep compactness
        // We'll replace min1 and min2 by the last entries before node_count
        // Move node at (node_count) will be appended, so to keep thing simple, we will set freq to UINT32_MAX
        nodes[min1].freq = UINT32_MAX;
        nodes[min2].freq = UINT32_MAX;

        node_count++;
        // continue until we've created node_count == 2*n -1
        // break when node_count == max_nodes
        if (node_count >= max_nodes) break;
    }

    // The above algorithm marked merged nodes' freq as UINT32_MAX — that's a bit hacky.
    // Simpler approach: rebuild using a selection algorithm: we'll build a fresh array of active indices each iteration.
    // To keep code correct and simpler, we'll re-implement building properly below and free the current nodes, then do correct build.

    free(nodes);

    // Proper build: maintain dynamic list of candidate nodes (struct with freq and index)
    // We'll allocate nodes again and fill leaves
    nodes = malloc(sizeof(HuffNode) * max_nodes);
    if (!nodes) return NULL;
    int idx = 0;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            nodes[idx].freq = freq[i];
            nodes[idx].left = -1;
            nodes[idx].right = -1;
            nodes[idx].symbol = i;
            idx++;
        }
    }
    int cur_nodes = idx; // number of filled nodes (leaves)
    int next_index = cur_nodes; // next free index to create internal nodes

    // helper to find two smallest indices among 0..next_index-1
    while (cur_nodes > 1) {
        // find smallest two in [0..next_index-1] among nodes with freq>0
        int s1 = -1, s2 = -1;
        for (int i = 0; i < next_index; ++i) {
            if (nodes[i].freq == 0) continue; // already taken
            if (s1 == -1 || nodes[i].freq < nodes[s1].freq) { s2 = s1; s1 = i; }
            else if (s2 == -1 || nodes[i].freq < nodes[s2].freq) { s2 = i; }
        }
        if (s2 == -1) break;

        // create parent at next_index
        nodes[next_index].freq = nodes[s1].freq + nodes[s2].freq;
        nodes[next_index].left = s1;
        nodes[next_index].right = s2;
        nodes[next_index].symbol = -1;

        // mark s1 and s2 as used by setting freq=0
        nodes[s1].freq = 0;
        nodes[s2].freq = 0;

        next_index++;
        cur_nodes--; // one fewer active nodes
    }

    int root = -1;
    // find final root (the one with non-zero freq or last created)
    for (int i = next_index - 1; i >= 0; --i) {
        if (nodes[i].freq > 0) { root = i; break; }
    }
    if (root == -1) root = next_index - 1;

    *out_node_count = next_index;
    *out_root = root;
    return nodes;
}

/* Generate code lengths and bit patterns by traversing tree */
void generate_codes_from_tree(HuffNode* nodes, int node_count, int root, uint32_t codes[256], uint8_t code_bits[256]) {
    // initialize
    for (int i = 0; i < 256; ++i) { codes[i] = 0; code_bits[i] = 0; }

    // recursive stack
    typedef struct { int node; uint32_t code; uint8_t len; } StackItem;
    StackItem *stack = malloc(sizeof(StackItem) * (node_count + 8));
    int sp = 0;
    if (root < 0) { free(stack); return; }

    stack[sp++] = (StackItem){ .node = root, .code = 0, .len = 0 };

    while (sp > 0) {
        StackItem it = stack[--sp];
        int n = it.node;
        if (nodes[n].left == -1 && nodes[n].right == -1) {
            // leaf
            if (nodes[n].symbol >= 0) {
                codes[(unsigned char)nodes[n].symbol] = it.code;
                code_bits[(unsigned char)nodes[n].symbol] = it.len ? it.len : 1; // at least 1 bit
            }
            continue;
        }
        // traverse right with bit 1, left with bit 0 — but push right first so left is processed later (LIFO)
        if (nodes[n].right != -1) {
            stack[sp++] = (StackItem){ .node = nodes[n].right, .code = (it.code << 1) | 1u, .len = (uint8_t)(it.len + 1) };
        }
        if (nodes[n].left != -1) {
            stack[sp++] = (StackItem){ .node = nodes[n].left, .code = (it.code << 1) | 0u, .len = (uint8_t)(it.len + 1) };
        }
    }
    free(stack);
}

/* Bit writer */
typedef struct {
    unsigned char cur;
    int bits_filled; // number of bits in 'cur' already (0..7), we fill from MSB side: left shift
    FILE *f;
} BitWriter;

void bw_init(BitWriter *bw, FILE *f) { bw->cur = 0; bw->bits_filled = 0; bw->f = f; }
void bw_write_bit(BitWriter *bw, int bit) {
    bw->cur = (unsigned char)((bw->cur << 1) | (bit & 1));
    bw->bits_filled++;
    if (bw->bits_filled == 8) {
        fputc(bw->cur, bw->f);
        bw->bits_filled = 0;
        bw->cur = 0;
    }
}
void bw_write_bits(BitWriter *bw, uint32_t bits, int len) {
    // write most-significant first: for i=len-1..0 write (bits >> i) & 1
    for (int i = len - 1; i >= 0; --i) bw_write_bit(bw, (bits >> i) & 1);
}
void bw_flush(BitWriter *bw) {
    if (bw->bits_filled > 0) {
        // pad lower bits with zeros (by shifting left)
        bw->cur = (unsigned char)(bw->cur << (8 - bw->bits_filled));
        fputc(bw->cur, bw->f);
        bw->bits_filled = 0;
        bw->cur = 0;
    }
}

/* Bit reader */
typedef struct {
    FILE *f;
    unsigned char cur;
    int bits_left; // bits remaining in cur
} BitReader;
void br_init(BitReader *br, FILE *f) { br->f = f; br->cur = 0; br->bits_left = 0; }
int br_read_bit(BitReader *br, int *out_bit) {
    if (br->bits_left == 0) {
        int c = fgetc(br->f);
        if (c == EOF) return 0;
        br->cur = (unsigned char)c;
        br->bits_left = 8;
    }
    // read MSB first
    int bit = (br->cur >> (br->bits_left - 1)) & 1;
    br->bits_left--;
    *out_bit = bit;
    return 1;
}

/* Huffman encode: input is chunk_stream (bytes, size), output file path.
   We'll write a simple header:
   - magic "PPHF" (4 bytes)
   - width (uint32), height (uint32), channels (uint32)
   - uncompressed_chunk_size (uint32)
   - freq table (256 * uint32, little endian)
   - then bitstream bytes (packed)
*/
/* NOTE: removed unused chunk_path parameter; chunk stream is passed directly */
void huffman_encode_and_write(const char *out_path,
                              int width, int height, int channels,
                              const ByteBuf *chunk_stream) {
    // compute freq
    uint32_t freq[256];
    memset(freq, 0, sizeof(freq));
    for (size_t i = 0; i < chunk_stream->size; ++i) freq[chunk_stream->buf[i]]++;

    // build tree
    int node_count = 0, root = -1;
    HuffNode *nodes = build_huffman(freq, &node_count, &root);
    if (!nodes && node_count==0) {
        fprintf(stderr, "Huffman: no symbols to encode\n");
        return;
    }

    uint32_t codes[256];
    uint8_t code_bits[256];
    generate_codes_from_tree(nodes, node_count, root, codes, code_bits);

    // open output file
    FILE *out = fopen(out_path, "wb");
    if (!out) { perror("open out"); free(nodes); return; }

    // write header magic
    fwrite("PPHF", 1, 4, out);
    // write width,height,channels, uncompressed chunk size (all uint32 little-endian)
    uint32_t u32;
    u32 = (uint32_t)width; fwrite(&u32, 4, 1, out);
    u32 = (uint32_t)height; fwrite(&u32, 4, 1, out);
    u32 = (uint32_t)channels; fwrite(&u32, 4, 1, out);
    u32 = (uint32_t)chunk_stream->size; fwrite(&u32, 4, 1, out);

    // write freq table (256 * uint32)
    for (int i = 0; i < 256; ++i) {
        uint32_t v = freq[i];
        fwrite(&v, 4, 1, out);
    }

    // Now write bitstream using the codes
    BitWriter bw;
    bw_init(&bw, out);

    for (size_t i = 0; i < chunk_stream->size; ++i) {
        unsigned char b = chunk_stream->buf[i];
        uint32_t code = codes[b];
        int len = code_bits[b];
        // when building codes we made codes with MSB-first orientation (as we pushed bits shifting left)
        // ensure len>0, else skip (shouldn't happen)
        if (len == 0) len = 1;
        bw_write_bits(&bw, code, len);
    }

    bw_flush(&bw);
    fclose(out);
    free(nodes);
    printf("Huffman: wrote compressed file %s (input chunk bytes %zu -> Huffman)\n", out_path, chunk_stream->size);
}

/* Huffman decode: read file, reconstruct freq table and decode bitstream into a byte buffer of size 'uncompressed_size' */
unsigned char* huffman_read_and_decode(const char *in_path, uint32_t *out_uncompressed_size,
                                       int *out_width, int *out_height, int *out_channels) {
    FILE *in = fopen(in_path, "rb");
    if (!in) { perror("open in"); return NULL; }

    // read header
    char magic[5] = {0};
    if (fread(magic, 1, 4, in) != 4) { fclose(in); return NULL; }
    if (strncmp(magic, "PPHF", 4) != 0) { fprintf(stderr, "Not a PPHF file\n"); fclose(in); return NULL; }

    uint32_t u32;
    if (fread(&u32, 4, 1, in) != 1) { fclose(in); return NULL; }
    *out_width = (int)u32;
    if (fread(&u32, 4, 1, in) != 1) { fclose(in); return NULL; }
    *out_height = (int)u32;
    if (fread(&u32, 4, 1, in) != 1) { fclose(in); return NULL; }
    *out_channels = (int)u32;
    if (fread(&u32, 4, 1, in) != 1) { fclose(in); return NULL; }
    uint32_t uncompressed_size = u32;
    *out_uncompressed_size = uncompressed_size;

    uint32_t freq[256];
    for (int i = 0; i < 256; ++i) {
        if (fread(&freq[i], 4, 1, in) != 1) { fclose(in); return NULL; }
    }

    // Build Huffman tree from freq
    int node_count = 0, root = -1;
    HuffNode *nodes = build_huffman(freq, &node_count, &root);
    if (!nodes) { // no data
        fclose(in);
        *out_uncompressed_size = 0;
        return NULL;
    }

    // Decode bitstream into buffer of bytes (uncompressed_size)
    unsigned char *out_buf = malloc(uncompressed_size ? uncompressed_size : 1);
    if (!out_buf) { free(nodes); fclose(in); return NULL; }

    BitReader br;
    br_init(&br, in);

    // To decode, we traverse tree using bits
    int count = 0;
    // handle single-symbol special case: if root is a leaf
    if (nodes[root].left == -1 && nodes[root].right == -1) {
        // repeat the same symbol freq[root] times but uncompressed_size tells us how many bytes expected
        unsigned char sym = (unsigned char)nodes[root].symbol;
        while (count < (int)uncompressed_size) out_buf[count++] = sym;
    } else {
        int node = root;
        while (count < (int)uncompressed_size) {
            int bit;
            if (!br_read_bit(&br, &bit)) break; // EOF or error
            if (bit == 0) node = nodes[node].left;
            else node = nodes[node].right;
            if (node == -1) { fprintf(stderr, "Huffman decode error: invalid node\n"); break; }
            if (nodes[node].left == -1 && nodes[node].right == -1) {
                // leaf
                out_buf[count++] = (unsigned char)nodes[node].symbol;
                node = root;
            }
        }
    }

    free(nodes);
    fclose(in);

    if (count != (int)uncompressed_size) {
        fprintf(stderr, "Warning: Huffman decoded size %d != expected %u\n", count, uncompressed_size);
        // still return the buffer with what we have
    }
    return out_buf;
}

/* ------------------ Chunk decoder from memory buffer ------------------ */
Pixel* decode_chunks_from_buffer(const unsigned char *buf, size_t buf_size, int total_pixels) {
    if (!buf) return NULL;
    Pixel* out_pixels = malloc((size_t)total_pixels * sizeof(Pixel));
    if (!out_pixels) return NULL;

    size_t pos = 0;
    int count = 0;
    Pixel prev = {0,0,0};

    while (pos < buf_size && count < total_pixels) {
        unsigned char code = buf[pos++];
        if (code == OP_RLE) {
            if (pos + 4 > buf_size) break;
            unsigned char run = buf[pos++];
            unsigned char r = buf[pos++];
            unsigned char g = buf[pos++];
            unsigned char b = buf[pos++];
            Pixel px = {r,g,b};
            for (int i = 0; i < run && count < total_pixels; ++i) out_pixels[count++] = px;
            prev = px;
        } else if (code == OP_RAW) {
            if (pos + 3 > buf_size) break;
            unsigned char r = buf[pos++];
            unsigned char g = buf[pos++];
            unsigned char b = buf[pos++];
            Pixel px = {r,g,b};
            out_pixels[count++] = px;
            prev = px;
        } else if (code == OP_DIFF) {
            if (pos + 1 > buf_size) break;
            unsigned char packed = buf[pos++];
            int dr = ((packed >> 4) & 0x03) - 2;
            int dg = ((packed >> 2) & 0x03) - 2;
            int db = (packed & 0x03) - 2;
            Pixel px = {
                clamp_u8((int)prev.r + dr),
                clamp_u8((int)prev.g + dg),
                clamp_u8((int)prev.b + db)
            };
            out_pixels[count++] = px;
            prev = px;
        } else if (code == OP_LUMA) {
            if (pos + 2 > buf_size) break;
            unsigned char dg_biased = buf[pos++];
            unsigned char second = buf[pos++];
            int dg = (int)dg_biased - 32;
            int dr_dg = ((second >> 4) & 0x0F) - 8;
            int db_dg = (second & 0x0F) - 8;
            Pixel px = {
                clamp_u8((int)prev.r + dg + dr_dg),
                clamp_u8((int)prev.g + dg),
                clamp_u8((int)prev.b + dg + db_dg)
            };
            out_pixels[count++] = px;
            prev = px;
        } else {
            fprintf(stderr, "Unknown chunk code 0x%02X in buffer at pos %zu\n", code, pos-1);
            break;
        }
    }

    if (count != total_pixels) {
        fprintf(stderr, "Warning: decoded pixel count %d != expected %d\n", count, total_pixels);
    } else {
        printf("Decoded %d pixels successfully (from buffer)\n", count);
    }
    return out_pixels;
}

/* ------------------ Main flow ------------------ */
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <input.bmp> <output.pp> <decoded.bmp>\n", argv[0]);
        return 1;
    }

    const char *inpath = argv[1];
    const char *out_compressed = argv[2];
    const char *out_decoded_bmp = argv[3];

    int width, height, channels;
    unsigned char *img = stbi_load(inpath, &width, &height, &channels, 0);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }
    if (channels < 3) {
        fprintf(stderr, "Image has fewer than 3 channels (%d)\n", channels);
        stbi_image_free(img);
        return 1;
    }

    printf("Loaded image: %dx%d channels=%d\n", width, height, channels);

    int total_pixels = width * height;
    Pixel* pixels = convert_pixels(img, width, height, channels);
    stbi_image_free(img);
    if (!pixels) { fprintf(stderr, "OOM\n"); return 1; }

    // Build chunk stream
    ByteBuf chunk_stream;
    build_chunk_stream(pixels, total_pixels, &chunk_stream);
    printf("Built chunk stream bytes = %zu\n", chunk_stream.size);

    // Compress using Huffman and write to final output (.pp)
    huffman_encode_and_write(out_compressed, width, height, channels, &chunk_stream);
    printf("Compression complete. Output saved to %s\n", out_compressed);

    // Decompression
    uint32_t uncompressed_size = 0;
    int rwidth=0, rheight=0, rchannels=0;
    unsigned char *decoded_chunk_buf = huffman_read_and_decode(out_compressed, &uncompressed_size, &rwidth, &rheight, &rchannels);
    if (!decoded_chunk_buf) {
        fprintf(stderr, "Huffman decode failed\n");
        bb_free(&chunk_stream);
        free(pixels);
        return 1;
    }

    Pixel *decoded_pixels = decode_chunks_from_buffer(decoded_chunk_buf, uncompressed_size, total_pixels);
    if (!decoded_pixels) {
        fprintf(stderr, "Chunk decode failed\n");
        free(decoded_chunk_buf);
        bb_free(&chunk_stream);
        free(pixels);
        return 1;
    }

    // Write decoded BMP
    unsigned char *outbuf = malloc((size_t)total_pixels * 3);
    for (int i = 0; i < total_pixels; ++i) {
        outbuf[i*3+0] = decoded_pixels[i].r;
        outbuf[i*3+1] = decoded_pixels[i].g;
        outbuf[i*3+2] = decoded_pixels[i].b;
    }
    stbi_write_bmp(out_decoded_bmp, width, height, 3, outbuf);
    printf("Decoded image written to %s\n", out_decoded_bmp);

    // cleanup
    free(outbuf);
    free(decoded_pixels);
    free(decoded_chunk_buf);
    bb_free(&chunk_stream);
    free(pixels);

    return 0;
}
