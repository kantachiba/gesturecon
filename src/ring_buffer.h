/*
 * ring_buffer.h
 * 加速度センサーデータ用リングバッファ
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <gesture_con.h>

typedef struct {
    float x[GC_RING_SIZE];
    float y[GC_RING_SIZE];
    float z[GC_RING_SIZE];
    int   head;    /* 次に書き込むインデックス */
    int   total;   /* 書き込んだサンプル総数 */
} RingBuf;

void rb_init(RingBuf *rb);
void rb_write(RingBuf *rb, float x, float y, float z);
int  rb_ready(const RingBuf *rb);
/* 最新 GC_WINDOW_SIZE サンプルを [ax0,ay0,az0, ax1,ay1,az1, ...] へコピー */
void rb_copy_window(const RingBuf *rb, float *out);

#endif /* RING_BUFFER_H */
