/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
 Synchronization
When implementing words_count_p.c, keep in mind the functionality of all these methods are identical to what you did in words_count_l.c. However, you must use synchronization techiques to ensure coordination amongst different threads to prevent race conditions.

Your synchronization must be fine-grained. Different threads should be able to open and read their respective files concurrently, serializing only their modifications to shared data. In particular, it is unacceptable to use a global lock around the call to the count_words function in pwords.c, since it would prevent multiple threads from concurrently reading files. We reserve the right to deduct points from such implementations. Instead, you should only synchronize access to the word count list data structure in word_count_p.c. You will need to ensure all such modifications are complete before printing the result or terminating the process.

We recommend that you start by just implementing the thread-per-file aspect (i.e. without synchronization). This will be quite similar to the code you’ve written in word_count_l.c. Your program might not even error, since multithreaded programs with synchronization bugs may appear to work properly much of the time. Once you’re confident in the logic of your methods, then add in the necessary synchronization.

To help you find subtle synchronization bugs in your program, we have provided a decently large input for your words program in the gutenberg/ directory. These files were generated from select stories from Project Gutenberg, making sure to choose short stories so that the word count program does not take too long to run. Make sure to compare the results of running pwords to running words to check if your output is correct. As stated before, this does not ensure your synchronization is correct, but it might alert you to subtle synchronization bugs that may not manifest for smaller inputs.
 */
#include "list.h"
#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"
void init_words(word_count_list_t* wclist) { /* TODO */
  list_init(&wclist->lst);
  //init the mutex lock
  pthread_mutex_init(&wclist->lock,NULL);
}

size_t len_words(word_count_list_t* wclist) {
  /* TODO */
  return list_size(&wclist->lst);
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  struct list_elem* e;
  struct list_elem* begin = list_begin(&wclist->lst);
  struct list_elem* end = list_end(&wclist->lst);

  for(e = begin; e != end; e = list_next(e)){
    word_count_t *entry = list_entry(e, word_count_t, elem);
    if(strcmp(entry->word, word) == 0){
      return entry;
    }
  }
  return NULL;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  pthread_mutex_lock(&wclist->lock);
  word_count_t* found = find_word(wclist, word);
  if(found != NULL){
    found->count++;
    pthread_mutex_unlock(&wclist->lock);
    return found;
  }

  word_count_t* wc = malloc(sizeof(word_count_t));
  wc->word = strdup(word);
  wc->count = 1;
  list_push_back(&wclist->lst, &wc->elem);
  pthread_mutex_unlock(&wclist->lock);
  return wc;
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */
  pthread_mutex_lock(&wclist->lock);
  struct list_elem* e;
  struct list_elem* begin = list_begin(&wclist->lst);
  struct list_elem* end = list_end(&wclist->lst);
  for(e = begin; e != end; e = list_next(e)){
    word_count_t *entry = list_entry(e, word_count_t, elem);
    fprintf(outfile, "%i\t%s\n", entry->count, entry->word);
  }
  pthread_mutex_unlock(&wclist->lock);
}
static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {
  /* TODO */
  word_count_t* wc1 = list_entry(ewc1, word_count_t, elem);
  word_count_t* wc2 = list_entry(ewc2, word_count_t, elem);
  bool (*less)(const word_count_t*, const word_count_t*) = aux;
  return less(wc1,wc2);
}

void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) {
  /* TODO */
  pthread_mutex_lock(&wclist->lock);
  list_sort(&wclist->lst, less_list, less);
  pthread_mutex_unlock(&wclist->lock);
}
