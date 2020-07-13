#pragma once

typedef struct point_s point_t;
struct point_s {
    double x, y, z;
    int cluster_id;
};

typedef struct node_s node_t;
struct node_s {
    unsigned int index;
    node_t *next;
};

typedef struct epsilon_neighbours_s epsilon_neighbours_t;
struct epsilon_neighbours_s {
    unsigned int num_members;
    node_t *head, *tail;
};

node_t *create_node(unsigned int index);
int append_at_end(
    unsigned int index,
    epsilon_neighbours_t *en);
epsilon_neighbours_t *get_epsilon_neighbours(
    unsigned int index,
    point_t *points,
    unsigned int num_points,
    double epsilon,
    double(*dist)(point_t *a, point_t *b));
void print_epsilon_neighbours(
    point_t *points,
    epsilon_neighbours_t *en);
void destroy_epsilon_neighbours(epsilon_neighbours_t *en);
void dbscan(
    point_t *points,
    unsigned int num_points,
    double epsilon,
    unsigned int minpts,
    double(*dist)(point_t *a, point_t *b));
int expand(
    unsigned int index,
    unsigned int cluster_id,
    point_t *points,
    unsigned int num_points,
    double epsilon,
    unsigned int minpts,
    double(*dist)(point_t *a, point_t *b));
int spread(
    unsigned int index,
    epsilon_neighbours_t *seeds,
    unsigned int cluster_id,
    point_t *points,
    unsigned int num_points,
    double epsilon,
    unsigned int minpts,
    double(*dist)(point_t *a, point_t *b));
double euclidean_dist(point_t *a, point_t *b);
double adjacent_intensity_dist(point_t *a, point_t *b);
unsigned int parse_input(
    FILE *file,
    point_t **points,
    double *epsilon,
    unsigned int *minpts);
void print_points(
    point_t *points,
    unsigned int num_points);