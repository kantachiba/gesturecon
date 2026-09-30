/*
 * ring_buffer.c
 */
#include "ring_buffer.h"

void rb_init(RingBuf *rb) {
    rb->head  = 0;
    rb->total = 0;
}

void rb_write(RingBuf *rb, float x, float y, float z) {
    rb->x[rb->head] = x;
    rb->y[rb->head] = y;
    rb->z[rb->head] = z;
    rb->head = (rb->head + 1) % GC_RING_SIZE;
    rb->total++;
}

/* WINDOW_SIZE 以上蓄積後、STRIDE サンプルごとに 1 を返す */
int rb_ready(const RingBuf *rb) {
    if (rb->total < GC_WINDOW_SIZE) return 0;
    return ((rb->total - GC_WINDOW_SIZE) % GC_WINDOW_STRIDE == 0);
}

/* 最新 GC_WINDOW_SIZE サンプルを古い順に out[] へコピー */
void rb_copy_window(const RingBuf *rb, float *out) {
    int start = (rb->head - GC_WINDOW_SIZE + GC_RING_SIZE) % GC_RING_SIZE;
    for (int i = 0; i < GC_WINDOW_SIZE; i++) {
        int idx = (start + i) % GC_RING_SIZE;
        out[i * 3 + 0] = rb->x[idx];
        out[i * 3 + 1] = rb->y[idx];
        out[i * 3 + 2] = rb->z[idx];
    }
}
