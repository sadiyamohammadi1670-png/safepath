/*
 * ============================================================
 *  SafePath — Women's Safety Route Finder
 *  Backend: C Implementation of Dijkstra's Algorithm
 *  File   : safety_route.c
 *
 *  Algorithm: Modified Dijkstra's Shortest Path
 *  Weight   : distance_km × (11 − safety_score)
 *             → Favours short AND safe roads
 *             → Higher safety_score = lower cost
 *
 *  Compile  : gcc -o safety_route safety_route.c -lm
 *  Run      : ./safety_route
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* ── Constants ── */
#define MAX_NODES    20
#define MAX_EDGES   100
#define MAX_LABEL    64
#define INF         DBL_MAX

/* ── Data Structures ── */

/* A node in the city graph */
typedef struct {
    char  id[4];            /* Single letter ID like "A", "B" … */
    char  label[MAX_LABEL]; /* Human-readable name                */
    char  zone[32];         /* Zone type: commercial, residential … */
} Node;

/* A road (edge) between two nodes */
typedef struct {
    int    u, v;            /* Indices of the two endpoints       */
    double dist_km;         /* Physical distance in kilometres    */
    int    safety_score;    /* 1–10: 10 = perfectly safe          */
} Edge;

/* Graph holding all city data */
typedef struct {
    char  city_name[64];
    int   num_nodes;
    int   num_edges;
    Node  nodes[MAX_NODES];
    Edge  edges[MAX_EDGES];
} Graph;

/* Safety filters chosen by the user */
typedef struct {
    int cctv;         /* 1 = prefer routes with CCTV       */
    int lighting;     /* 1 = avoid poorly lit roads        */
    int police;       /* 1 = prefer police patrol zones    */
    int crowd;        /* 1 = prefer crowded (safer) areas  */
    int avoid_night;  /* 1 = penalise dark/isolated roads  */
} Filters;

/* Result returned after running Dijkstra */
typedef struct {
    int    path[MAX_NODES];  /* Node indices along the path        */
    int    path_len;         /* Number of nodes in path            */
    double total_dist_km;    /* Total physical distance            */
    double avg_safety;       /* Average safety score along path    */
    double weighted_cost;    /* Final Dijkstra cost (composite)    */
} RouteResult;

/* Min-heap node for the priority queue */
typedef struct {
    int    node_idx;
    double cost;
} PQNode;

/* Priority queue (binary min-heap) */
typedef struct {
    PQNode data[MAX_NODES * MAX_NODES];
    int    size;
} MinHeap;


/* ══════════════════════════════════════════
   Priority Queue (Min-Heap) operations
   ══════════════════════════════════════════ */

void pq_push(MinHeap *pq, int node, double cost) {
    int i = pq->size++;
    pq->data[i].node_idx = node;
    pq->data[i].cost     = cost;

    /* Bubble up */
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (pq->data[parent].cost <= pq->data[i].cost) break;
        PQNode tmp       = pq->data[parent];
        pq->data[parent] = pq->data[i];
        pq->data[i]      = tmp;
        i = parent;
    }
}

PQNode pq_pop(MinHeap *pq) {
    PQNode top = pq->data[0];
    pq->data[0] = pq->data[--pq->size];

    /* Bubble down */
    int i = 0;
    while (1) {
        int left  = 2*i + 1;
        int right = 2*i + 2;
        int smallest = i;

        if (left  < pq->size && pq->data[left].cost  < pq->data[smallest].cost) smallest = left;
        if (right < pq->size && pq->data[right].cost < pq->data[smallest].cost) smallest = right;
        if (smallest == i) break;

        PQNode tmp          = pq->data[smallest];
        pq->data[smallest]  = pq->data[i];
        pq->data[i]         = tmp;
        i = smallest;
    }
    return top;
}


/* ══════════════════════════════════════════
   Weight calculation
   Higher safety score → lower weight → preferred
   ══════════════════════════════════════════ */
double compute_weight(double dist_km, int safety, const Filters *f) {
    double penalty = (double)(11 - safety); /* Base: 10=1.0, 1=10.0 */

    if (f->avoid_night && safety < 6) penalty *= 2.5;
    if (f->lighting    && safety < 5) penalty *= 1.8;
    if (f->cctv        && safety < 4) penalty *= 2.0;
    if (f->police      && safety > 7) penalty *= 0.8; /* slight bonus */
    if (f->crowd       && safety < 5) penalty *= 1.5;

    return dist_km * penalty;
}


/* ══════════════════════════════════════════
   DIJKSTRA'S ALGORITHM
   Returns 1 on success, 0 if no path found
   ══════════════════════════════════════════ */
int dijkstra(
    const Graph   *g,
    const Filters *f,
    int            start,
    int            end,
    RouteResult   *result
) {
    double dist[MAX_NODES];   /* Minimum weighted cost from start   */
    int    prev[MAX_NODES];   /* Previous node on optimal path      */
    int    visited[MAX_NODES];
    double edge_dist[MAX_NODES];   /* Physical distance of chosen edge   */
    int    edge_safety[MAX_NODES]; /* Safety score of chosen edge        */

    /* Adjacency list */
    typedef struct { int to; double d; int s; } Adj;
    Adj adj[MAX_NODES][MAX_EDGES];
    int adj_cnt[MAX_NODES];
    memset(adj_cnt, 0, sizeof(adj_cnt));

    for (int i = 0; i < g->num_edges; i++) {
        int u = g->edges[i].u, v = g->edges[i].v;
        double d = g->edges[i].dist_km;
        int    s = g->edges[i].safety_score;
        adj[u][adj_cnt[u]++] = (Adj){v, d, s};
        adj[v][adj_cnt[v]++] = (Adj){u, d, s};
    }

    /* Initialise */
    for (int i = 0; i < g->num_nodes; i++) {
        dist[i]        = INF;
        prev[i]        = -1;
        visited[i]     = 0;
        edge_dist[i]   = 0.0;
        edge_safety[i] = 0;
    }
    dist[start] = 0.0;

    MinHeap pq;
    pq.size = 0;
    pq_push(&pq, start, 0.0);

    /* Main loop */
    while (pq.size > 0) {
        PQNode cur = pq_pop(&pq);
        int u = cur.node_idx;

        if (visited[u]) continue;
        visited[u] = 1;

        if (u == end) break; /* Found destination — early exit */

        for (int i = 0; i < adj_cnt[u]; i++) {
            int    v    = adj[u][i].to;
            double d    = adj[u][i].d;
            int    s    = adj[u][i].s;
            double w    = compute_weight(d, s, f);
            double newD = dist[u] + w;

            if (newD < dist[v]) {
                dist[v]        = newD;
                prev[v]        = u;
                edge_dist[v]   = d;
                edge_safety[v] = s;
                pq_push(&pq, v, newD);
            }
        }
    }

    if (dist[end] == INF) return 0; /* No path */

    /* Reconstruct path */
    int path[MAX_NODES], plen = 0;
    int cur = end;
    while (cur != -1) {
        path[plen++] = cur;
        cur = prev[cur];
    }
    /* Reverse */
    for (int i = 0; i < plen / 2; i++) {
        int t = path[i]; path[i] = path[plen-1-i]; path[plen-1-i] = t;
    }

    /* Copy path & compute stats */
    result->path_len       = plen;
    result->total_dist_km  = 0.0;
    result->weighted_cost  = dist[end];
    double safety_sum = 0.0;
    int    edge_count = 0;

    for (int i = 0; i < plen; i++) {
        result->path[i] = path[i];
        if (i > 0) {
            result->total_dist_km += edge_dist[path[i]];
            safety_sum            += edge_safety[path[i]];
            edge_count++;
        }
    }
    result->avg_safety = edge_count > 0 ? safety_sum / edge_count : 0.0;
    return 1;
}


/* ══════════════════════════════════════════
   Graph builder — New Delhi (Demo)
   ══════════════════════════════════════════ */
void build_delhi_graph(Graph *g) {
    strcpy(g->city_name, "New Delhi");
    g->num_nodes = 8;
    g->num_edges = 0;

    /* Nodes */
    char ids[][4]    = {"A","B","C","D","E","F","G","H"};
    char labels[][MAX_LABEL] = {
        "Connaught Place","Karol Bagh","Lajpat Nagar","Hauz Khas",
        "Saket","Vasant Kunj","Dwarka","RK Puram"
    };
    char zones[][32] = {
        "commercial","residential","market","upscale",
        "mall","residential","suburb","govt"
    };
    for (int i = 0; i < g->num_nodes; i++) {
        strcpy(g->nodes[i].id,    ids[i]);
        strcpy(g->nodes[i].label, labels[i]);
        strcpy(g->nodes[i].zone,  zones[i]);
    }

    /* Edges: [u_idx, v_idx, dist_km, safety_score] */
    int raw[][4] = {
        {0,1, 42, 8}, {0,2, 55, 7}, {0,3, 70, 9},
        {1,6, 81, 5}, {1,3, 58, 7}, {2,4, 43, 6},
        {3,4, 31, 9}, {3,7, 28, 8}, {4,5, 52, 7},
        {4,7, 25, 8}, {5,6, 63, 5}, {5,7, 38, 8},
        {6,7, 72, 4}
    };
    int num_raw = sizeof(raw)/sizeof(raw[0]);
    g->num_edges = num_raw;
    for (int i = 0; i < num_raw; i++) {
        g->edges[i].u           = raw[i][0];
        g->edges[i].v           = raw[i][1];
        g->edges[i].dist_km     = raw[i][2] / 10.0; /* stored as tenths */
        g->edges[i].safety_score= raw[i][3];
    }
}


/* ══════════════════════════════════════════
   Pretty-print route result
   ══════════════════════════════════════════ */
void print_route(const Graph *g, const RouteResult *r, const char *label) {
    printf("\n  ┌─ %s %s\n", label,
           r->avg_safety >= 7.5 ? "[ SAFE ✔ ]" :
           r->avg_safety >= 6.0 ? "[ CAUTION ⚠ ]" : "[ RISK ✘ ]");

    printf("  │  Path    : ");
    for (int i = 0; i < r->path_len; i++) {
        printf("%s", g->nodes[r->path[i]].id);
        if (i < r->path_len - 1) printf(" → ");
    }
    printf("\n");

    printf("  │  Labels  : ");
    for (int i = 0; i < r->path_len; i++) {
        printf("%s", g->nodes[r->path[i]].label);
        if (i < r->path_len - 1) printf(" → ");
    }
    printf("\n");

    printf("  │  Distance: %.1f km\n",   r->total_dist_km);
    printf("  │  Safety  : %.1f / 10\n", r->avg_safety);
    printf("  │  Cost    : %.2f\n",      r->weighted_cost);
    printf("  └─────────────────────────────────\n");
}


/* ══════════════════════════════════════════
   Sorting utility — Sort edges by safety desc
   Uses Quick Sort (C standard library qsort)
   ══════════════════════════════════════════ */
int cmp_edge_safety_desc(const void *a, const void *b) {
    const Edge *ea = (const Edge *)a;
    const Edge *eb = (const Edge *)b;
    return eb->safety_score - ea->safety_score;
}

void print_sorted_edges(const Graph *g) {
    Edge sorted[MAX_EDGES];
    memcpy(sorted, g->edges, g->num_edges * sizeof(Edge));
    qsort(sorted, g->num_edges, sizeof(Edge), cmp_edge_safety_desc);

    printf("\n  ══ Road Safety Rankings (Quick Sort, descending) ══\n");
    printf("  %-20s  %-20s  %5s  %6s\n", "From", "To", "Dist", "Safety");
    printf("  %-20s  %-20s  %5s  %6s\n",
           "────────────────────","────────────────────","─────","──────");
    for (int i = 0; i < g->num_edges; i++) {
        printf("  %-20s  %-20s  %4.1fkm  %3d/10\n",
               g->nodes[sorted[i].u].label,
               g->nodes[sorted[i].v].label,
               sorted[i].dist_km,
               sorted[i].safety_score);
    }
}


/* ══════════════════════════════════════════
   Interactive menu
   ══════════════════════════════════════════ */
void show_menu(const Graph *g) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  🛡️   SafePath — Women's Safety Route Finder     ║\n");
    printf("║        City: %-34s ║\n", g->city_name);
    printf("╚══════════════════════════════════════════════════╝\n");

    printf("\n  Available Locations:\n");
    for (int i = 0; i < g->num_nodes; i++)
        printf("    [%s] %s (%s)\n",
               g->nodes[i].id,
               g->nodes[i].label,
               g->nodes[i].zone);
}

int node_index(const Graph *g, const char *id) {
    for (int i = 0; i < g->num_nodes; i++)
        if (strcmp(g->nodes[i].id, id) == 0) return i;
    return -1;
}


/* ══════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════ */
int main(void) {
    Graph g;
    build_delhi_graph(&g);
    show_menu(&g);

    /* Get user input */
    char start_id[8], end_id[8];
    printf("\n  Enter START node ID  : ");
    scanf("%7s", start_id);
    printf("  Enter END   node ID  : ");
    scanf("%7s", end_id);

    /* Convert to uppercase */
    for (int i = 0; start_id[i]; i++)
        if (start_id[i] >= 'a') start_id[i] -= 32;
    for (int i = 0; end_id[i]; i++)
        if (end_id[i] >= 'a') end_id[i] -= 32;

    int s = node_index(&g, start_id);
    int e = node_index(&g, end_id);

    if (s < 0 || e < 0) {
        printf("\n  ❌ Invalid node ID(s). Exiting.\n");
        return 1;
    }
    if (s == e) {
        printf("\n  ⚠  Start and end are the same node.\n");
        return 1;
    }

    /* Get filter preferences */
    printf("\n  ── Safety Filters ──\n");
    Filters f = {0};
    char buf[4];
    printf("  Enable CCTV preference?   (y/n): "); scanf("%3s", buf); f.cctv        = buf[0]=='y';
    printf("  Enable lighting filter?   (y/n): "); scanf("%3s", buf); f.lighting    = buf[0]=='y';
    printf("  Prefer police zones?      (y/n): "); scanf("%3s", buf); f.police      = buf[0]=='y';
    printf("  Prefer crowded areas?     (y/n): "); scanf("%3s", buf); f.crowd       = buf[0]=='y';
    printf("  Night mode (avoid dark)?  (y/n): "); scanf("%3s", buf); f.avoid_night = buf[0]=='y';

    /* ── Run three route variants ── */
    printf("\n\n══════════ ROUTE RESULTS ══════════\n");

    RouteResult r1, r2, r3;

    /* 1. Safest route: all filters enabled */
    Filters f_safe = {1,1,0,1,1};
    int ok1 = dijkstra(&g, &f_safe, s, e, &r1);
    if (ok1) print_route(&g, &r1, "Safest Route  ");

    /* 2. Fastest route: no filters (minimise distance) */
    Filters f_fast = {0,0,0,0,0};
    int ok2 = dijkstra(&g, &f_fast, s, e, &r2);
    if (ok2) print_route(&g, &r2, "Fastest Route ");

    /* 3. User-filtered route */
    int ok3 = dijkstra(&g, &f, s, e, &r3);
    if (ok3) print_route(&g, &r3, "Your Route    ");

    if (!ok1 && !ok2 && !ok3)
        printf("\n  ❌ No route found between %s and %s.\n", start_id, end_id);

    /* Print sorted edge safety table */
    print_sorted_edges(&g);

    printf("\n  ✔  Dijkstra complexity: O((V + E) log V)\n");
    printf("  ✔  Quick Sort for edges: O(E log E)\n\n");

    return 0;
}
