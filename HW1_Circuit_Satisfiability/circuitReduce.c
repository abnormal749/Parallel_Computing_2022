/*  HW1 by Matthew Chen
 *  ID: Q36091334
 */

#include <stdio.h>     // printf()
#include <limits.h>    // UINT_MAX
#include "mpi.h"

int checkCircuit (int, int);

int main (int argc, char *argv[]) {
   int i;                                  /* loop variable (32 bits) */
   int id = 0;                             /* process id */
   int localCount = 0, globalCount = 0;    /* number of solutions */
   int numprocs;
   double startTime = 0.0, totalTime;
   int    namelen;
   char   processor_name[MPI_MAX_PROCESSOR_NAME];

   MPI_Init(&argc,&argv);
   MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
   MPI_Comm_rank(MPI_COMM_WORLD,&id);
   MPI_Get_processor_name(processor_name,&namelen);

   //MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

   if (id == 0)
        startTime = MPI_Wtime();

   int portion = (int) USHRT_MAX/numprocs;
   for (i = id*portion; i <= (id+1)*portion; i++) {
      localCount += checkCircuit (id, i);
   }
   printf ("Process %d finished.\n", id);
   fflush (stdout);
   MPI_Reduce(&localCount, &globalCount, 1, MPI_INTEGER, MPI_SUM, 0, MPI_COMM_WORLD);

   if (id == 0){
      totalTime = MPI_Wtime() - startTime;
      printf("CircuitSAT finished in time %f secs.\n", totalTime);
      printf("A total of %d solutions were found.\n\n", globalCount);
      //printf("%d %f %d" numprocs, totalTime, globalCount);
      fflush(stdout);
   }

   MPI_Finalize();
   return 0;
}


#define EXTRACT_BIT(n,i) ( (n & (1<<i) ) ? 1 : 0)
#define SIZE 16

int checkCircuit (int id, int bits) {
   int v[SIZE];        /* Each element is a bit of bits */
   int i;

   for (i = 0; i < SIZE; i++)
     v[i] = EXTRACT_BIT(bits,i);

   if (  (v[0] || v[1]) && (!v[1] || !v[3]) && (v[2] || v[3])
       && (!v[3] || !v[4]) && (v[4] || !v[5])
       && (v[5] || !v[6]) && (v[5] || v[6])
       && (v[6] || !v[15]) && (v[7] || !v[8])
       && (!v[7] || !v[13]) && (v[8] || v[9])
       && (v[8] || !v[9]) && (!v[9] || !v[10])
       && (v[9] || v[11]) && (v[10] || v[11])
       && (v[12] || v[13]) && (v[13] || !v[14])
       && (v[14] || v[15])  )
   {
      printf ("%d) %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d \n", id,
         v[15],v[14],v[13],v[12],
         v[11],v[10],v[9],v[8],v[7],v[6],v[5],v[4],v[3],v[2],v[1],v[0]);
      fflush (stdout);
      return 1;
   } else {
      return 0;
   }
}

