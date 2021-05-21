#include "list.h"
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

// Complete structs from .h
typedef struct Node_s Node;
struct Node_s {
    void* pItem;
    Node* pNext;
    Node* pPrev;
};

enum ListOutOfBounds {
    LIST_OOB_START,
    LIST_OOB_END
};
typedef struct List_s List;
struct List_s {
    Node* pFirstNode;
    Node* pLastNode;
    Node* pCurrentNode;
    int count;
    List* pNextFreeHead;
    enum ListOutOfBounds lastOutOfBoundsReason;
};


static List s_heads[LIST_MAX_NUM_HEADS];
static Node s_nodes[LIST_MAX_NUM_NODES];
static List *s_pFirstFreeHead;
static Node *s_pFirstFreeNode;
static bool s_isInitialized = false;

/* Private Headers */
static void initializeDataStructures();
static bool isOOBAtStart();
static bool isOOBAtEnd();

// thread-safe (recursive)
static pthread_mutex_t s_listMutex = PTHREAD_MUTEX_INITIALIZER;
static void mutexInitialize(void) 
{   
    // Recursive mutex
    // https://stackoverflow.com/questions/7037481/c-how-do-you-declare-a-recursive-mutex-with-posix-threads
    pthread_mutexattr_t Attr;
    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&s_listMutex, &Attr);
    
    pthread_mutexattr_destroy(&Attr);
}
static void mutexLock(void)
{
    pthread_mutex_lock(&s_listMutex);
}
static void mutexUnlock(void)
{
    pthread_mutex_unlock(&s_listMutex);
}


List* List_create()
{
    if (!s_isInitialized) {
        initializeDataStructures();
    }

    mutexLock();

    // Get next free head
    List* pList = s_pFirstFreeHead;
    if (pList) {
        s_pFirstFreeHead = s_pFirstFreeHead->pNextFreeHead;
        pList->pNextFreeHead = NULL;
    }

    mutexUnlock();
    return pList;
}


int List_count(List* pList)
{
    mutexLock();
    int count = pList->count;
    mutexUnlock();
    return count;
}

void* List_first(List* pList)
{
    mutexLock();
    // Point current to first
    pList->pCurrentNode = pList->pFirstNode;
    void *pItem = List_curr(pList);
    mutexUnlock();
    return pItem;
}

void* List_last(List* pList)
{
    mutexLock();
    pList->pCurrentNode = pList->pLastNode;
    void *pItem = List_curr(pList);
    mutexUnlock();
    return pItem;
}


void* List_next(List* pList)
{
    mutexLock();
    // When before the list, move to first node
    if (isOOBAtStart(pList)) {
        pList->pCurrentNode = pList->pFirstNode;
    } else if(isOOBAtEnd(pList)) {
        // do nothing
    } else {
        pList->pCurrentNode = pList->pCurrentNode->pNext;
    }

    // If after the list; record reason
    if (pList->pCurrentNode == NULL) {
        pList->lastOutOfBoundsReason = LIST_OOB_END;
    }

    void* pItem = List_curr(pList);
    mutexUnlock();
    return pItem;
}


void* List_prev(List* pList)
{
    mutexLock();
    // When after the list, move to last node
    if (isOOBAtEnd(pList)) {
       pList->pCurrentNode = pList->pLastNode;
    } else if(isOOBAtStart(pList)){
        // do nothing
    } else {
        pList->pCurrentNode = pList->pCurrentNode->pPrev;
    }

    // If after the list; record reason
    if (pList->pCurrentNode == NULL) {
        pList->lastOutOfBoundsReason = LIST_OOB_START;
    }
    void* pItem = List_curr(pList);
    mutexUnlock();
    return pItem;
}

void* List_curr(List* pList)
{
    mutexLock();
    void* pItem = NULL;
    if (pList->pCurrentNode != NULL) {
        pItem = pList->pCurrentNode->pItem;
    }
    mutexUnlock();
    return pItem;
}


static bool haveNoFreeNodes()
{
    return s_pFirstFreeNode == NULL;
}
static Node* makeNewNode(void* pItem) 
{
    assert(!haveNoFreeNodes());
    Node* pNode = s_pFirstFreeNode;
    pNode->pItem = pItem;
    s_pFirstFreeNode = s_pFirstFreeNode->pNext;
    pNode->pNext = NULL;
    return pNode;
}
static void linkNodeAtStart(List* pList, Node* pNode)
{
    pNode->pPrev = NULL;
    pNode->pNext = pList->pFirstNode;
    if (pList->count >= 1) {
        pList->pFirstNode->pPrev = pNode;
    } else {
        pList->pLastNode = pNode;
    }

    pList->pFirstNode = pNode;
    pList->pCurrentNode = pNode;
    pList->count++;
}
static void linkNodeAtEnd(List* pList, Node* pNode)
{
    pNode->pPrev = pList->pLastNode;
    pNode->pNext = NULL;
    if (pList->count >= 1) {
        pList->pLastNode->pNext = pNode;
    } else {
        pList->pFirstNode = pNode;
    }
    pList->pLastNode = pNode;
    pList->pCurrentNode = pNode;
    pList->count++;
}
static void linkNodeAfterCurrent(List* pList, Node* pNode)
{
    // If current is at the last node, we are actually
    // linking this to the end (must update last node ptr)
    bool isInsertAtEnd = pList->pCurrentNode == pList->pLastNode
        || isOOBAtEnd(pList);
    if (isInsertAtEnd) {
        linkNodeAtEnd(pList, pNode);
    } else if (isOOBAtStart(pList)) {
        linkNodeAtStart(pList, pNode);
    } else {
        assert(pList->pCurrentNode != NULL);
        pNode->pPrev = pList->pCurrentNode;
        pNode->pNext = pList->pCurrentNode->pNext;
        pList->pCurrentNode->pNext = pNode;
        pNode->pNext->pPrev = pNode;
        pList->pCurrentNode = pNode;
        pList->count++;
    }
}



// Add after current
int List_add(List* pList, void* pItem)
{
    mutexLock();
    // Get free node
    if (haveNoFreeNodes()) {
        mutexUnlock();
        return LIST_FAIL;
    }
    Node* pNode = makeNewNode(pItem);
    
    // Insert
    linkNodeAfterCurrent(pList, pNode);
    mutexUnlock();
    return LIST_SUCCESS;
}

// Add before current
int List_insert(List* pList, void* pItem)
{
    mutexLock();
    // Get free node
    if (haveNoFreeNodes()) {
        mutexUnlock();
        return LIST_FAIL;
    }
    Node* pNode = makeNewNode(pItem);
    
    // Insert
    List_prev(pList);
    linkNodeAfterCurrent(pList, pNode);
    mutexUnlock();
    return LIST_SUCCESS;
}

// Add at end
int List_append(List* pList, void* pItem)
{
    mutexLock();
    // Get free node
    if (haveNoFreeNodes()) {
        mutexUnlock();
        return LIST_FAIL;
    }
    Node* pNode = makeNewNode(pItem);
    
    // Insert
    pList->pCurrentNode = pList->pLastNode;
    linkNodeAtEnd(pList, pNode);
    mutexUnlock();
    return LIST_SUCCESS;
}

// Add at beginning
int List_prepend(List* pList, void* pItem)
{
    mutexLock();

    // Get free node
    if (haveNoFreeNodes()) {
        mutexUnlock();
        return LIST_FAIL;
    }
    Node* pNode = makeNewNode(pItem);
    
    // Insert
    linkNodeAtStart(pList, pNode);
    mutexUnlock();
    return LIST_SUCCESS;
}

// Remove current
void* List_remove(List* pList)
{
    mutexLock();

    if (pList->count == 0
        || isOOBAtStart(pList)
        || isOOBAtEnd(pList)
    ) {
        mutexUnlock();
        return NULL;
    }

    // Get node/item
    Node* pRemoveNode = pList->pCurrentNode;
    void* pItem = pRemoveNode->pItem;

    // Unlink forwards
    Node* pPrevNode = pRemoveNode->pPrev;
    Node* pNextNode = pRemoveNode->pNext;
    if (pPrevNode != NULL) {
        pPrevNode->pNext = pNextNode;
    } else {
        pList->pFirstNode = pNextNode;
    }

    // Unlink backwards
    if (pNextNode != NULL) {
        pNextNode->pPrev = pPrevNode;
    } else {
        pList->pLastNode = pPrevNode;
    }
    pList->count --;

    // Recover node
    pRemoveNode->pNext = s_pFirstFreeNode;
    s_pFirstFreeNode = pRemoveNode;

    // Reset current to last (smartly)
    pList->pCurrentNode = pNextNode;
    if (pList->pCurrentNode == NULL) {
        pList->lastOutOfBoundsReason = LIST_OOB_END;
    }
    mutexUnlock();
    return pItem;
}

// Remove last
void* List_trim(List* pList)
{
    mutexLock();

    List_last(pList);
    void* pItem = List_remove(pList);
    List_last(pList);
    
    mutexUnlock();
    return pItem;
}

void List_concat(List* pList1, List* pList2)
{
    mutexLock();

    // Relink Nodes from list 2 to list 1:
    Node* pTail1 = pList1->pLastNode;
    Node* pHead2 = pList2->pFirstNode;
    if (pHead2 == NULL) {
        // 2nd list is empty; done!
    } else if (pTail1 == NULL) {
        pList1->pFirstNode = pHead2;
        pList1->pLastNode = pList2->pLastNode;
    } else {
        pTail1->pNext = pHead2;
        pHead2->pPrev = pTail1;
        pList1->pLastNode = pList2->pLastNode;        
    }
    pList1->count += pList2->count;

    pList2->count = 0;
    pList2->pCurrentNode = NULL;
    pList2->pFirstNode = NULL;
    pList2->pLastNode = NULL;

    // Delete the list
    // O(1) because the list is empty.
    List_free(pList2, NULL);

    mutexUnlock();
}

void List_free(List* pList, FREE_FN pItemFreeFn)
{
    mutexLock();
    // Free all nodes
    while (List_count(pList) > 0) {
        Node* pNode = List_trim(pList);

        // Call free function (possibly cleaning up memory)
        if (pItemFreeFn != NULL) {
            (*pItemFreeFn)(pNode);
        }
    }

    // Free list
    pList->pNextFreeHead = s_pFirstFreeHead;
    s_pFirstFreeHead = pList;
    mutexUnlock();
}

// Search pList, starting at the current item, until the end is reached or a match is found. 
// In this context, a match is determined by the comparator parameter. This parameter is a
// pointer to a routine that takes as its first argument an item pointer, and as its second 
// argument pComparisonArg. Comparator returns 0 if the item and comparisonArg don't match, 
// or 1 if they do. Exactly what constitutes a match is up to the implementor of comparator. 
// 
// If a match is found, the current pointer is left at the matched item and the pointer to 
// that item is returned. If no match is found, the current pointer is left beyond the end of 
// the list and a NULL pointer is returned.
void* List_search(List* pList, COMPARATOR_FN pComparator, void* pComparisonArg)
{
    mutexLock();
    if (isOOBAtStart(pList)) {
        List_first(pList);
    }

    while(pList->pCurrentNode != NULL) {
        // Match? 
        void* pItem = pList->pCurrentNode->pItem;
        if ( (*pComparator)(pItem, pComparisonArg) == 1) {
            return pItem;
        }

        List_next(pList);
    }
    mutexUnlock();
    return NULL;
}







/*
    PRIVATE FUNCTIONS
*/
static void initializeDataStructures() {
    mutexInitialize();
    
    mutexLock();

    assert(LIST_MAX_NUM_NODES > 0);
    assert(LIST_MAX_NUM_HEADS > 0);


    // Nodes
    s_pFirstFreeNode = &s_nodes[0];
    for (int i = 0; i < LIST_MAX_NUM_NODES; i++) {
        s_nodes[i].pItem = NULL;
        s_nodes[i].pPrev = NULL;
        s_nodes[i].pNext = NULL;
        if (i + 1 < LIST_MAX_NUM_NODES) {
            s_nodes[i].pNext = &s_nodes[i+1];
        }
    }

    // Heads
    s_pFirstFreeHead = &s_heads[0];
    for (int i = 0; i < LIST_MAX_NUM_HEADS; i++) {
        s_heads[i].count = 0;
        s_heads[i].pCurrentNode = NULL;
        s_heads[i].lastOutOfBoundsReason = LIST_OOB_START;
        s_heads[i].pFirstNode = NULL;
        s_heads[i].pLastNode = NULL;
        s_heads[i].pNextFreeHead = NULL;
        if (i + 1 < LIST_MAX_NUM_HEADS) {
            s_heads[i].pNextFreeHead = &s_heads[i + 1];
        }
    }
    s_isInitialized = true;
    mutexUnlock();
}

static bool isOOBAtStart(List* pList) {
    assert(pList->lastOutOfBoundsReason == LIST_OOB_START 
        || pList->lastOutOfBoundsReason == LIST_OOB_END);
    return pList->pCurrentNode == NULL 
        && pList->lastOutOfBoundsReason == LIST_OOB_START;
}
static bool isOOBAtEnd(List* pList) {
    assert(pList->lastOutOfBoundsReason == LIST_OOB_START 
        || pList->lastOutOfBoundsReason == LIST_OOB_END);

    return pList->pCurrentNode == NULL 
        && pList->lastOutOfBoundsReason == LIST_OOB_END;
}