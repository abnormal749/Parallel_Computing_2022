#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <random>
#include <vector>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <omp.h>
#include <mpi.h>

#define NGENERATION 100000000
#define ALPHA 2
#define BETA 5
#define RHO 0.2

int Q  = 2000.0;
int num_city;
using namespace std;

class Ant {
public:
    int curr_city;
    int path_length = 0;
    vector<int> tour;
    vector<bool> is_visited;
    Ant(){
        curr_city = rand() % num_city;
        tour.push_back(curr_city);
        is_visited.resize(num_city, false);
        is_visited[curr_city] = true;
    }
};

int main(int argc, char* argv[]){
    int comm_sz, my_rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    int path_length[10000];
    int count = 0;

    // READ IN DATA FROM FILE
    if(my_rank == 0){
        ifstream in((const char *)argv[1] );
        for (string line; getline( in, line );){
            stringstream ss( line );
            for(int d; ss >> d;)
                path_length[count++] = d;
        }
        in.close();
    }


    MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&path_length, count, MPI_INT, 0, MPI_COMM_WORLD);

    num_city = sqrt(count);
    int num_ant = num_city*1.5;
    
    vector<vector<int>> distance(num_city, vector<int>(num_city));
    vector<vector<double>> phero_Matrix(num_city, vector<double>(num_city, 1.0));

    int min_length = INT_MAX;
    int *local_min_tour = (int*)malloc(num_city * sizeof(int));


    for(int i = 0; i < num_city; i++){
        for(int j = 0; j < num_city; j++){
            distance[i][j] = path_length[i*num_city + j];
        }
    }

    // TIMER START ---------------------------------------------------------------------------------------------
    double start = MPI_Wtime();
    srand(time(NULL));

    for(int t = 0; t < NGENERATION/comm_sz; t++){
        vector<Ant> ants(num_ant);

        for(int i = 0; i < num_city - 1; i++){
            #pragma omp for shared(phero_Matrix,distance)
            for(int k = 0; k < num_ant; k++){
                double Pij_arr[num_city]; // PROBABILITY i to j
                double nij_arr[num_city]; // HEURISTIC VALUE
                double tn_arr[num_city];  // tij

                // nij
                for(int j = 0; j < num_city; j++){
                    nij_arr[j] = 0.0;
                    if(!ants[k].is_visited[j] && ants[k].curr_city != j)
                        nij_arr[j] = 1.0 / distance[ants[k].curr_city][j];
                }

                // tn
                double tn_sum = 0.0;
                for(int j = 0; j < num_city; j++){
                    double tij = 1.0;
                    tij = pow(tij, ALPHA);
                    double nij = 1.0;
                    nij = pow(nij, BETA);
                    tn_arr[j] = tij*nij;
                    if(!ants[k].is_visited[j])
                        tn_sum += tn_arr[j];
                }

                // Pij
                for(int j = 0; j < num_city; j++){
                    Pij_arr[j] = 0.0;
                    if(tn_sum != 0 && !ants[k].is_visited[j])
                        Pij_arr[j] = tn_arr[j] / tn_sum;
                }
                
                // Roulette Wheel Probability
                random_device rd;
                mt19937 gen(rd());
                uniform_real_distribution<> dist(0, 1);
                double rnd = dist(gen);
                double curr_Prob = 0.0;
                int next = -1;
                for(int j = 0; j < num_city; j++){
                    curr_Prob += Pij_arr[j];
                    if(rnd <= curr_Prob){
                        next = j;
                        break;
                    }
                }
                if(next != -1){
                    ants[k].path_length += distance[ants[k].curr_city][next];
                    ants[k].curr_city = next;
                    ants[k].is_visited[next] = true;
                    ants[k].tour.push_back(next);
                }
            }
        }
        
        #pragma omp for
        for(int k = 0; k < num_ant; k++){
            ants[k].path_length += distance[ants[k].curr_city][ants[k].tour[0]];
            ants[k].curr_city = ants[k].tour[0];
        }

        // UPDATE Lenght L*
        #pragma omp for
        for(int k = 0; k < num_ant; k++){
            if(ants[k].path_length >= min_length) continue;

            #pragma omp critical
            {
                if(ants[k].path_length < min_length){
                    min_length = ants[k].path_length;
                    for(int x = 0; x < num_city; x++){
                        local_min_tour[x] = ants[k].tour[x];
                    }
                    // PREVIEW RESULT
                    if(t!=0 && t%100000==0){
                        cout << min_length << " ";
                        for(int temp = 0; temp<num_city; temp++)
                            cout << local_min_tour[temp] << " ";
                    cout << "\n";
                   }
                }
             }
        }

        Q = min_length;
        // UPDATE PHEROMONE M. FOR NEXT RUN
        for(auto &v:phero_Matrix){
            for(double &x:v){
                x = (1 - RHO) * x;
            }
        }
        for(int k = 0; k < num_ant; k++){
            if(ants[k].path_length == 0) continue;
            for(int j = 0; j < num_city - 1; j++){
                phero_Matrix[ants[k].tour[j]][ants[k].tour[j+1]] += Q / ants[k].path_length;
            }
            phero_Matrix[ants[k].tour[num_city - 1]][ants[k].tour[0]] += Q / ants[k].path_length;
        }
        ants.clear();
    }
    
    //STATE TYPE
    struct {int cost;int rank;}local_data, global_data;
    
    local_data.cost = min_length;
    local_data.rank = my_rank;

    // GLOBAL MINIMUM & INDEX ATTACHED TO IT
    MPI_Allreduce(&local_data, &global_data, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);

    if(global_data.rank != 0){
        if(my_rank == 0){
            //RECIEVE UPDATE FROM THE GLOBAL MIN
            MPI_Status status;
            MPI_Recv(&local_min_tour[0], num_city, MPI_INT, global_data.rank, 0, MPI_COMM_WORLD, &status);
        }
        else if(my_rank == global_data.rank){
            //PUSH OUR BEST TO NODE 0.
            MPI_Send(&local_min_tour[0], num_city, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }

    // TIMER FINISH ---------------------------------------------------------------------------------------------
    double end = MPI_Wtime();

    // SHOW ANSWER
    if(my_rank == 0){
        cout << "Shortest length: " << global_data.cost << "\n";
        cout << "Path: ";
        for(int i = 0; i < num_city; i++){
            cout << local_min_tour[i] << " ";
        }
        cout<< local_min_tour[0] << '\n';
        cout << "The execution time: " << end - start << " sec\n";
    }

    
    MPI_Finalize();
    return 0;
}
