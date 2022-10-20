/*  HW2 by Matthew Chen Q36091334 */

#include <stdio.h>     // printf()
#include <stdlib.h>
#include <limits.h>    // UINT_MAX
#include <time.h>
#include <stdint.h>
#include "mpi.h"

#define distance_squared(x,y) x*x+y*y

int main (int argc, char *argv[]) {
   int    id,numprocs;
   unsigned n;
   double startTime=0.0, totalTime;
   int    localCount=0, globalCount=0;
   int    namelen;
   char   processor_name[MPI_MAX_PROCESSOR_NAME];

   MPI_Init(&argc,&argv);
   MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
   MPI_Comm_rank(MPI_COMM_WORLD,&id);
   MPI_Get_processor_name(processor_name,&namelen);

   if (id == 0)
        startTime = MPI_Wtime();

   n = 1000000000;        //total number of darts
   MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

   srand(time(NULL));
   unsigned portion = n/numprocs;
   for (unsigned i = 0; i < portion; i++){
       double x,y;
       x = ((double)rand() / (double)RAND_MAX);
       y = ((double)rand() / (double)RAND_MAX);
       if (distance_squared(x,y) <= 1)
           localCount++;
   }

   MPI_Reduce(&localCount, &globalCount, 1, MPI_UNSIGNED , MPI_SUM, 0, MPI_COMM_WORLD);

   if (id == 0){
      totalTime = MPI_Wtime() - startTime;
      printf("CircuitSAT finished in time %f secs.\n", totalTime);
      printf("The solutions were: %.10f\n\n", 4*(globalCount/((double) n)));
      fflush(stdout);
   }

   MPI_Finalize();
   return 0;
}
