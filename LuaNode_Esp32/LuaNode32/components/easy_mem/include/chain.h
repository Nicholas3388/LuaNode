/**
 * Chain operation
 *
 * Nicholas3388
 */

#ifndef _CHAIN_H_
#define _CHAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#define add_to_head(head, item) {   \
        if (head == NULL) { \
            head = item;    \
            item->next = NULL;  \
        } else {    \
            item->next = head;  \
            head = item;    \
        }   \
}

#define add_to_tail(head, type, item)   {   \
        type *pos = head;  \
        if(pos == NULL) {   \
            head = item;    \
            item->next = NULL;  \
        } else {   \
            while(pos->next != NULL) {pos = pos->next;}   \
            pos->next = item;   \
            item->next = NULL;  \
        }   \
}

#define chain_cat(chain1, chain2, type)   {   \
        type *pos = chain1; \
        if(chain1 == NULL) {chain1 = chain2;}   \
        if(chain2 != NULL && chain1 != NULL) {  \
            while(pos->next != NULL) {pos = pos->next;} \
            pos->next = chain2; \
        }   \
}

#define remove_from_chain(head, type, item)   {   \
        type *pos = head; type *prev = NULL;    \
        while(pos != NULL) {    \
            if(pos == item) {   \
                if(prev != NULL) {prev->next = pos->next;}  \
                else {head = pos->next;}   \
                item->next = NULL;  \
                break;  \
            }   \
            prev = pos; pos = pos->next;    \
        }   \
}

#define is_chain_empty(head)    (head == NULL)

#define foreach(head, iterator)  for(iterator=head; iterator!=NULL; iterator=iterator->next)

#ifdef __cplusplus
}
#endif

#endif // _CHAIN_H_
