#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <chrono>

#define ARRAY_SIZE = 10000

using namespace std;

/* COUNT SORT */
void Count_sort(int a[], int n) {
    int i, j, count;
    int* temp = malloc(n*sizeof(int));
    for (i = 0; i < n; i++) {
        count = 0;
        for (j = 0; j < n; j++)
            if (a[j] < a[i]) count++;
            else if (a[j] == a[i] && j < i)
                count++;
        temp[count] = a[i];
    }
    memcpy(a, temp, n*sizeof(int));
    free(temp);
}

int main(int argc, char* argv[]){
    //SERIAL WAY
    int input[ARRAY_SIZE];
    auto startwtime = std::chrono::high_resolution_clock::now();
    
    Count_sort(input,ARRAY_SIZE);
    
    auto endwtime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = endwtime - startwtime;
    cout << "The execution time = %d seconds." << elapsed_seconds.count() << " seconds." <<endl;
    
    
    return 0;
}
