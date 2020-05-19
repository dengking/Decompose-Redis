```
typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;
```

节点的数据类型是pointer，因为redis中的所有的数据都是来自于网络，都是从接收到的数据new出一片空间的；

在`networking.c`中有如下函数：
```
/* This function links the client to the global linked list of clients.
 * unlinkClient() does the opposite, among other things. */
void linkClient(client *c) {
    listAddNodeTail(server.clients,c);
    /* Note that we remember the linked list node where the client is stored,
     * this way removing the client in unlinkClient() will not require
     * a linear scan, but just a constant time operation. */
    c->client_list_node = listLast(server.clients);
    uint64_t id = htonu64(c->id);
    raxInsert(server.clients_index,(unsigned char*)&id,sizeof(id),c,NULL);
}
```

`listAddNodeTail`函数的原型如下：

```
list *listAddNodeTail(list *list, void *value)
```
显然在`linkClient`函数中，涉及了从`client *`到`void *`类型的转换