#include <iostream>
#include <fstream>
#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <utility>
#include <queue>
#include <cstdlib>

using namespace std;

using Type = priority_queue<int, vector<int>, greater<int>>;
// priority queue pentru retinerea indexului inversat in mod crescator
// dupa ID-ul fisierelor

bool cmp(pair<string, Type> &a, pair<string, Type> &b) { //perechi de forma (cuvant,lista_fisiere)
    if (a.second.size() == b.second.size())
        return a.first > b.first;
    return a.second.size() <= b.second.size();
} //comparator pentru indecsii inversati: ordonam descrescator
//dupa numarul de fisiere in care apare cuvantul sau alfabetic pentru lungimi egale


using PQ = priority_queue<pair<string, Type>, vector<pair<string, Type>>, decltype(&cmp)>;
//un element de tip PQ este de forma (cuvant,lista_id-uri_fisiere)

// rezultatele finale vor fi introduse intr-un priority queue in care prioritatea cea mai mare o are elementul care
// are indexul inversat cel mai lung( se ia alfabetic in caz de egalitate)

struct thread_arg {
    string thread_type; // M sau R
    int thread_id;
    pthread_barrier_t* bariera; // o folosim pentru ca reducerii sa execute taskuri abia dupa mapperi
    int M; // numarul de thread-uri de tip mapper
    vector <vector<map<pair<string, int>, int>> >* dictionar; // vector de dictionare pe litere pentru retinerea listelor partiale
    // in dictionar am elemente de forma dictionar[id_thread][litera][(cuvant,id_fisier)]
    vector<map<string, Type>>* index_invers_unordered; // listele finale pe litere, dar neordonate
    vector<PQ>* pq; // listele finale ordonate corespunzator pe litere
    queue < pair <string,int> >* file_names; // coada dinamica de fisiere pentru mappers
    pthread_mutex_t* queue_mutex; // sa nu existe 2 threaduri sa extraga simultan din cozi
    queue <char>* letters_queue; // coada dinamica de litere pentru reducers
};


void mapper_thread_work(void* arg) {
    struct thread_arg* argument = (struct thread_arg*)arg;
    int file_index;
    while (true) {
        string file_name;

        pthread_mutex_lock(argument->queue_mutex); // doar 1 thread are voie sa extraga
        if (argument->file_names->empty()) {
            pthread_mutex_unlock(argument->queue_mutex);
            break;
        }

        file_name = argument->file_names->front().first; // nume fisier
        file_index = argument->file_names->front().second; // index fisier
        argument->file_names->pop(); // extragem fisierul din coada
        pthread_mutex_unlock(argument->queue_mutex); // deblocam resursa


        ifstream file(file_name);
        string current_word;

        while (file >> current_word) { // citim cuvant cu cuvant
            string parsed_word;
            for (auto c : current_word) { // parsam cuvantul citit
                if (islower(c))
                    parsed_word += c;
                else if (isupper(c))
                    parsed_word += tolower(c);
            }

            if (!parsed_word.empty()) {
                int code = static_cast<int>(parsed_word[0]) - 'a';

                (*(argument->dictionar))[argument->thread_id][code][{parsed_word, file_index}] = 1;
                // avem un rezultat partial de forma (cuvant, id_fisier) care incepe cu litera char(code + 'a')
                // si a fost procesat de thread-ul argument->thread_id
            }
        }
        file.close();
    }
}


void reducer_thread_work(void* arg) {


    while(true) {
        struct thread_arg* argument = (struct thread_arg*)arg;

        pthread_mutex_lock(argument->queue_mutex);
        if(argument->letters_queue->empty()) {
            pthread_mutex_unlock(argument->queue_mutex);
            break;
        }

        int i = argument->letters_queue->front() - 97; // indexul literei in vectori
        argument->letters_queue->pop(); // extrag litera din coada dinamica de litere
        pthread_mutex_unlock(argument->queue_mutex);


        for(int k = 0; k < argument -> M; k++) { // ma uit in informatiile procesate de fiecare mapper

            for (auto dict : (*(argument->dictionar))[k][i]) // din informatia prelucrata de thread
                // ma intereseaza doar cea de la litera abia extrasa din coada
                ((*(argument->index_invers_unordered))[i])[dict.first.first].push(dict.first.second);
                // i este indexul corespunzator literei
                // dict.first.first e string-ul ce desemneaza un cuvant
                // dict.first.second este un numar intreg ce desemneaza id-ul fisierului care contine cuvantul
        }



        for (auto dict : (*(argument->index_invers_unordered))[i]) //liste finale ordonate
            (*(argument->pq))[i].emplace(pair<string, Type>(dict.first, dict.second));
            // la indexul literei extrase din coada inserez indexul inversat, desemnat de cuvant si
            // lista de fisiere in care acesta se afla



        char letter_file[] = "q.txt";
        letter_file[0] = char(i + 97); // modific pentru litera curenta
        ofstream fout(letter_file);


        PQ h = (*(argument->pq))[i]; // lista cu indecsii inversati pt cuvintele ce incep cu litera
        // extrasa din coada


        pair<string, Type> helper; // element din coada de prioritati a literei extrase
        while (!h.empty()) {
            helper = h.top();
            fout << helper.first << ":["; // cuvantul
            Type aux = helper.second;
            int prev = 0;
            while (!aux.empty()) { // lista cu id-urile de fisiere
                if (prev == 1)
                    fout << " ";
                prev = 1;
                fout << aux.top();
                aux.pop();
            }
            fout << "]\n";
            h.pop();
        }
        fout.close();
    }

}

void* f(void* arg) {
    if (((struct thread_arg*)arg)->thread_type == "M") {
        mapper_thread_work(arg);
        pthread_barrier_wait(((struct thread_arg*)arg)->bariera);
    } else {
        pthread_barrier_wait(((struct thread_arg*)arg)->bariera);
        reducer_thread_work(arg);
    }
    return nullptr;
}

queue < pair <string,int> > read_input_file(string &fisier_intrare) {
// functie pentru citirea numelor de fisiere in care se afla informatie de procesat

    ifstream file(fisier_intrare);
    queue < pair <string,int> > file_queue;
    if (file.is_open()) {
        string line;
        getline(file, line);
        int nr_files = stoi(line);
        for (int i = 0; i < nr_files; i++) {
            getline(file, line);
            file_queue.emplace(pair<string,int>(line,i + 1));
        }

    }
    return file_queue;
}

int main(int argc, char* argv[]) {
    int M, R, nr_threads;
    string fisier_intrare;
    pthread_barrier_t bariera;
    M = atoi(argv[1]);
    R = atoi(argv[2]);
    nr_threads = M + R;
    fisier_intrare = argv[3];
    queue < pair <string,int> > file_names = read_input_file(fisier_intrare);


    if (file_names.empty()) {
        cout << "Nu a mers ceva la fisier" << '\n';
        exit(-1);
    }


    if (pthread_barrier_init(&bariera, nullptr, M + R)) {
        cout << "Nu s-a creat bariera" << endl;
        exit(-1);
    }



    vector <vector<map<pair<string, int>, int>> > dictionar(M, vector<map<pair<string, int>, int>>(26) );
    //dictionar[thread_id][initiala][(cuvant,id_fisier)]

    vector<map<string, Type>> index_invers_unordered(26); // liste combinate pe litere si neordonate

    //string e cuvantul, iar elem de tip Type e indexul inversat

    vector<PQ> pqVector(26, PQ(&cmp)); // liste combinate ordonate

    pthread_mutex_t queue_mutex;

    if( pthread_mutex_init(&queue_mutex,nullptr) ) {
        cout << "Nu s-a creat mutexul" << endl;
        exit(-1);
    }

    queue <char> letters_queue;
    for(int i = 0; i < 26; i++)
        letters_queue.push(char(97 + i));


    vector<pthread_t> threads(M + R);
    vector<struct thread_arg> thread_args(M + R);


    for (int i = 0; i < nr_threads; i++) {
        if (i < M) {
            thread_args[i].thread_type = "M";
            thread_args[i].file_names = &file_names;
        } else {
            thread_args[i].thread_type = "R";
        }

        thread_args[i].thread_id = i;
        thread_args[i].bariera = &bariera;
        thread_args[i].M = M;
        thread_args[i].dictionar = &dictionar;
        thread_args[i].index_invers_unordered = &index_invers_unordered;
        thread_args[i].pq = &pqVector;
        thread_args[i].queue_mutex = &queue_mutex;
        thread_args[i].letters_queue = &letters_queue;

        int r = pthread_create(&threads[i], nullptr, f, &thread_args[i]);
        if (r) {
            cout << "Nu s-a creat thread-ul";
            exit(-1);
        }

    }

    for (int i = 0; i < nr_threads; i++)
        pthread_join(threads[i], nullptr);

    pthread_barrier_destroy(&bariera);
    pthread_mutex_destroy(&queue_mutex);

    return 0;
}