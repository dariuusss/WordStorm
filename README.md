# WordStorm

# Execution Flow in main()

In `main()`, we extract command-line arguments, read the names of the files to be processed, initialize thread data, create threads, and join them. The function used as a parameter for the threads is `f`, which handles synchronization and the execution of mapper and reducer functions as follows:

- As a synchronization primitive to ensure reducers execute **only after** mappers, a barrier initialized with `M + R` is used, where `M` is the number of mapper threads and `R` is the number of reducer threads.
- All threads, regardless of type, will wait at the barrier. The only difference lies in the order of reaching the barrier and executing tasks.
- If `f` is called for a mapper, it invokes `mapper_thread_work()`, then waits at the barrier.
- If `f` is called for a reducer, it waits at the barrier **before** invoking `reducer_thread_work()`.
- Until the barrier is lifted, **only the mappers** execute their tasks; only after that can reducers start their work.

---

## Data Types and Functions Used

### **Type Definitions**

- `using Type = priority_queue<int, vector<int>, greater<int>>;`  
  → Stores file IDs where a specific word appears, implemented as a priority queue to keep them in ascending order.

- `using PQ = priority_queue<pair<string, Type>, vector<pair<string, Type>>, decltype(&cmp)>;`  
  → Used for storing the inverted index: `pair[0]` represents the word, and `pair[1]` represents the list of files in which it appears.

### **Thread Argument Structure**

```cpp
struct thread_arg {
    string thread_type; // "M" (Mapper) or "R" (Reducer)
    int thread_id;
    pthread_barrier_t* bariera; // Ensures reducers execute only after mappers
    int M; // Number of mapper threads
    vector<vector<map<pair<string, int>, int>>>* dictionar; // Partial lists storage
    // Structure: dictionar[thread_id][letter][(word, file_id)]
    vector<map<string, Type>>* index_invers_unordered; // Unordered final lists by letter
    vector<PQ>* pq; // Ordered final lists by letter
    queue<pair<string, int>>* file_names; // Dynamic queue of files for mappers
    pthread_mutex_t* queue_mutex; // Prevents simultaneous extraction by multiple threads
    queue<char>* letters_queue; // Dynamic queue of letters for reducers
};
```

---

## Function Descriptions

### **`mapper_thread_work(void* arg)`**

This function is used for executing a mapper thread. To ensure efficiency:
- A **file queue** is used, where each thread extracts a file to process.
- The extracted file is opened, read, and parsed **word by word**.
- For each unique, non-empty parsed word, a **partial list** element `(word, file_id)` is generated and stored.
- Storage is structured based on the **initial letter** of the word and the **ID of the processing thread**.

---

### **`reducer_thread_work(void* arg)`**

This function is used for executing a reducer thread. To ensure efficiency:
- Each reducer **dynamically extracts** a letter from a **letter queue**.
- The reducer **aggregates** partial lists into an **inverted index**.
- Sorting is performed **in descending order** based on the number of files in which a word appears, with **alphabetical sorting** in case of ties.
- The results are written to a file named `$`.txt, where `$` represents a lowercase English letter.

#### **Processing flow for a reducer thread**
1. Extracts a **letter** from the queue.
2. Iterates through **partial lists** from all mapper threads, selecting only those whose words start with the extracted letter.
3. Aggregates results into an **intermediate structure**.
4. **Sorts** the structure based on the **inverted index length** (descending) and alphabetically in case of ties.
5. **Writes** the sorted results into the corresponding output file.

This structured approach ensures efficient mapping, reducing, and synchronization for processing the given files.

