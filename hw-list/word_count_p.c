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

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

void init_words(word_count_list_t* wclist) { /* TODO */
  pthread_mutex_init(&(wclist->lock),NULL);
  list_init(&(wclist->lst));
}

size_t len_words(word_count_list_t* wclist) {
  /* TODO */
  return list_size(&(wclist->lst));
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  struct list_elem *e;
  for(e=list_begin(&(wclist->lst));e!=list_end(&(wclist->lst));e=list_next(e)){
    word_count_t *wc_ptr=list_entry(e,struct word_count,elem);
    if(strcmp(word,wc_ptr->word)==0){
      return wc_ptr;
    }
  }
  return NULL;
  
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  pthread_mutex_lock(&(wclist->lock));
  word_count_t * wc_ptr=find_word(wclist,word);
  if(wc_ptr!=NULL){
    ++wc_ptr->count;
  }else{
    wc_ptr=(word_count_t*)malloc(sizeof(word_count_t));
    wc_ptr->count=1;
    wc_ptr->word=word;
    list_push_front(&(wclist->lst),&wc_ptr->elem);
  }
  pthread_mutex_unlock(&(wclist->lock));
  return wc_ptr;
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */
  struct list_elem *e;
  for(e=list_begin(&(wclist->lst));e!=list_end(&(wclist->lst));e=list_next(e)){
    word_count_t *wc_ptr=list_entry(e,struct word_count,elem);
    fprintf(outfile,"%i\t%s\n",wc_ptr->count,wc_ptr->word);
  }
}

void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) {
  /* TODO */
  // pthread_mutex_lock(&(wclist->lock));
  // list_sort(&(wclist->lst),false,less);
  // pthread_mutex_unlock(&(wclist->lock));
}
