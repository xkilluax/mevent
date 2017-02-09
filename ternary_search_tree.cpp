#include "ternary_search_tree.h"
#include "util.h"

#define TST_MAX_RANK 0xffffffff

namespace mevent {
namespace tst {

TernarySearchTree::TernarySearchTree() {
    root_ = NULL;
}
    
TernarySearchTree::~TernarySearchTree() {
    Clean(root_);
}

Node *TernarySearchTree::Insert(const NodeData *data, Node **npp) {
    root_ = Insert(root_, data->str.c_str(), data, npp);
    return root_;
}

Node *TernarySearchTree::Insert(Node *np, const char *pos, const NodeData *data, Node **npp) {
    if (!np) {
        np = new Node();
        
        np->c = *pos;
        np->left = NULL;
        np->center = NULL;
        np->right = NULL;
        np->data = NULL;
    }

    if (*pos < np->c) {
        np->left = Insert(np->left, pos, data, npp);
    } else if (*pos > np->c) {
        np->right = Insert(np->right, pos, data, npp);
    } else {
        if (*(pos + 1) == 0) {
            if (!np->data) {
                np->data = new NodeData(*data);
                
                if (npp) {
                    *npp = np;
                }
            } else {
                if (npp) {
                    *npp = NULL;
                }
            }
        } else {
            np->center = Insert(np->center, ++pos, data, npp);
        }
    }
    
    return np;
}

void TernarySearchTree::Traverse(Node *np, Result *rp) {
    if (!np) {
        return;
    }
    
    Traverse(np->left, rp);
    
    if (!np->data) {
        Traverse(np->center, rp);
    } else {
        if (rp) {
            ResultAdd(rp, np->data);
        }
        
        if (rp->count > rp->limit) {
            return;
        }
        
        if (np->center != 0) {
            Traverse(np->center, rp);
        }
    }
    
    Traverse(np->right, rp);
}

void TernarySearchTree::Traverse(Result *tsrp) {
    Traverse(root_, tsrp);
}

Result *TernarySearchTree::ResultInit() {
    Result *result = new Result();
    
    result->count = 0;
    result->list = NULL;
    result->tail = NULL;
    
    return result;
}

void TernarySearchTree::Search(Node *np, const char *pos, Result *rp) {
    if (!np) {
        return;
    }
    
    if (*pos < np->c) {
        Search(np->left, pos, rp);
    } else if (*pos > np->c) {
        Search(np->right, pos, rp);
    } else {
        if (*(pos + 1) == 0) {
            if (np->data) {
                ResultAdd(rp, np->data);
            }
            
            Traverse(np->center, rp);
        } else {
            Search(np->center, ++pos, rp);
        }
    }

}

void TernarySearchTree::Search(Node *np, const char *pos, NodeData *ndp) {
    if (!np) {
        return;
    }
    
    if (*pos < np->c) {
        Search(np->left, pos, ndp);
    } else if (*pos > np->c) {
        Search(np->right, pos, ndp);
    } else {
        if (np->data) {
            ndp->func = np->data->func;
            ndp->str = np->data->str;
        }
        
        if (*(pos + 1)) {
            Search(np->center, ++pos, ndp);
        }
    }
}

Result *TernarySearchTree::Search(const char *str, int limit) {
    Result *result = ResultInit();
    result->limit = limit;
    if (!result) {
        return NULL;
    }
    
    Search(root_, str, result);
    
    return result;
}

NodeData TernarySearchTree::SearchOne(const char *str) {
    NodeData result;
    
    Search(root_, str, &result);
    
    return result;
}

void TernarySearchTree::ResultAdd(Result *rp, const NodeData *data) {
    if (!data) {
        return;
    }
    
    ResultNode *node = new ResultNode();
    
    node->data = new NodeData(*data);
    
    node->next = NULL;
    node->prev = NULL;
    
    if (!rp->tail) {
        rp->list = node;
        rp->tail = node;
    } else {
        node->prev = rp->tail;
        rp->tail->next = node;
        rp->tail = node;
    }
    
    rp->count++;
}

void TernarySearchTree::Erase(const char *str) {
    Node *last_np = NULL, *np = root_, *first_np = NULL;
    const char *s = str, *last_s = s;
    
    if (!str) {
        return;
    }

    while (np) {
        if (*s == np->c) {
            if (!first_np) {
                first_np = np;
            }
            
            if (*++s == 0) {
                break;
            }
            
            np = np->center;
            if (!np) {
                return;
            }
        } else {
            last_s = s;

            if (*s < np->c) {
                np = np->left;
            } else {
                np = np->right;
            }
            
            if (!np) {
                break;
            }
        }
        
        if (np->left || np->right) {
            last_np = np;
            last_s = s + 1;
        }
    }
    
    if (*s != 0) {
        return;
    }
    
    if (!np->data) {
        return;
    } else {
        delete np->data;
        np->data = NULL;
    }
    
    if (np->center) {
        return;
    }

    if (last_np) {
        if (last_np->left && last_np->left->c == *last_s) {
            Clean(last_np->left);
            last_np->left = NULL;
        } else if (last_np->right && last_np->right->c == *last_s) {
            Clean(last_np->right);
            last_np->right = NULL;
        } else if(last_np->center && last_np->center->c == *last_s) {
            Clean(last_np->center);
            last_np->center = NULL;
        }
    } else {
        Clean(first_np->center);
        first_np->center = NULL;
    }
}

void TernarySearchTree::Clean(Node *np) {
    if (!np) {
        return;
    }
    
    Clean(np->left);
    Clean(np->center);
    Clean(np->right);
    
    if (np->data) {
        delete np->data;
    }
    
    delete np;
}

void TernarySearchTree::FreeNode(Node *tnp) {
    if (tnp->data) {
        delete tnp->data;
    }
    
    delete tnp;
}

void TernarySearchTree::FreeResult(Result *rp) {
    ResultNode *list = rp->list;
    ResultNode *node;
    
    while (list) {
        node = list;
        list = list->next;
        delete node->data;
        delete node;
    }
    
    delete rp;
}

} // namespace tst
} // namespace mevent