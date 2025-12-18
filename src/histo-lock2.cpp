// 本文件实现的是基于Ticket lock的自旋锁

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cassert>
#include <pthread.h>
#include "Timer.h"

extern "C"
{
#include "ppmb_io.h"
}

struct img
{
    int xsize;
    int ysize;
    int maxrgb;
    unsigned char *r;
    unsigned char *g;
    unsigned char *b;
};

int g_thread_num;

void print_histogram(FILE *f, int *hist, int N)
{
    fprintf(f, "%d\n", N + 1);
    for (int i = 0; i <= N; i++)
    {
        fprintf(f, "%d %d\n", i, hist[i]);
    }
}

// Ticket锁
struct Ticket_lock
{
    volatile int ticket; // 发出去的票
    volatile int turn;   // 目前的位置
};

// 初始化锁，一开始未被占用
void init_lock(Ticket_lock *lock)
{
    lock->ticket = lock->turn = 0;
}

// 获取锁
void acquire_lock(Ticket_lock *lock)
{
    int my_ticket = __sync_fetch_and_add(&lock->ticket, 1); // 获取票（原子的）
    while (lock->turn != my_ticket)
    {
    }
}

// 释放锁
void release_lock(Ticket_lock *lock)
{
    __sync_fetch_and_add(&lock->turn, 1); // 当前位置前进（原子的）
}

struct ThreadArg
{
    img *input;
    int *hist_r;
    int *hist_g;
    int *hist_b;
    int start, end;
    Ticket_lock *locks_r;
    Ticket_lock *locks_g;
    Ticket_lock *locks_b;
};

void *thread_histo(void *arg)
{
    ThreadArg *t = (ThreadArg *)arg;
    for (int i = t->start; i < t->end; ++i)
    {
        // 获取该位置的rgb值
        unsigned char r = t->input->r[i];
        unsigned char g = t->input->g[i];
        unsigned char b = t->input->b[i];

        acquire_lock(&t->locks_r[r]);
        ++t->hist_r[r];
        release_lock(&t->locks_r[r]);

        acquire_lock(&t->locks_g[g]);
        ++t->hist_g[g];
        release_lock(&t->locks_g[g]);

        acquire_lock(&t->locks_b[b]);
        ++t->hist_b[b];
        release_lock(&t->locks_b[b]);
    }

    return nullptr;
}

void histogram(struct img *input, int *hist_r, int *hist_g, int *hist_b)
{
    // we assume hist_r, hist_g, hist_b are zeroed on entry.
    int length = input->xsize * input->ysize;
    int each_len = length / g_thread_num;
    int maxrgb = input->maxrgb;

    if (g_thread_num == 1)
    {
        for (int pix = 0; pix < length; pix++)
        {
            hist_r[input->r[pix]] += 1;
            hist_g[input->g[pix]] += 1;
            hist_b[input->b[pix]] += 1;
        }

        return;
    }

    // 为rgb的每个取值分别设置自己的锁
    Ticket_lock *locks_r = (Ticket_lock *)malloc((maxrgb + 1) * sizeof(Ticket_lock));
    Ticket_lock *locks_g = (Ticket_lock *)malloc((maxrgb + 1) * sizeof(Ticket_lock));
    Ticket_lock *locks_b = (Ticket_lock *)malloc((maxrgb + 1) * sizeof(Ticket_lock));

    // 为所有锁初始化
    for (int i = 0; i <= maxrgb; ++i)
    {
        init_lock(&locks_r[i]);
        init_lock(&locks_g[i]);
        init_lock(&locks_b[i]);
    }

    pthread_t *threads = new pthread_t[g_thread_num];
    ThreadArg *args = new ThreadArg[g_thread_num];

    for (int i = 0; i < g_thread_num; ++i)
    {
        args[i].input = input;
        args[i].hist_r = hist_r;
        args[i].hist_g = hist_g;
        args[i].hist_b = hist_b;
        args[i].start = i * each_len;
        args[i].end = (i == g_thread_num - 1) ? length : (i + 1) * each_len;
        args[i].locks_r = locks_r;
        args[i].locks_g = locks_g;
        args[i].locks_b = locks_b;
    }

    for (int i = 0; i < g_thread_num; ++i)
    {
        pthread_create(&threads[i], nullptr, thread_histo, &args[i]);
    }

    for (int i = 0; i < g_thread_num; ++i)
    {
        pthread_join(threads[i], nullptr);
    }

    free(locks_r);
    free(locks_g);
    free(locks_b);

    delete[] threads;
    delete[] args;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s input-file output-file threads\n", argv[0]);
        printf("       For single-threaded runs, pass threads = 1\n");
        exit(1);
    }

    char *output_file = argv[2];
    char *input_file = argv[1];
    int threads = atoi(argv[3]);

    g_thread_num = threads;

    /* remove this in multithreaded version */
    // if(threads != 1) {
    //   printf("ERROR: Only supports single-threaded execution\n");
    //   exit(1);
    // }

    struct img input;

    if (!ppmb_read(input_file, &input.xsize, &input.ysize, &input.maxrgb,
                   &input.r, &input.g, &input.b))
    {
        if (input.maxrgb > 255)
        {
            printf("Maxrgb %d not supported\n", input.maxrgb);
            exit(1);
        }

        int *hist_r, *hist_g, *hist_b;

        hist_r = (int *)calloc(input.maxrgb + 1, sizeof(int));
        hist_g = (int *)calloc(input.maxrgb + 1, sizeof(int));
        hist_b = (int *)calloc(input.maxrgb + 1, sizeof(int));

        ggc::Timer t("histogram");

        t.start();
        histogram(&input, hist_r, hist_g, hist_b);
        t.stop();

        FILE *out = fopen(output_file, "w");
        if (out)
        {
            print_histogram(out, hist_r, input.maxrgb);
            print_histogram(out, hist_g, input.maxrgb);
            print_histogram(out, hist_b, input.maxrgb);
            fclose(out);
        }
        else
        {
            fprintf(stderr, "Unable to output!\n");
        }
        printf("Time: %llu ns\n", t.duration());
    }
}
