#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cassert>
#include "Timer.h"
#include <pthread.h>

extern "C"
{
#include "ppmb_io.h"
}

int g_thread_num; // global thread count

struct img
{
    int xsize;
    int ysize;
    int maxrgb;
    unsigned char *r;
    unsigned char *g;
    unsigned char *b;
};

void print_histogram(FILE *f, int *hist, int N)
{
    fprintf(f, "%d\n", N + 1);
    for (int i = 0; i <= N; i++)
    {
        fprintf(f, "%d %d\n", i, hist[i]);
    }
}

struct ThreadArg
{
    img *input;
    int *local_r; // 记录该线程内的数据
    int *local_g;
    int *local_b;
    int start, end; // 起始索引和终止索引
};

// 单个线程计算直方图
void *thread_histo(void *arg)
{
    ThreadArg *t = (ThreadArg *)arg;
    for (int i = t->start; i < t->end; ++i)
    {
        t->local_r[t->input->r[i]]++;
        t->local_g[t->input->g[i]]++;
        t->local_b[t->input->b[i]]++;
    }

    return nullptr;
}

void histogram(struct img *input, int *hist_r, int *hist_g, int *hist_b)
{
    // we assume hist_r, hist_g, hist_b are zeroed on entry.
    int length = input->xsize * input->ysize; // 总长度
    int each_len = length / g_thread_num;     // 每个线程的长度
    int maxrgb = input->maxrgb;               // rgb最大值

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

    pthread_t *threads = new pthread_t[g_thread_num];
    ThreadArg *args = new ThreadArg[g_thread_num];

    // 设置每个线程的参数
    for (int i = 0; i < g_thread_num; ++i)
    {
        args[i].input = input;
        args[i].start = i * each_len;
        args[i].end = (i == g_thread_num) ? length : (i + 1) * each_len;
        args[i].local_r = (int *)calloc(maxrgb + 1, sizeof(int));
        args[i].local_g = (int *)calloc(maxrgb + 1, sizeof(int));
        args[i].local_b = (int *)calloc(maxrgb + 1, sizeof(int));
    }

    for (int i = 0; i < g_thread_num; ++i)
    {
        pthread_create(&threads[i], nullptr, thread_histo, &args[i]);
    }

    // 合并各个线程的直方图
    for (int i = 0; i < g_thread_num; ++i)
    {
        pthread_join(threads[i], nullptr);
        for (int j = 0; j <= input->maxrgb; ++j)
        {
            hist_r[j] += args[i].local_r[j];
            hist_g[j] += args[i].local_g[j];
            hist_b[j] += args[i].local_b[j];
        }

        // 释放分配的空间
        free(args[i].local_r);
        free(args[i].local_g);
        free(args[i].local_b);
    }

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

    g_thread_num = threads; // set g_thread_num

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
