#ifndef __HASH_NODE_H__
#define __HASH_NODE_H__

#define OVER_WRITE_CALL_ID 223
typedef enum EHashOperator 
{
    kHashInsert,
    kHashFind,
    kHashErase,
}HashOperator;
typedef struct SCallContext
{
    HashOperator oper;
    int key;
    int value;
}CallContext;

#endif
