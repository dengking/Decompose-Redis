# rax

redis的rax实现其实是即支持trie又支持radix tree的；认知到这一点，我是先通过阅读了trie的实现后，再阅读redis rax的实现才发现的这个问题，以下是trie的两种实现：

- python trie：https://github.com/dengking/TheAlgorithms-Python/tree/master/data_structures/trie
- c trie：https://github.com/dengking/TheAlgorithms-C/tree/master/data_structures/trie

python trie中，node的定义是如下的：

```python
class TrieNode:
    def __init__(self):
        self.nodes = dict()  # Mapping from char to TrieNode
        self.is_leaf = False
```

它通过使用`dict`来实现节点之间的关联，其实可以认为`dict`所保存的是edge，`dict` 的key就是edge的label；

radish rax在支持trie的时候，其实使用的就是类似于上述的实现方式，以下是它的文档中给出的：

>     /* Data layout is as follows:
>      *
>      * If node is not compressed we have 'size' bytes, one for each children
>      * character, and 'size' raxNode pointers, point to each child node.
>      * Note how the character is not stored in the children but in the
>      * edge of the parents:
>      *
>      * [header iscompr=0][abc][a-ptr][b-ptr][c-ptr](value-ptr?)

`[abc][a-ptr][b-ptr][c-ptr]`其实就相当于一个dict

