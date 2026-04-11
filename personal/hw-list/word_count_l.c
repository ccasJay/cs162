/*
 * Implementation of the word_count interface using Pintos lists.
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

/* Finally, complete word_count_l.c to properly implement the word count API given in word_count.h. You must use the Pintos list API. After you finish making this change, lwords should work properly (i.e. exhibit the same behavior as frequency mode of words).

The wordcount_sort function sorts the wordcount list according to the comparator passed as an argument. Although lwords uses the less count function from word_helpers.h as the less argument, the wordcount sort function should be generic enough to work with any valid comparator passed in as the less argument. For example, passing the less word function from word_helpers.h as the less parameter should. Check out some basics on function pointers if you’re having trouble understanding and writing the syntax. */


#include "list.h"

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_l.c"
#endif

#include "word_count.h"

void init_words(word_count_list_t* wclist) { /* TODO */
  list_init(wclist);
}

size_t len_words(word_count_list_t* wclist) {
  /* TODO */
  if(list_empty(wclist))perror("list is empty");
  return list_size(wclist);
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  if(list_empty(wclist))perror("list is empty");
  struct list_elem* e;
  struct list_elem* begin = list_begin(wclist);
  struct list_elem* end = list_end(wclist);
  for(e = begin; e != end; e = list_next(e)){
    word_count_t *entry = list_entry(e,  word_count_t, elem);
    if(strcmp(entry->word, word) == 0){  //strcmp()是C语言中用于比较两个字符串是否相等的函数。当strcmp()返回0时，表示两个字符串相等。
      return entry;
    }
  }
  perror("word not found");
  return NULL;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  if(list_empty(wclist))perror("list is empty");
  word_count_t* found = find_word(wclist,word);
  if(found != NULL){
    found->count++;
    return found;
  }

  word_count_t* wc = malloc(sizeof(word_count_t));
  wc->word = word;
  wc->count = 1;
  list_push_back(wclist, &wc->elem);
  return NULL;
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */
  if(list_empty(wclist))perror("list is empty");
  struct list_elem* e;
  struct list_elem* begin = list_begin(wclist);
  struct list_elem* end = list_end(wclist);
  for(e = begin; e != end; e = list_next(e)){
    word_count_t *entry = list_entry(e,  word_count_t, elem);
    fprintf(outfile, "%i\t%s\n", entry->count, entry->word);
  }
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
  list_sort(wclist, less_list, less);
}
