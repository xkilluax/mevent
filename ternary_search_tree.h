#ifndef _TST_H
#define _TST_H

#include <stdint.h>
#include <sys/types.h>

#include <deque>
#include <string>
#include <memory>
#include <functional>

namespace mevent {
    
class Connection;
    
typedef std::function<void(Connection *c)> HTTPHandleFunc;
    
namespace tst {

struct NodeData {
    std::string      str;
    HTTPHandleFunc   func;
};

struct Node {
    Node             *left;
    Node             *center;
    Node             *right;
    NodeData         *data;
    char              c;
};

struct ResultNode {
    ResultNode     *next;
    ResultNode     *prev;
    NodeData       *data;
};

struct Result {
    ResultNode     *list;
    ResultNode     *tail;
    int             count;
    int             limit;
};

class TernarySearchTree {
public:
    TernarySearchTree();
    ~TernarySearchTree();
    
    Node *Insert(const NodeData *data, Node **npp);
    
    void Traverse(Result *rp);
    
    Result *Search(const char *str, int limit);
    
    NodeData SearchOne(const char *str);
    
    void FreeResult(Result *rp);
    
    void Erase(const char *str);
    
    void Traverse(Node *np, Result *rp);
    
private:
    Node *Insert(Node *np, const char *pos, const NodeData *data, Node **npp);
    
    void Search(Node *np, const char *pos, Result *rp);
    void Search(Node *np, const char *pos, NodeData *ndp);
    
    void Clean(Node *np);
    
    void FreeNode(Node *np);
    
    Result *ResultInit();
    
    void ResultAdd(Result *rp, const NodeData *data);
    
    Node      *root_;
};

} // namespace tst
} // namespace mevent

#endif
