/*
 * BEST PATH FOUND Threads=64, tries=1000
 * 38714. 3 34 44 9 23 31 20 42 16 26 18 36 5 29 27 35 43 17 6 30 37 8 7 0 15 21 39 46 19 32 45 14 11 10 13 33 40 2 22 12 24 38 47 4 28 1 41 25
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for IO redirection
#include <fcntl.h> // for file creation
#include <math.h> // compile with -lm

#define NUM_THREADS 64
#define NUM_TRIES 100
#define FILE_NAME "cities.txt"

// Method for threads to execute
void* thread_hill_climb(void*);

// Initialization methods
void init_dists(); // Tested and works 100%
void init_path(); 

// Helper methods 
int find_path_len(int path[]);
void switch_pvalues(int path[], int index1, int index2);
void print_path(int path[], int length);
float distance(int x1, int y1, int x2, int y2);
int rand_int(int n);

// Synchronized methods for threads to access the critical section
int compare_and_copy_bpath(int path[], int *length);
void compare_and_update_bpath(int path[], int length);

int num_nodes; // number of nodes loaded
int min_len; // current minimum round trip length
int *min_path; // the current minimum path (array)
// dists[a][b] == dists[b][a] := distance from a to b
int **dists; // 2D array filled with distances between nodes.

int last_len;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main() {
    srand(time(NULL));
    init_dists();
    init_path();

    print_path(min_path, min_len);

    pthread_t t[NUM_THREADS];

    pthread_attr_t attr; // set thread detached attribute
    pthread_attr_init(&attr); // uses dynamic memory allocation
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Create and run the threads
    int i;
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&t[i], &attr, thread_hill_climb, NULL);
    }
    pthread_attr_destroy(&attr);

    // Join the threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(t[i], 0);
    }
    
    // Deallocate some memory
    free(min_path);
    for(i = 0; i < num_nodes; i++)
    {
        free(dists[i]);
    }
    free(dists);
    
    return EXIT_SUCCESS;
}

/**
 * Function for the threads to execute the hill climbing algorithm
 * for finding an OK traveling salesman path
 */
void* thread_hill_climb(void* t) {
    // The local length and path of this thread
    int plen = min_len + 1; // so that distance is > min_distance
    int path[num_nodes];
    
    int r1, r2, trycount;
    for (trycount = 0; trycount < NUM_TRIES; trycount++) 
    {
        // Check to see if the global solution is better than the 
        // local solution. If it is, the global solution will be
        // copied.
        if (min_len < plen && compare_and_copy_bpath(path, &plen)) {
            // local path was updated with the best path
            trycount = 0;
        }
        
        // Pick two random indices. Store in r1, r2
        r1 = rand_int(num_nodes);
        r2 = rand_int(num_nodes-1);
        r2 += r2 >= r1; // Shift r2's distribution so that r2 != r1
        
        // Switch the two and see if it makes the path better
        switch_pvalues(path, r1, r2);
        int newplen = find_path_len(path);
        
        // Check to see if the new length is better
        if (newplen < plen) {
            // If so then update the global path with the current path
            // only if the newplen is better
            compare_and_update_bpath(path, newplen);
            // if this "better path" is worse than another best path found,
            // the next iteration will copy the new best path
        }
        // The new length is worse...
        else {
            // Undo the swap
            switch_pvalues(path, r1, r2);
        }
    }
    pthread_exit(NULL);
}


/*
 * Reads the local file "cities.txt" which should be filled with integers
 * which represent a series of x and y values for nodes.
 * This method will initialize 'num_nodes' and 'dists'.
 * If "cities.txt" is not found this method will cause core dump :(.
 */
void init_dists() {
    // I use i and j a lot so I'll initialize them here
    int i, j;
    
    // Redirect input
    int file_id = open(FILE_NAME, O_RDONLY, 0666);
    if (file_id == -1) {
        printf("FILE '%s' NOT FOUND.\n", FILE_NAME);
        exit(EXIT_FAILURE);
    }
    close(0);
    dup(file_id);
    close(file_id);
    
    // Declare the number of nodes to be 0
    num_nodes = 0;
    
    // Used as a low level stack to store all input from the file
    struct stack_node {
        struct stack_node *next;
        int x;
        int y;
    };
    struct stack_node *stack_head = NULL; // head node of the stack
    
    // temp variables
    int temp_xval=0, temp_yval=0;
    int success = 1;
    while(success)
    {
        // Get the next two numbers from the file and use them as the
        // x, y values of the next node. If There are no more numbers in the 
        // file or if the count of numbers in the file is odd, success will be
        // false. Otherwise success is true.
        success = scanf("%d", &temp_xval) >= 0;
        success &= scanf("%d", &temp_yval) >= 0;
        
        // We've loaded two values from the file so add them to the stack
        if (success)
        {
            // Declare the next node
            struct stack_node *next_node = (struct stack_node *) malloc(sizeof(struct stack_node));
            next_node->x = temp_xval;
            next_node->y = temp_yval;
            next_node->next = stack_head;
            stack_head = next_node;
            num_nodes++;
        }
    }
    
    // Initialize an array to store all of the x,y values from the stack
    // so I can use random access.
    int nodevals[num_nodes*2];
    struct stack_node *temp_node = NULL; 
    
    // Go through the stack and store the values into an array,
    // pop, and deallocate
    for(i = 2*num_nodes-1; stack_head != NULL; i-=2)
    {
        nodevals[i-1] = stack_head->x;
        nodevals[i] = stack_head->y;
        temp_node = stack_head->next;
        free(stack_head);
        stack_head = temp_node;
    }
    
    // Allocate memory for 2D distances array of size num_nodes x num_nodes.
    dists = (int **) malloc(num_nodes * sizeof(int *));
    
    for(i = 0; i < num_nodes; i++)
    {
        dists[i] = (int *) malloc(num_nodes * sizeof(int));
    }

    // Calculate the distance between every combination of two cities.
    for (i = 0; i < num_nodes; i++) {
        for (j = 0; j < num_nodes; j++) {
            int dx = nodevals[i*2] - nodevals[j*2];
            int dy = nodevals[i*2+1] - nodevals[j*2+1];
            dists[i][j] = (int)sqrt((float)(dx*dx + dy*dy));
        }
    }
}
/**
 * Initialize the min_path variable with a random starting path 
 * for the round trip. Also calculates and assigns the length for that
 * path into min_length.
 */
void init_path() {
    // Allocate min_path to the appropriate size
    min_path = (int *) malloc(num_nodes * sizeof(int));
    
    // Assign a random starting path
    
    int rand_path_help[num_nodes]; // used to mark which nodes have been traveled to
    int i, j, nodes_left = num_nodes, temp;
    
    // Say that no paths have been traveled to
    for(i = 0; i < num_nodes; i++) {
        rand_path_help[i] = 0;
    }
    
    // Go through each index of the min_path and assign a random node to travel to
    for(i = 0; i < num_nodes; i++)
    {
        temp = rand_int(nodes_left) + 1;
        // find the temp'th node that has not been traveled to
        for(j = 0; temp > 0 && j < num_nodes; j++)
        {
            if (rand_path_help[j] == 0) {
                temp--;
            }
        }
        // the loop above must execute at least once since temp will have
        // a minimum value of 1. So 1 <= j <= num_nodes.
        
        rand_path_help[j-1] = 1; // mark the j-1'th node as traveled to
        min_path[i] = j-1; // travel to that node next
        nodes_left--;
    }
    
    // Assign the length
    min_len = find_path_len(min_path);
    last_len = min_len;
}

/**
 * Will compare the global path length (min_len) with the given length.
 * If the given length is higher (worse), the given path is updated
 * with the global path and the given length (by reference) is updated to be
 * min_len. This method uses a mutex to make it synchronized. 
 * @param path The path to override if length is greater than min_len
 * @param length The length of the given path.
 * @return 1 if the given path was overridden. Otherwise 0.
 */
int compare_and_copy_bpath(int path[], int *length) {
    int flag_success = 0;
    pthread_mutex_lock(&mutex);
    if (min_len < *length) {
        int i;
        for (i = 0; i < num_nodes; i++) {
            path[i] = min_path[i];
        }
        *length = min_len;
        flag_success = 1;
    }
    pthread_mutex_unlock(&mutex);
    return flag_success;
}

/**
 * Will compare the global path length (min_len) with the given length.
 * If the given length is lower (better), the given path is copied
 * to be the global path and the given length (by reference) is copied to be
 * min_len. This method uses a mutex to make it synchronized. 
 * @param path The path to copy if it's better than the global version
 * @param length The length of the given path.
 */
void compare_and_update_bpath(int path[], int length) {
    pthread_mutex_lock(&mutex);
    if (length < min_len) {
        int i;
        for (i = 0; i < num_nodes; i++) {
            min_path[i] = path[i];
        }
        // Just to be safe, instead of using the given length.
        min_len = find_path_len(min_path);
        
        // Print the path if there has been 'significant' improvement
        if (last_len - min_len >= 1000)
        {
            last_len = min_len;
            print_path(min_path, min_len);
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Will generate and return a random integer in the range [0, n)
 */
int rand_int(int n) {
    return rand() % n;
}

/**
 * Helper method. Prints the length and path given.
 */
void print_path(int path[], int length) {
    printf("%d.", length);
    int i;
    for (i = 0; i < num_nodes; i++) {
        printf(" %d", path[i]);
    }
    printf("\n");
}

/**
 * Helper method for finding the length of the given path.
 * @param path The path to find the length of
 * @return The length of the given path
 */
int find_path_len(int path[]) {
    int sum = 0, a, b, i;
    for (i = 0; i < num_nodes; i++) {
        a = path[i];
        b = path[(i + 1) % num_nodes];
        sum += dists[a][b];
    }
    return sum;
}

/**
 * Helper method for switching the values of path at the given indexes.
 * So the values at path[index1] and path[index2] are switched.
 * @param path The path to switch values in
 * @param index1 The index of the first value to switch
 * @param index2 The index of the second value to switch
 */
void switch_pvalues(int path[], int index1, int index2)
{
    int temp = path[index1];
    path[index1]=path[index2];
    path[index2]=temp;
}
