#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <CL/cl.h>

#include "definitions.h"
#include "error.h"

#define MAX_SOURCE_SIZE (0x100000)
#define CLUSTER_ROWS 50 // Constant number of rows for the cluster structure
#define CLUSTER_COLS 50 // Constant number of columns for the cluster structure

#define NUM_VELO 48     // Total number of VELO detectors

#define VELO_MIN -40.0
#define VELO_MAX 40.0

typedef struct cluster {
    int position;       // First index in the hits structure where the elements start
    int num_elems;      // Number of elements inside this cluster
} cluster;

typedef struct hit_str {
    float x;
    float y;
    int z;
    //int id;   // Is the ID necessary during the calculation?
} hit_str;

struct link {
    int index;
    struct link* next;
}link;

typedef struct downup_str {
    int down;
    int up;
} downup_str;


cluster grid[NUM_VELO][CLUSTER_ROWS][CLUSTER_COLS];

downup_str *downup;
hit_str *hit_pos;

int* h_no_sensors;
int* h_no_hits;
int* h_sensor_Zs;
int* h_sensor_hitStarts;
int* h_sensor_hitNums;
int* h_hit_IDs;
float* h_hit_Xs;
float* h_hit_Ys;
int* h_hit_Zs;

/** Read the dump file that contains the tracking information.
 *
 * @param filename route to the dump file
 * @param input char vector pointer that will hold the information of the file
 * @param size holds the size of the file
 */
void readFile(char* filename, char** input, int* size){
	
    FILE *fd;
    

    fd = fopen(filename, "rb");
    if(fd == NULL) {
        fprintf(stderr,"The file %s cannot be opened\n",filename);
        exit(-1);
    }
    
    fseek(fd, 0, SEEK_END);
    *size = ftell(fd);       // Get the size of the file
    
    rewind(fd);
    *input = (char*)malloc(*size * (sizeof(char)));
    fread(*input, sizeof(char), *size, fd);     //read the content
    fclose(fd);

	h_no_sensors = (int*) input[0];
	h_no_hits = (int*) (h_no_sensors + 1);
	h_sensor_Zs = (int*) (h_no_hits + 1);
	h_sensor_hitStarts = (int*) (h_sensor_Zs + h_no_sensors[0]);
	h_sensor_hitNums = (int*) (h_sensor_hitStarts + h_no_sensors[0]);
	h_hit_IDs = (int*) (h_sensor_hitNums + h_no_sensors[0]);
	h_hit_Xs = (float*) (h_hit_IDs + h_no_hits[0]);
	h_hit_Ys = (float*) (h_hit_Xs + h_no_hits[0]);
	h_hit_Zs = (int*) (h_hit_Ys + h_no_hits[0]);
}

/** 
 * Sort hits according the the position inside the cluster.
 * Save the 
 * This approach cannot be used inside the Device because function malloc cannot
 * be used inside.
 */
void sortHits() {

    int i, j, k;
    int row, col;   // Calculated row and column
    int count;  // counter of the number of elements of the hits of a panel
    int hit = 0;

    int index = 0;
    
    // Temporal structure to sort the hits
    struct str_cluster{
        int row;    // index of the row in the final cluster structer
        int col;  // index of the column in the final cluster structer
        int hit_index;  // Save the index to acces later
        int z;
        struct str_cluster *next;
    };
    
    struct str_cluster *first, *new, *current, *previous, *next;
    
    
    float cluster_row_size = (-VELO_MIN + VELO_MAX) / CLUSTER_ROWS;
    float cluster_col_size = (-VELO_MIN + VELO_MAX) / CLUSTER_COLS;
    
    
    
    // Iterate over the sensors
    for(i=0; i < *h_no_sensors; i++) {

        //printf("\n\n=======Sensor %d=============\n",i);
        
        // Create a list to sort the elements of the current panel
        
        count = 1;      // We didn't use any element of the list, still
        
        // Treat the element 0
        first = (struct str_cluster*) malloc( sizeof(struct str_cluster));

        // calculate the row and column inside the grid
        row = (int) ((-VELO_MIN + h_hit_Xs[hit])/cluster_row_size);
        col = (int) ((-VELO_MIN + h_hit_Ys[hit])/cluster_col_size);
        
        first->next = NULL;
        first->row = row;
        first->col = col;
        first->hit_index = hit;
        first->z = i;
        
        hit++;

        // Iterate over the hits of the sensor
        for(j=1; j < h_sensor_hitNums[i]; j++, hit++) {

            new = (struct str_cluster*) malloc( sizeof(struct str_cluster));

            // calculate the row and column inside the grid
            row = (int) ((-VELO_MIN + h_hit_Xs[hit])/cluster_row_size);
            col = (int) ((-VELO_MIN + h_hit_Ys[hit])/cluster_col_size);
        
            new->row = row;
            new->col = col;
            new->hit_index = hit;
            new->z = i;
            new->next = NULL;
    
            //printf("%d: (%d, %d)\n",j,row, col);
            
            previous = NULL;
            current = first;    // We are going to iterate over the list starting from the first element
            
            // Add the sruct inside the linked list
            // sort the elements in a bidimensional space
            for(k=0; k < count; k++) {

                if(current == first)  {
                    if((first->row * CLUSTER_COLS + first->col) >= (row * CLUSTER_COLS + col )) {
                        //printf("\t(%d,%d) is more than (%d,%d), ID: %d\n", first->row, first->col,row, col, h_hit_IDs[j]);
                        new->next = current;
                        first = new;
                        //printf("\nA) New first %d\n",h_hit_IDs[j]);
                        break;
                    }
                } else {
                    if((current->row * CLUSTER_COLS + current->col) > (row * CLUSTER_COLS + col )) {
                        //printf("\t(%d,%d) is more than (%d,%d), ID: %d\n", current->row, current->col,row, col, h_hit_IDs[j]);

                        new->next = current;
                        if(current == first) {
                            //printf("\nB) New first %d\n",h_hit_IDs[j]);
                            new = first;
                        } else {
                            previous->next = new;
                        }
                        break;  // Go out of the loop
                    }
                } 
                
                if(k == count - 1) {
                    current->next = new;
                }
                
                previous = current;
                current = current->next;
            }
            count++;
        }
        
        current = first;
        while(current != NULL) {
            hit_pos[index].x = h_hit_Xs[current->hit_index];
            hit_pos[index].y = h_hit_Ys[current->hit_index];
            hit_pos[index].z = h_hit_Zs[current->hit_index];
            
            if(grid[current->z][current->row][current->col].position == -1) {
                grid[current->z][current->row][current->col].position = index;
            }
            
            grid[current->z][current->row][current->col].num_elems++;
            
            
            index++;
            current = current->next;
        }
        


        current = first;
        printf("\n\n------------------------------\n");
        while(current != NULL) {
            printf("\tFREE: (%d,%d) %d\n",current->row, current->col, current->hit_index);
            next = current->next;
            free(current);
            current = next;
        }
        printf("------------------------------\n\n");
    }
}

void printDumpHeads() {
    printf("no_hits %d\n",*h_no_hits);
    printf("h_no_sensors %d\n",*h_no_sensors);
    printf("no_hits %d\n",*h_no_hits);
    printf("sensor_Zs %d\n",h_sensor_Zs[0]);
    printf("sensor_hitStarts %d\n",h_sensor_hitStarts[0]);
    printf("sensor_hitNums %d\n",h_sensor_hitNums[0]);
    printf("hit_IDs %d\n",h_hit_IDs[0]);
    printf("hit_Xs %f\n",h_hit_Xs[0]);
    printf("hit_Ys %f\n",h_hit_Ys[0]);
    printf("hit_Zs %d\n",h_hit_Zs[0]);
}

void printGrid() {

    int i, j, k;

     
    for(i=0; i < NUM_VELO; i++) {
        for(j=0; j < CLUSTER_ROWS; j++) {
            for(k=0; k < CLUSTER_COLS; k++) {
                printf("(%d,%d,%d) pos: %d elems: %d\n",i,j,k,grid[i][j][k].position,grid[i][j][k].num_elems);
            }
        }
        printf("\n");
    }
}


void printDataDump() {

    int i, j;
    int id =0;
    
    printf("========No sensors: %d, No hits %d==========\n",*h_no_sensors,*h_no_hits);
    for(i=0; i < *h_no_sensors; i++) {
        printf("\t---------Num hits sensor %d: %d, hits start: %d ------------\n",i,h_sensor_hitNums[i], h_sensor_hitStarts[i]);
        for(j=0; j< h_sensor_hitNums[i]; j++, id++) {
            printf("\t\t(%f,%f,%d) - %d\n",h_hit_Xs[id],h_hit_Ys[id],h_hit_Zs[id],h_hit_IDs[id]);
        }
    }
}

/** Load the collision from a dump file.
 * This function will be modified as the read operations will be diferent in 
 * production.
 * 
 */
void loadCollision(int* size) {

    // TODO this file is hardcoded for development purposes. Change this
	//char* dumpFile = "../dump/pixel-sft-event-0.dump"; // Dump file name
	char* dumpFile = "../dump/collision_straight.dump"; // Dump file name
	//int size;                           // Size of the dump file
	char* input;
	readFile(dumpFile,&input, size);

    printf("Size %d\n",*size);
}

void checkError(int retValue, char *msg) {
    if(retValue != 0) {
        printErrorByCode(retValue);
        fprintf(stderr, "Error %d: %s\n",retValue, msg);
        exit(-1);
    }
}

int gpuLoad(void) {
    // Create the two input vectors
    int i;
    const int LIST_SIZE = *h_no_hits;

    float *A = (float*)malloc(sizeof(float)*LIST_SIZE); // TODO remove
    
    int *start_hit = (int*)malloc(sizeof(int)*LIST_SIZE);
    
    // Load the kernel source code into the array source_str
    FILE *fp;
    char *source_str;
    size_t source_size;
 
    fp = fopen("kernel.cl", "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }
    source_str = (char*)malloc(MAX_SOURCE_SIZE);
    source_size = fread( source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
 
    // Get platform and device information
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;   
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_DEFAULT, 1, 
            &device_id, &ret_num_devices);

    checkError(ret, "Get device ID");
    // Create an OpenCL context
    cl_context context = clCreateContext( NULL, 1, &device_id, NULL, NULL, &ret);
 
    // Create a command queue
    cl_command_queue command_queue = clCreateCommandQueue(context, device_id, 0, &ret);


    // Grid structure
    cl_mem grid_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, 
            NUM_VELO*CLUSTER_ROWS*CLUSTER_COLS * sizeof(cluster), NULL, &ret);
    // Hit structure
    cl_mem hits_obj = clCreateBuffer(context, CL_MEM_READ_ONLY,
            *h_no_hits * sizeof(hit_str), NULL, &ret);
    // Down-up structure
    cl_mem downup_obj = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
            *h_no_hits * sizeof(downup_str), NULL, &ret);
    // Z list structure
    cl_mem zlist_obj = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
            *h_no_sensors * sizeof(int), NULL, &ret);
    // A debug object TODO remove
    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
            LIST_SIZE * sizeof(float), NULL, &ret);
    // Start hits array
    cl_mem start_hit_obj = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
            LIST_SIZE * sizeof(int), NULL, &ret);


    // Copy the lists grid and hit structure to their respective memory buffers
    ret = clEnqueueWriteBuffer(command_queue, grid_obj, CL_TRUE, 0,
            NUM_VELO*CLUSTER_ROWS*CLUSTER_COLS * sizeof(cluster), grid, 0, NULL, NULL);
    ret |= clEnqueueWriteBuffer(command_queue, hits_obj, CL_TRUE, 0, 
            *h_no_hits * sizeof(hit_str), hit_pos, 0, NULL, NULL);
    ret |= clEnqueueWriteBuffer(command_queue, zlist_obj, CL_TRUE, 0, 
            *h_no_sensors * sizeof(int), h_sensor_Zs, 0, NULL, NULL);

    checkError(ret, "Copying from device to host");
    
    // Create a program from the kernel source
    cl_program program = clCreateProgramWithSource(context, 1, 
            (const char **)&source_str, (const size_t *)&source_size, &ret);

    // Build the program
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    
    if (ret == CL_BUILD_PROGRAM_FAILURE) {
        // Determine the size of the log
        size_t log_size;
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        // Allocate memory for the log
        char *log = (char *) malloc(log_size);

        // Get the log
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

        // Print the log
        printf("%s\n", log);
    }
    
    checkError(ret, "Building program");
    // ------------Begin second kernel (FINDER)---------------------
    
    // Create the OpenCL kernel
    cl_kernel finder_kernel = clCreateKernel(program, "NeighborsFinder", &ret);
 
    // Set the arguments of the kernel
    ret = clSetKernelArg(finder_kernel, 0, sizeof(cl_mem), (void *)&grid_obj);
    ret |= clSetKernelArg(finder_kernel, 1, sizeof(cl_mem), (void *)&hits_obj);
    ret |= clSetKernelArg(finder_kernel, 2, sizeof(cl_mem), (void *)&downup_obj);
    ret |= clSetKernelArg(finder_kernel, 3, sizeof(cl_mem), (void *)&zlist_obj);
    ret |= clSetKernelArg(finder_kernel, 4, sizeof(int), (void *)h_no_sensors);
    ret |= clSetKernelArg(finder_kernel, 5, sizeof(int), (void *)h_no_hits);
    ret |= clSetKernelArg(finder_kernel, 6, sizeof(cl_mem), (void *)&a_mem_obj);   // TODO remove
 
    checkError(ret, "Setting the arguments of the function");
    // ------------ End second kernel (FINDER)---------------------


    // ------------Begin second kernel (CLEANER)---------------------
        
    // Create the OpenCL kernel
    cl_kernel cleaner_kernel = clCreateKernel(program, "NeighborsCleaner", &ret);
 
    // Set the arguments of the kernel
    ret = clSetKernelArg(cleaner_kernel, 0, sizeof(cl_mem), (void *)&downup_obj);
    ret |= clSetKernelArg(cleaner_kernel, 1, sizeof(int), (void *)h_no_hits);
 
    checkError(ret, "Setting the arguments of the function");
    // ------------ End second kernel (CLEANER)---------------------


    // ------------Begin second kernel (START HITS)---------------------
 
    checkError(ret, "Building program");
    
    // Create the OpenCL kernel
    cl_kernel start_hit_kernel = clCreateKernel(program, "StartHitsFinder", &ret);
 
    // Set the arguments of the kernel
    ret = clSetKernelArg(start_hit_kernel, 0, sizeof(cl_mem), (void *)&downup_obj);
    ret |= clSetKernelArg(start_hit_kernel, 1, sizeof(cl_mem), (void *)&start_hit_obj);
    ret |= clSetKernelArg(start_hit_kernel, 2, sizeof(int), (void *)h_no_sensors);
 
    checkError(ret, "Setting the arguments of the function");
    // ------------ End second kernel (START HITS)---------------------

    /* TODO the number of threads doesn't has to be hardcoded.
       the number of threads has to be related to the geometry, the number
       of devices and the size of the grid.
    */
    // Execute the OpenCL kernel on the list
    size_t global_item_size = 2048; // Process the entire lists
    size_t local_item_size = 64; // Divide work items into groups of 64
    // Execute neighbour finder kernel
    ret = clEnqueueNDRangeKernel(command_queue, finder_kernel, 1, NULL, 
            &global_item_size, &local_item_size, 0, NULL, NULL);
    // Execute cleaner kernel
    ret |= clEnqueueNDRangeKernel(command_queue, cleaner_kernel, 1, NULL, 
            &global_item_size, &local_item_size, 0, NULL, NULL);
    // Execute start hit kernel
    ret |= clEnqueueNDRangeKernel(command_queue, start_hit_kernel, 1, NULL, 
            &global_item_size, &local_item_size, 0, NULL, NULL);

    checkError(ret, "Enqueue ND Range Kernel"); 
    // Read the memory buffer C on the device to the local variable C
    float *C = (float*)malloc(sizeof(float)*LIST_SIZE);
    ret = clEnqueueReadBuffer(command_queue, downup_obj, CL_TRUE, 0, 
            *h_no_hits * sizeof(downup_str), downup, 0, NULL, NULL);
    ret |= clEnqueueReadBuffer(command_queue, a_mem_obj, CL_TRUE, 0, 
            *h_no_hits * sizeof(float), A, 0, NULL, NULL);  // TODO remove
    ret |= clEnqueueReadBuffer(command_queue, start_hit_obj, CL_TRUE, 0, 
            *h_no_hits * sizeof(int), start_hit, 0, NULL, NULL);

    checkError(ret, "Enqueue read buffer");
    
    // Display the result to the screen
    for(i = 0; i < *h_no_hits; i++) {
        printf("%d (%d,%d) A(%f) SH(%d)\n",i,downup[i].down, downup[i].up,A[i],start_hit[i]);
    }
    
    // Clean up
    ret = clFlush(command_queue);
    ret |= clFinish(command_queue);
    ret |= clReleaseKernel(finder_kernel);
    ret |= clReleaseKernel(cleaner_kernel);
    ret |= clReleaseKernel(start_hit_kernel);
    ret |= clReleaseProgram(program);
    ret |= clReleaseMemObject(grid_obj);
    ret |= clReleaseMemObject(hits_obj);
    ret |= clReleaseMemObject(downup_obj);
    ret |= clReleaseMemObject(a_mem_obj); // TODO remove
    ret |= clReleaseMemObject(start_hit_obj);
    ret |= clReleaseMemObject(zlist_obj);
    ret |= clReleaseCommandQueue(command_queue);
    ret |= clReleaseContext(context);

    checkError(ret, "Releasing variables");

    free(A);
    
    free(C);
    return 0;
}


int main() {
    
    int i, j, k;
    
    // TODO consider initialization with memset
    for(i=0; i < NUM_VELO; i++) {
        for(j=0; j < CLUSTER_ROWS; j++) {
            for(k=0; k < CLUSTER_COLS; k++) {
                grid[i][j][k].position = -1;
                grid[i][j][k].num_elems = 0;
            }
        }
    }

	int size;                           // Size of the dump file
	loadCollision(&size);
    
    hit_pos = (hit_str*) malloc(*h_no_hits * sizeof(hit_str));
    downup = (downup_str*) malloc(*h_no_hits * sizeof(downup_str));
    
    sortHits();

    //printDataDump();
    //printGrid();

    gpuLoad();

    return 0;
}
