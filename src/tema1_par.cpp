// Author: APD team, except where source was noted
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "MyThread.hpp"
#include <vector>
#include <algorithm>
#include <utility>

using std::vector, std::pair, std::min, std::make_pair;

#define CONTOUR_CONFIG_COUNT 16
#define FILENAME_MAX_SIZE 50
#define STEP 8
#define SIGMA 200
#define RESCALE_X 2048
#define RESCALE_Y 2048

#define CLAMP(v, min, max) \
    if (v < min)           \
    {                      \
        v = min;           \
    }                      \
    else if (v > max)      \
    {                      \
        v = max;           \
    }

// Clasa pentru paralelizarea algoritmului Marching Square mai usor
class Marching_Square
{
private:
    ppm_image *image; // imaginea finala
    ppm_image *aux;   // imaginea citita din fisier
    int step_x;
    int step_y;
    int p, q;
    unsigned char **grid;
    ppm_image **contour_map;
    char *in_file, *out_file;
    vector<MyThread *> threads;           // vector de pointeri la thread-uri
    pthread_barrier_t barrier1, barrier2; // bariere pentru sincronizare

    // Functia care se executa in thread-uri
    static void *ThreadFunction(void *arg)
    {
        // structure binding pentru a putea accesa usor datele
        auto &[marching_square, thread_id] = *(pair<Marching_Square *, long> *)arg;

        // calculam intervalul de lucru pentru redimensionarea imaginii
        int start = thread_id * (double)RESCALE_Y / marching_square->threads.size();
        int end = min((thread_id + 1) * (double)RESCALE_Y / marching_square->threads.size(), (double)RESCALE_Y);

        if (!(marching_square->aux->x <= RESCALE_X && marching_square->aux->y <= RESCALE_Y))
        {
            marching_square->rescale_image(marching_square->aux, marching_square->image, start, end);
        }
        // asteptam ca toate thread-urile sa termine de redimensionat
        pthread_barrier_wait(&marching_square->barrier1);

        // calculam intervalul de lucru pentru sample_grid si march
        start = thread_id * (double)marching_square->q / marching_square->threads.size();
        end = min((thread_id + 1) * (double)marching_square->q / marching_square->threads.size(), (double)marching_square->p);

        marching_square->sample_grid(start, end);

        // asteptam ca toate thread-urile sa termine de sample_grid
        pthread_barrier_wait(&marching_square->barrier2);

        marching_square->march(start, end);

        return NULL;
    }

    // Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
    // that need to be set on the output image. An array is used for this map since the keys are
    // binary numbers in 0-15. Contour images are located in the './contours' directory.
    ppm_image **init_contour_map()
    {
        ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
        if (!map)
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }

        for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
        {
            char filename[FILENAME_MAX_SIZE];
            sprintf(filename, "./contours/%d.ppm", i);
            map[i] = read_ppm(filename);
        }

        return map;
    }

    // Updates a particular section of an image with the corresponding contour pixels.
    // Used to create the complete contour image.
    void update_image(ppm_image *image, ppm_image *contour, int x, int y)
    {
        for (int i = 0; i < contour->x; i++)
        {
            for (int j = 0; j < contour->y; j++)
            {
                int contour_pixel_index = contour->x * i + j;
                int image_pixel_index = (x + i) * image->y + y + j;

                image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
                image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
                image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
            }
        }
    }

    // Calls `free` method on the utilized resources.
    void free_resources(ppm_image *aux, ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x)
    {
        for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
        {
            free(contour_map[i]->data);
            free(contour_map[i]);
        }

        free(contour_map);

        for (int i = 0; i <= image->x / step_x; i++)
        {
            free(grid[i]);
        }

        free(grid);

        if (aux != image)
        {
            free(aux->data);
            free(aux);
        }

        free(image->data);
        free(image);
    }

    void init_grid()
    {
        grid = (unsigned char **)calloc((p + 1), sizeof(unsigned char *));
        if (!grid)
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }

        for (int i = 0; i <= p; i++)
        {
            grid[i] = (unsigned char *)calloc((q + 1), sizeof(unsigned char));
            if (!grid[i])
            {
                fprintf(stderr, "Unable to allocate memory\n");
                exit(1);
            }
        }
    }
    // Initializam o imagine de dimensiunea maxima
    ppm_image *init_MAXimage()
    {
        ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
        if (!new_image)
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
        new_image->x = RESCALE_X;
        new_image->y = RESCALE_Y;

        new_image->data = (ppm_pixel *)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
        if (!new_image)
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
        return new_image;
    }

    // Initializam datele necesare algoritmului
    void init()
    {

        this->aux = read_ppm(in_file);
        this->step_x = STEP;
        this->step_y = STEP;
        this->contour_map = init_contour_map();
        if (aux->x <= RESCALE_X && aux->y <= RESCALE_Y)
        {
            this->image = aux;
        }
        else
        {
            this->image = init_MAXimage();
        }
        this->p = image->x / step_x;
        this->q = image->y / step_y;
        init_grid();
    }

    void rescale_image(ppm_image *image, ppm_image *new_image, int start, int end)
    {
        uint8_t sample[3];

        // use bicubic interpolation for scaling

        for (int i = 0; i < new_image->x; i++)
        {
            for (int j = start; j < end; j++)
            {
                float u = (float)i / (float)(new_image->x - 1);
                float v = (float)j / (float)(new_image->y - 1);
                sample_bicubic(image, u, v, sample);

                new_image->data[i * new_image->y + j].red = sample[0];
                new_image->data[i * new_image->y + j].green = sample[1];
                new_image->data[i * new_image->y + j].blue = sample[2];
            }
            // asteptam ca toate thread-urile sa termine de pentru pasul curent
            pthread_barrier_wait(&barrier1);
        }
    }
    void sample_grid(int start, int end)
    {

        for (int i = 0; i < p; i++)
        {
            for (int j = start; j < end; j++)
            {
                ppm_pixel curr_pixel = image->data[i * step_x * image->y + j * step_y];

                unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

                grid[i][j] = (curr_color <= SIGMA);
            }
            // asteptam ca toate thread-urile sa termine de pentru pasul curent
            pthread_barrier_wait(&barrier2);
        }

        // last sample points have no neighbors below / to the right, so we use pixels on the
        // last row / column of the input image for them
        // las primul thread sa faca si ultima coloana si ultimul rand
        if (start == 0)
        {
            for (int i = 0; i < p; i++)
            {
                ppm_pixel curr_pixel = image->data[i * step_x * image->y + image->x - 1];

                unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

                grid[i][q] = (curr_color <= SIGMA);
            }
            for (int j = 0; j < q; j++)
            {
                ppm_pixel curr_pixel = image->data[(image->x - 1) * image->y + j * step_y];

                unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

                grid[p][j] = (curr_color <= SIGMA);
            }
        }
    }
    void march(int start, int end)
    {
        for (int i = 0; i < p; i++)
        {
            for (int j = start; j < end; j++)
            {
                unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
                update_image(image, contour_map[k], i * step_x, j * step_y);
            }
            // asteptam ca toate thread-urile sa termine de pentru pasul curent
            pthread_barrier_wait(&barrier1);
        }
    }

public:
    // Initializez fisierele de intrare si iesire, numarul de thread-uri si barierele
    Marching_Square(char *in_file, char *out_file, int P)
    {
        pthread_barrier_init(&barrier1, NULL, P);
        pthread_barrier_init(&barrier2, NULL, P);
        this->threads = vector<MyThread *>(P);
        this->out_file = out_file;
        this->in_file = in_file;
    }

    // Functia care ruleaza algoritmul
    void run()
    {
        long n = threads.size();
        // creez un vector de perechi de pointeri la obiectul curent si id-ul thread-ului
        vector<pair<Marching_Square *, long>> threadData(n);
        init();
        for (long i = 0; i < n; ++i)
        {
            threadData[i] = make_pair(this, i);
            threads[i] = new MyThread(i, ThreadFunction, (void *)&threadData[i]);
            threads[i]->start();
        }
        // dezalocam thread-urile ca sa faca join
        for (auto &thread : threads)
        {
            delete thread;
        }
        write_ppm(image, out_file);
    }
    // Dezalocam resursele
    ~Marching_Square()
    {
        free_resources(aux, image, contour_map, grid, step_x);
        pthread_barrier_destroy(&barrier1);
        pthread_barrier_destroy(&barrier2);
    }
};

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    // Alocam pe heap obiectul pentru ca e prea mare pentru stack
    Marching_Square *marching_square = new Marching_Square(argv[1], argv[2], atoi(argv[3]));
    marching_square->run();
    delete marching_square;

    return 0;
}
