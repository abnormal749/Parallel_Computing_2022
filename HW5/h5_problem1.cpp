#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <chrono>

#define ARRAY_SIZE 10000
int thread_count;

using namespace std;

/* DATA STRUCTURE THAT SEND TO FUNCTIONS */
typedef struct{
    int* data;
    int size;
    int rank;
    int* result;
}args;

/* READ IN DATA FROM FILE */
using TYPE = int;
template< typename T > T* loadtxt( const char * filename ){
    T* data = new T[ARRAY_SIZE];
    ifstream in( filename );
    int i = 0;
    for (string line; getline( in, line ) && i<ARRAY_SIZE; i++){
        stringstream ss( line );
        T d;
        for ( T d; ss >> d; i++)
            data[i] = d;
    }
    return data;
}


/* COUNT SORT */
void* Count_sort(void* inputs) {
    int* a = ((args*)inputs)->data;
    int n = ((args*)inputs)->size;
    long local_rank = (long) ((args*)inputs)->rank;
    int* result = ((args*)inputs)->result;
    
    
    for (int i = (n*local_rank)/thread_count; i < (n*(local_rank+1))/thread_count; i++) {
        int count = 0;
        for (int j = 0; j < n; j++)
            if (a[j] < a[i])
                count++;
            else if (a[j] == a[i] && j < i)
                count++;
        result[count] = a[i];
    }
    return NULL;
}


int main(int argc, char* argv[]){
    const char* infileName = "input.txt";
    int* input = loadtxt<int>(infileName);
    int* result = (int*) malloc(ARRAY_SIZE*sizeof(int));
    
    //CREATE THREAD LIST & ITEMS
    thread_count = strtol(argv[1],NULL,10);
    pthread_t *  thread_handles;
    thread_handles = (pthread_t*)malloc(thread_count*sizeof(pthread_t));
    
    //TIMER START
    auto startwtime = std::chrono::high_resolution_clock::now();
    
    //ACTIVATE THREADS
    for(long i=0; i<thread_count; i++){
        args* info = new args;
        info->data = input;
        info->size = ARRAY_SIZE;
        info->rank = i;
        info->result = result;
        pthread_create(&thread_handles[i], NULL, Count_sort, (void *) info);
    }
    
    //WAIT ALL FINISH
    for (int i = 0; i < thread_count; i++)
        pthread_join(thread_handles[i], NULL);
    
    memcpy(input, result, ARRAY_SIZE*sizeof(int));
    
    //TIMER FINISH
    auto endwtime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = endwtime - startwtime;
    cout << "The execution time = %d seconds." << elapsed_seconds.count() << " seconds." <<endl;
    
    //RELEASE MEMORY
    free(thread_handles);
    free(input);
    
    return 0;
}
