#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bmp_header.h"

#define BUFFER_SIZE 50
#define HEIGHT info_h.height
#define WIDTH info_h.width

// type definitions

#pragma pack(1)

typedef struct pixel {
    unsigned char B, G, R; // cele 3 canale de culoare pentru fiecare pixel
} pixel;

typedef struct input {
    FILE *input_text_file; // file pointer catre input.txt
    char *image_path; // file path catre imaginea .bmp
    char *conv_filter; // file path catre filtrul convolutional
    char *pooling; // file path catre filtrul de pooling
    char *clustering; // file path catre filtrul de clustering
    int test_number; // numarul testului curent
} input;

typedef struct bmp_file {
    FILE *image; // file pointer catre imaginea .bmp
    bmp_fileheader file_h;
    bmp_infoheader info_h;
    pixel **map; // matricea de pixeli
    unsigned char padding; // nr de octeti de padding
} bmp_file;

typedef struct coord {
    int i;
    int j;
} coord;

#pragma pack()

// memory management

void mem_dealloc_input(input *data) {
    free(data->image_path);
    data->image_path = NULL;
    free(data->conv_filter);
    data->conv_filter = NULL;
    free(data->pooling);
    data->pooling = NULL;
    free(data->clustering);
    data->clustering = NULL;
    free(data);
    data = NULL;
}

pixel **mem_alloc_map(bmp_file *img) {
    img->map = (pixel **)malloc(img->HEIGHT * sizeof(pixel *));
    int i;
    for (i = 0 ; i < img->HEIGHT; i++) {
        img->map[i] = (pixel *)calloc(img->WIDTH, sizeof(pixel));
    }
    return img->map;
}

void mem_dealloc_bmp(bmp_file *img) {
    int i;
    for (i = 0 ; i < img->HEIGHT; i++) {
        free(img->map[i]);
        img->map[i] = NULL;
    }
    free(img->map);
    img->map = NULL;
    free(img);
    img = NULL;
}

double **mem_alloc_filter(FILE *conv, int filter_dim) {
    double **filter = (double **)malloc(filter_dim * sizeof(double *));
    int i;
    for (i = 0; i < filter_dim; i++) {
        filter[i] = (double *)malloc(filter_dim * sizeof(double));
        int j;
        for (j = 0; j < filter_dim; j++) {
            fscanf(conv, "%le", &filter[i][j]);
        }
    }
    return filter;
}

void mem_dealloc_filter(double **filter, int filter_dim) {
    int i;
    for (i = 0; i < filter_dim; i++) {
        free(filter[i]);
        filter[i] = NULL;
    }
    free(filter);
    filter = NULL;
}

int **mem_alloc_overlay(bmp_file *img) {
    int **overlay_map = (int **)calloc(img->HEIGHT, sizeof(int *));
    int i;
    for (i = 0; i < img->HEIGHT; i++) {
        overlay_map[i] = (int *)calloc(img->WIDTH, sizeof(int));
    }
    return overlay_map;
}

void mem_dealloc_overlay(bmp_file *img, int **overlay_map) {
    int i;
    for (i = 0; i < img->HEIGHT; i++) {
        free(overlay_map[i]);
        overlay_map[i] = NULL;
    }
    free(overlay_map);
    overlay_map = NULL;
}

coord *mem_alloc_coord(bmp_file *img) {
    return (coord *)calloc(img->HEIGHT * img->WIDTH, sizeof(coord));
}

void mem_dealloc_coord(coord *coord_map) {
    free(coord_map);
    coord_map = NULL;
}

// read image from file

void read_headers(bmp_file *img) {
    fseek(img->image, 0, SEEK_SET);
    fread(&img->file_h, sizeof(bmp_fileheader), 1, img->image);
    fread(&img->info_h.biSize, sizeof(bmp_infoheader), 1, img->image);
}

void read_map(bmp_file *img) {
    fseek(img->image, img->file_h.imageDataOffset, SEEK_SET);
    img->padding = img->WIDTH % 4;
    img->map = mem_alloc_map(img);
    int i;
    for (i = 0 ; i < img->HEIGHT; i++) {
        int j;
        for (j = 0; j < img->WIDTH; j++) {
            fread(&img->map[i][j], sizeof(pixel), 1, img->image);
        }
        if (img->padding) {
            fseek(img->image, img->padding, SEEK_CUR);
        }
    }
}

// print image to file

void write_headers(bmp_file *img) {
    fseek(img->image, 0, SEEK_SET);
    fwrite(&img->file_h, sizeof(bmp_fileheader), 1, img->image);
    fwrite(&img->info_h, sizeof(bmp_infoheader), 1, img->image);
}

void write_map(bmp_file *img) {
    fseek(img->image, img->file_h.imageDataOffset, SEEK_SET);
    img->padding = img->WIDTH % 4;
    int i;
    for (i = 0; i < img->HEIGHT; i++) {
        int j;
        for (j = 0; j < img->WIDTH; j++) {
            fwrite(&img->map[i][j], sizeof(pixel), 1, img->image);
        }
        if (img->padding) {
            unsigned char blank_char = 0;
            int k;
            for(k = 0; k < img->padding; k++) {
                fwrite(&blank_char, 1, 1, img->image);
            }
        }
    }
}

// read input files

void get_input(input *data, bmp_file *img) {
    // read file paths from input.txt
    data->input_text_file = fopen("input.txt", "r");
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    fscanf(data->input_text_file, "%s", buffer);
    data->image_path = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
    strcpy(data->image_path, buffer);
    fscanf(data->input_text_file, "%s", buffer);
    data->conv_filter = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
    strcpy(data->conv_filter, buffer);
    fscanf(data->input_text_file, "%s", buffer);
    data->pooling = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
    strcpy(data->pooling, buffer);
    fscanf(data->input_text_file, "%s", buffer);
    data->clustering = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
    strcpy(data->clustering, buffer);
    free(buffer);
    fclose(data->input_text_file);
    sscanf(data->image_path, "test%d.bmp", &data->test_number);

    // read .bmp file
    img->image = fopen(data->image_path, "rb");
    read_headers(img);
    read_map(img);
    fclose(img->image);
}

void assign_gray_pixel(pixel *px, unsigned char value) {
    px->B = value;
    px->G = value;
    px->R = value;
}

void grayscale(input *in_data, bmp_file *in_img) {
    // task-specific declarations
    bmp_file *gray_img = (bmp_file *)malloc(sizeof(bmp_file));
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    sprintf(buffer, "test%d_black_white.bmp", in_data->test_number);
    gray_img->image = fopen(buffer, "wb");
    free(buffer);
    gray_img->file_h = in_img->file_h;
    gray_img->info_h = in_img->info_h;
    gray_img->padding = in_img->padding;
    gray_img->map = mem_alloc_map(gray_img);
    int i;
    for (i = 0 ; i < gray_img->HEIGHT; i++) {
        int j;
        for (j = 0; j < gray_img->WIDTH; j++) {
            unsigned int average = 0;
            average += in_img->map[i][j].B;
            average += in_img->map[i][j].G;
            average += in_img->map[i][j].R;
            average /= 3;
            assign_gray_pixel(&gray_img->map[i][j], (unsigned char)average);
        }
    }
    write_headers(gray_img);
    write_map(gray_img);
    fclose(gray_img->image);
    mem_dealloc_bmp(gray_img);
}

void no_crop(input *in_data, bmp_file *in_img) {
    // task-specific declarations
    bmp_file *crop_img = (bmp_file *)malloc(sizeof(bmp_file));
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    sprintf(buffer, "test%d_nocrop.bmp", in_data->test_number);
    crop_img->image = fopen(buffer, "wb");
    free(buffer);
    crop_img->file_h = in_img->file_h;
    crop_img->info_h = in_img->info_h;

    if (crop_img->WIDTH == crop_img->HEIGHT) {
        // case: image is already square
        crop_img->map = mem_alloc_map(crop_img);
        int i;
        for (i = 0 ; i < crop_img->HEIGHT; i++) {
            int j;
            for (j = 0; j < crop_img->WIDTH; j++) {
                crop_img->map[i][j] = in_img->map[i][j];
            }
        }
    } else if (crop_img->WIDTH > crop_img->HEIGHT) {
        // case: image is wider
        int diff = crop_img->WIDTH - crop_img->HEIGHT;
        crop_img->HEIGHT = crop_img->WIDTH;
        crop_img->map = mem_alloc_map(crop_img);
        int i;
        for (i = 0 ; i < crop_img->WIDTH; i++) {
            int j;
            for (j = 0; j < crop_img->WIDTH; j++) {
                if ((i < diff/2 + diff%2) ||
                    (i > crop_img->WIDTH - diff/2 - 1)) {
                    assign_gray_pixel(&crop_img->map[i][j], 255);
                } else {
                    crop_img->map[i][j] = in_img->map[i - diff/2 - diff%2][j];
                }
            }
        }
    } else if (crop_img->WIDTH < crop_img->HEIGHT) {
        // case: image is higher
        int diff = crop_img->HEIGHT - crop_img->WIDTH;
        crop_img->WIDTH = crop_img->HEIGHT;
        crop_img->map = mem_alloc_map(crop_img);
        int i;
        for (i = 0 ; i < crop_img->WIDTH; i++) {
            int j;
            for (j = 0; j < crop_img->WIDTH; j++) {
                if ((j < diff/2 + diff%2) ||
                    (j > crop_img->WIDTH - diff/2 - 1)) {
                    assign_gray_pixel(&crop_img->map[i][j], 255);
                } else {
                    crop_img->map[i][j] = in_img->map[i][j - diff/2 - diff%2];
                }
            }
        }
    }
    write_headers(crop_img);
    write_map(crop_img);
    fclose(crop_img->image);
    mem_dealloc_bmp(crop_img);
}

// truncates color channels to 0 - 255

void truncate_pixel_color(pixel *px, int blue, int green, int red) {
    if (blue > 255) {
        px->B = 255;
    } else if (blue < 0) {
        px->B = 0;
    } else {
        px->B = blue;
    }

    if (green > 255) {
        px->G = 255;
    } else if (green < 0) {
        px->G = 0;
    } else {
        px->G = green;
    }

    if (red > 255) {
        px->R = 255;
    } else if (red < 0) {
        px->R = 0;
    } else {
        px->R = red;
    }
}

void convolutional_layers(input *in_data, bmp_file *in_img) {
    // task-specific declarations
    FILE *conv = fopen(in_data->conv_filter, "r");
    int filter_dim;
    fscanf(conv, "%d", &filter_dim);
    double **filter = mem_alloc_filter(conv, filter_dim);
    fclose(conv);
    bmp_file *conv_img = (bmp_file *)malloc(sizeof(bmp_file));
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    sprintf(buffer, "test%d_filter.bmp", in_data->test_number);
    conv_img->image = fopen(buffer, "wb");
    free(buffer);
    conv_img->file_h = in_img->file_h;
    conv_img->info_h = in_img->info_h;
    conv_img->padding = in_img->padding;
    conv_img->map = mem_alloc_map(conv_img);
    int i;
    for (i = 0 ; i < conv_img->HEIGHT; i++) {
        int j;
        for (j = 0; j < conv_img->WIDTH; j++) {
            int blue = 0, green = 0, red = 0;
            int q;
            for (q = i - filter_dim/2; q <= i + filter_dim/2; q++) {
                int r;
                for (r = j - filter_dim/2; r <= j + filter_dim/2; r++) {
                    if ((q >= 0) && (q < conv_img->HEIGHT) &&
                        (r >= 0) && (r < conv_img->WIDTH)) {
                        blue += in_img->map[q][r].B *
                        filter[-q + i + filter_dim/2][r - j + filter_dim/2];
                        green += in_img->map[q][r].G *
                        filter[-q + i + filter_dim/2][r - j + filter_dim/2];
                        red += in_img->map[q][r].R *
                        filter[-q + i + filter_dim/2][r - j + filter_dim/2];
                    }
                }
            }
            truncate_pixel_color(&conv_img->map[i][j], blue, green, red);
        }
    }
    write_headers(conv_img);
    write_map(conv_img);
    fclose(conv_img->image);
    mem_dealloc_bmp(conv_img);
    mem_dealloc_filter(filter, filter_dim);
}

void edge_detect(input *in_data, bmp_file *in_img, double transform) {
    // task-specific declarations

    int filter_dim, aux;

    FILE *edge_h = fopen("edgefilter_h.txt", "r");
    fscanf(edge_h, "%d", &filter_dim);
    FILE *edge_v = fopen("edgefilter_v.txt", "r");
    fscanf(edge_v, "%d", &aux);

    if (aux != filter_dim) {
        fclose(edge_h);
        fclose(edge_v);
        return;
    }

    double **filter_h = mem_alloc_filter(edge_h, filter_dim);
    double **filter_v = mem_alloc_filter(edge_v, filter_dim);
    fclose(edge_h);
    fclose(edge_v);

    bmp_file *edge_img = (bmp_file *)malloc(sizeof(bmp_file));
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    sprintf(buffer, "test%d_edges.bmp", in_data->test_number);
    edge_img->image = fopen(buffer, "wb");
    free(buffer);

    edge_img->file_h = in_img->file_h;
    edge_img->info_h = in_img->info_h;
    edge_img->padding = in_img->padding;
    edge_img->map = mem_alloc_map(edge_img);

    int i;
    double ** aux_map_h = (double **)malloc(edge_img->HEIGHT * sizeof(double *));
    for (i = 0 ; i < edge_img->HEIGHT; i++) {
        aux_map_h[i] = (double *)calloc(edge_img->WIDTH, sizeof(double));
    }

    double ** aux_map_v = (double **)malloc(edge_img->HEIGHT * sizeof(double *));
    for (i = 0 ; i < edge_img->HEIGHT; i++) {
        aux_map_v[i] = (double *)calloc(edge_img->WIDTH, sizeof(double));
    }

    double ** pixel_val = (double **)malloc(edge_img->HEIGHT * sizeof(double *));
    for (i = 0 ; i < edge_img->HEIGHT; i++) {
        pixel_val[i] = (double *)calloc(edge_img->WIDTH, sizeof(double));
    }

    for (i = 0 ; i < edge_img->HEIGHT; i++) {
        int j;
        for (j = 0; j < edge_img->WIDTH; j++) {
            int q;
            for (q = i - filter_dim/2; q <= i + filter_dim/2; q++) {
                int r;
                for (r = j - filter_dim/2; r <= j + filter_dim/2; r++) {
                    if ((q >= 0) && (q < edge_img->HEIGHT) &&
                        (r >= 0) && (r < edge_img->WIDTH)) {

                        aux_map_h[i][j] += in_img->map[q][r].B *
                        filter_h[-q + i + filter_dim/2][r - j + filter_dim/2];
                        aux_map_h[i][j] += in_img->map[q][r].G *
                        filter_h[-q + i + filter_dim/2][r - j + filter_dim/2];
                        aux_map_h[i][j] += in_img->map[q][r].R *
                        filter_h[-q + i + filter_dim/2][r - j + filter_dim/2];

                        aux_map_v[i][j] += in_img->map[q][r].B *
                        filter_v[-q + i + filter_dim/2][r - j + filter_dim/2];
                        aux_map_v[i][j] += in_img->map[q][r].G *
                        filter_v[-q + i + filter_dim/2][r - j + filter_dim/2];
                        aux_map_v[i][j] += in_img->map[q][r].R *
                        filter_v[-q + i + filter_dim/2][r - j + filter_dim/2];
                    }
                }
            }
        }
    }

    for (i = 0 ; i < edge_img->HEIGHT; i++) {
        int j;
        for (j = 0; j < edge_img->WIDTH; j++) {
            aux_map_h[i][j] *= aux_map_h[i][j];
            aux_map_v[i][j] *= aux_map_v[i][j];
            pixel_val[i][j] = sqrt(aux_map_h[i][j] + aux_map_v[i][j]);
            pixel_val[i][j] *= transform;
            if (pixel_val[i][j] > 255) {
                pixel_val[i][j] = 255;
            }
            if (pixel_val[i][j] < 0) {
                pixel_val[i][j] = 0;
            }

            edge_img->map[i][j].B = (char) pixel_val[i][j];
            edge_img->map[i][j].G = (char) pixel_val[i][j];
            edge_img->map[i][j].R = (char) pixel_val[i][j];
        }
    }

    for (i = 0 ; i < edge_img->HEIGHT; i++) {
        aux_map_h[i] = NULL;
        aux_map_v[i] = NULL;
        pixel_val[i] = NULL;
    }
    aux_map_h = NULL;
    aux_map_v = NULL;
    pixel_val = NULL;

    write_headers(edge_img);
    write_map(edge_img);
    fclose(edge_img->image);
    mem_dealloc_bmp(edge_img);
    mem_dealloc_filter(filter_h, filter_dim);
    mem_dealloc_filter(filter_v, filter_dim);
}

void pool_pixel(pixel *ref, pixel *pooled, char filter_type) {
    if (filter_type == 'M') {
        if (ref->B > pooled->B) {
            pooled->B = ref->B;
        }
        if (ref->G > pooled->G) {
            pooled->G = ref->G;
        }
        if (ref->R > pooled->R) {
            pooled->R = ref->R;
        }
    }
    if (filter_type == 'm') {
        if (ref->B < pooled->B) {
            pooled->B = ref->B;
        }
        if (ref->G < pooled->G) {
            pooled->G = ref->G;
        }
        if (ref->R < pooled->R) {
            pooled->R = ref->R;
        }
    }
}

void pooling(input *in_data, bmp_file *in_img) {
    // task-specific declarations
    FILE *pool = fopen(in_data->pooling, "r");
    char filter;
    fscanf(pool, "%s", &filter);
    int filter_dim;
    fscanf(pool, "%d", &filter_dim);
    fclose(pool);
    bmp_file *pool_img = (bmp_file *)malloc(sizeof(bmp_file));
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    sprintf(buffer, "test%d_pooling.bmp", in_data->test_number);
    pool_img->image = fopen(buffer, "wb");
    free(buffer);
    pool_img->file_h = in_img->file_h;
    pool_img->info_h = in_img->info_h;
    pool_img->padding = in_img->padding;
    pool_img->map = mem_alloc_map(pool_img);
    int i;
    for (i = 0 ; i < pool_img->HEIGHT; i++) {
        int j;
        for (j = 0; j < pool_img->WIDTH; j++) {
            pool_img->map[i][j] = in_img->map[i][j];
            int q;
            for (q = i - filter_dim/2; q <= i + filter_dim/2; q++) {
                int r;
                for (r = j - filter_dim/2; r <= j + filter_dim/2; r++) {
                    if ((q >= 0) && (q < pool_img->HEIGHT) &&
                        (r >= 0) && (r < pool_img->WIDTH)) {
                        pool_pixel(&in_img->map[q][r], &pool_img->map[i][j],
                        filter);
                    }
                    if ((q < 0) || (q >= pool_img->HEIGHT) ||
                        (r < 0) || (r >= pool_img->WIDTH)) {
                        if (filter == 'm') {
                            assign_gray_pixel(&pool_img->map[i][j], 0);
                        }
                    }
                }
            }
        }
    }
    write_headers(pool_img);
    write_map(pool_img);
    fclose(pool_img->image);
    mem_dealloc_bmp(pool_img);
}

int is_in_threshold(pixel *ref, pixel *clustered, int treshold) {
    int weight = 0;
    if (ref->B > clustered->B) {
        weight += ref->B - clustered->B;
    } else {
        weight += clustered->B - ref->B;
    }
    if (ref->G > clustered->G) {
        weight += ref->G - clustered->G;
    } else {
        weight += clustered->G - ref->G;
    }
    if (ref->R > clustered->R) {
        weight += ref->R - clustered->R;
    } else {
        weight += clustered->R - ref->R;
    }
    if (weight <= treshold) {
        return 1;
    } else {
        return 0;
    }
}

// check neighbouring pixels to see if they are in the threshold
// if affirmative, add their own neighbours in queue to be checked 

void check_neighbours(bmp_file *in_img, coord *pos, int q, int start, int *end,
    int **overlay, int threshold) {
    if ((pos[q].j - 1 >= 0) && (!overlay[pos[q].i][pos[q].j - 1]) &&
    is_in_threshold(&in_img->map[pos[start].i][pos[start].j],
    &in_img->map[pos[q].i][pos[q].j - 1], threshold)) {
        (*end)++;
        overlay[pos[q].i][pos[q].j - 1] = 1;
        pos[*end].i = pos[q].i;
        pos[*end].j = pos[q].j - 1;
    }

    if ((pos[q].i + 1 < in_img->HEIGHT) && (!overlay[pos[q].i + 1][pos[q].j]) &&
    is_in_threshold(&in_img->map[pos[start].i][pos[start].j],
    &in_img->map[pos[q].i + 1][pos[q].j], threshold)) {
        (*end)++;
        overlay[pos[q].i + 1][pos[q].j] = 1;
        pos[*end].i = pos[q].i + 1;
        pos[*end].j = pos[q].j;
    }

    if ((pos[q].j + 1 < in_img->WIDTH) && (!overlay[pos[q].i][pos[q].j + 1]) &&
    is_in_threshold(&in_img->map[pos[start].i][pos[start].j],
    &in_img->map[pos[q].i][pos[q].j + 1], threshold)) {
        (*end)++;
        overlay[pos[q].i][pos[q].j + 1] = 1;
        pos[*end].i = pos[q].i;
        pos[*end].j = pos[q].j + 1;
    }
    
    if ((pos[q].i - 1 >= 0) && (!overlay[pos[q].i - 1][pos[q].j]) &&
    is_in_threshold(&in_img->map[pos[start].i][pos[start].j],
    &in_img->map[pos[q].i - 1][pos[q].j], threshold)) {
        (*end)++;
        overlay[pos[q].i - 1][pos[q].j] = 1;
        pos[*end].i = pos[q].i - 1;
        pos[*end].j = pos[q].j;
    }
}

// fill cluster with the average color

void fill_cluster(bmp_file *in_img, bmp_file *cluster_img,
    coord *pos, int start, int end) {
    int blue = 0, green = 0, red = 0;
    int q;
    for (q = start; q <= end; q++) {
        blue += in_img->map[pos[q].i][pos[q].j].B;
        green += in_img->map[pos[q].i][pos[q].j].G;
        red += in_img->map[pos[q].i][pos[q].j].R;
    }

    blue /= (end - start + 1);
    green /= (end - start + 1);
    red /= (end - start + 1);

    for (q = start; q <= end; q++) {
        cluster_img->map[pos[q].i][pos[q].j].B = blue;
        cluster_img->map[pos[q].i][pos[q].j].G = green;
        cluster_img->map[pos[q].i][pos[q].j].R = red;
    }
}

void clustering(input *in_data, bmp_file *in_img) {
    // task-specific declarations
    FILE *cluster = fopen(in_data->clustering, "r");
    int th;
    fscanf(cluster, "%d", &th);
    fclose(cluster);
    bmp_file *cluster_img = (bmp_file *)malloc(sizeof(bmp_file));
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    sprintf(buffer, "test%d_clustered.bmp", in_data->test_number);
    cluster_img->image = fopen(buffer, "wb");
    free(buffer);
    cluster_img->file_h = in_img->file_h;
    cluster_img->info_h = in_img->info_h;
    cluster_img->padding = in_img->padding;
    cluster_img->map = mem_alloc_map(cluster_img);
    int **overlay = mem_alloc_overlay(cluster_img);
    coord *pos = mem_alloc_coord(cluster_img);

    int start = 0, end = 0;
    int i;
    for (i = cluster_img->HEIGHT - 1; i >= 0; i--) {
        int j;
        for (j = 0; j < cluster_img->WIDTH; j++) {
            if (!overlay[i][j]) {
                overlay[i][j] = 1;
                pos[start].i = i;
                pos[start].j = j;
                int q = start;
                while(q <= end) {                    
                    check_neighbours(in_img, pos, q, start, &end, overlay, th);
                    q++;
                }
                fill_cluster(in_img, cluster_img, pos, start, end);
                start = ++end;
            }
        }
    }    
    write_headers(cluster_img);
    write_map(cluster_img);
    fclose(cluster_img->image);
    mem_dealloc_overlay(cluster_img, overlay);
    mem_dealloc_coord(pos);
    mem_dealloc_bmp(cluster_img);
}

int main() {
    // alloc memory for input data and input image; read it

    input *in_data = (input *)malloc(sizeof(input));
    bmp_file *in_img = (bmp_file *)malloc(sizeof(bmp_file));
    get_input(in_data, in_img);

    // solve tasks

    edge_detect(in_data, in_img, 0.2);
    grayscale(in_data, in_img);
    no_crop(in_data, in_img);
    convolutional_layers(in_data, in_img);
    pooling(in_data, in_img);
    clustering(in_data, in_img);

    // dealloc memory

    mem_dealloc_input(in_data);
    mem_dealloc_bmp(in_img);

    return 0;
}
