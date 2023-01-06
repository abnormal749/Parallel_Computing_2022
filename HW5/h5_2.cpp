#include <iostream>
#include <dirent.h>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <chrono>
#include <omp.h>


using namespace std;

/* KEYWORDS, FILES(IN DIRECTORY), MAX OF LINE*/
const char* keywords_f = "keywords.txt";
const char* files_d = "file";

#define MAX_L 1000
#define KEYW_SIZE 50

/* TOKENIZE */
void get_keywords_list(vector<string> &keyw, map<string,int>& k2i);
void get_document_list(vector<string> &flist);

int main(int argc, char* argv[]){
    //GET FILES LIST IN DIRECTORY
    vector<string> keywords;
    vector<string> file_list;
    map<string, int> keyw_to_index;
    get_keywords_list(keywords, keyw_to_index);
    get_document_list(file_list);
    
    
    //SHARE VARIABLES ACROSS THREADS
    int head_of_filelist = 0;
    int keyw_size = keywords.size();
    int flist_size = file_list.size();
    queue<string> queue_of_lines;
    omp_lock_t lock_of_queue;
    int queue_size = 0;
    int kw_number[KEYW_SIZE];
    
    //TIMER START
    auto startwtime = std::chrono::high_resolution_clock::now();
    
# pragma omp parallel shared(head_of_filelist, lock_of_queue, queue_size)
    {
        int thread_count = omp_get_num_threads();
        int my_rank = omp_get_thread_num();
        if(my_rank%2 == 0){
            //PRODUCOR KEEP READING FILES
            while(head_of_filelist<flist_size){
                int cur_file;
                # pragma omp critical
                {
                    cur_file = head_of_filelist++;
                    cout << my_rank << ": GET THE FILE " << cur_file << endl;
                }
                
                //PUT LINES INTO QUEUE
                string filename = "./" +file_list[cur_file];
                cout << my_rank << ": " << filename << endl;
                ifstream in(filename);
                for (string line; getline( in, line );){
                    cout << my_rank << ": " << line << endl;
                    omp_set_lock(&lock_of_queue);
                    queue_of_lines.push(line);
                    queue_size++;
                    omp_unset_lock(&lock_of_queue);
                }
            }
            cout << my_rank << ": FINISH PRODUCING\n";
        }else{
            cout << my_rank << ": START CONSUMING\n";
            //CONSUMER KEEP GETTING FROM QUEUE
            while(head_of_filelist<flist_size){
                if(!queue_size){
                    omp_set_lock(&lock_of_queue);
                    string untoken_str = queue_of_lines.front();
                    queue_of_lines.pop();
                    queue_size--;
                    omp_unset_lock(&lock_of_queue);
                    char *cstr = (char*) untoken_str.c_str();
                    cout << my_rank << untoken_str << endl;
                    
                    //TOKENIZE WORDS AND MATCH
                    int temp_keyw[KEYW_SIZE] = {0};
                    char* cut_str = strtok ( cstr , " -,\t\n");
                    while ( cut_str ){
                        string temp_str = temp_str;
                        if(keyw_to_index.find(temp_str) == keyw_to_index.end()){
                            temp_keyw[keyw_to_index[temp_str]]++;
                        }
                        cut_str = strtok ( NULL , " -,\t\n");
                    }
                    for(int i=0; i<KEYW_SIZE; i++){
                        if(temp_keyw[i]!=0){
                            # pragma omp atomic
                            kw_number[i]+=temp_keyw[i];
                        }
                    }
                }
            }
            cout << my_rank << ": FINISH CONSUMING\n";
        }
        
    }
    
    //TIMER STOP
    auto endwtime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = endwtime - startwtime;
    cout << "The execution time = %d seconds." << elapsed_seconds.count() << " seconds." <<endl;
    
    return 0;
}

/* GET THE LIST OF KEYWORDS */
void get_keywords_list(vector<string> &keyw, map<string,int>& k2i){
    ifstream in( keywords_f );
    int index = 0;
    for (string line; getline( in, line );){
        stringstream ss( line );
        for (string d; ss >> d; index++){
            keyw.push_back(d);
            k2i[d] = index;
        }
    }
}

/* GET THE LIST OF DOCUMENTS */
void get_document_list(vector<string> &flist){
    DIR *d;
    struct dirent *dir;
    d = opendir(files_d);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(strlen(dir->d_name)>=5 && !strcmp(&dir->d_name[strlen(dir->d_name)-4],".txt"))
                flist.push_back((string)dir->d_name);
        }
        closedir(d);
    }else{
        printf("DIRECTORY NO EXISTS.\n");
        exit(1);
    }
}
