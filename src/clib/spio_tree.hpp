#ifndef __SPIO_TREE_HPP__
#define __SPIO_TREE_HPP__

namespace PIO_Util{

/* All visitors used with SPIO_tree<T> must inherit from this generic
 * visitor class
 */
template<typename T>
class SPIO_tree_visitor{
  public:
    /* Called once before the first node is traversed */
    virtual void begin(void ) {};
    /* Called the first time a traversal enters a tree node */
    virtual void enter_node(T &val, int val_id) {};
    virtual void enter_node(T &val, int val_id, T &parent_val, int parent_id) {};
    /* Called all times, except the first time, a traveral is on a tree node */
    virtual void on_node(T &val, int val_id) {};
    virtual void on_node(T &val, int val_id, T &parent_val, int parent_id) {};
    /* Called when we exit the node */
    virtual void exit_node(T &val, int val_id) {};
    virtual void exit_node(T &val, int val_id, T &parent_val, int parent_id) {};
    /* Called once after all the nodes are traversed */
    virtual void end(void ) {};
    virtual ~SPIO_tree_visitor() {};
};

/* A generic tree
 * Only adding values/nodes to the tree is allowed. Deletion is not supported
 */
template<typename T>
class SPIO_tree{
  public:
    SPIO_tree();
    /* Add a value to the tree, returns a unique id for the value */
    int add(const T &val);
    /* Add a value as a child to a previous value added in the tree
     * The id of the previous value needs to be passed in as the parent_id
     */
    int add(const T &val, int parent_id);
    /* Perform depth first search on the tree */
    void dfs(SPIO_tree_visitor<T> &vis);
  private:
    /* Internal node of the tree */
    struct Node{
      /* Unique id for the node */
      int id;
      /* Id of parent of the node */
      int parent_id;
      /* The user data cached on the node */
      T val;
      /* Ids of children of this node */
      std::vector<int> children;
    };
    /* Id of the internal root added for the tree */
    int root_id_;
    /* The nodes of the tree is stored in this vector */
    std::vector<Node> nodes_;
    /* Internal function that implements DFS */
    void dfs(Node &node, SPIO_tree_visitor<T> &vis);
};

template<typename T>
SPIO_tree<T>::SPIO_tree()
{
  int id = 0;
  int parent_id = 0;
  T dummy_val;
  std::vector<int> children;

  /* Create the root node, this node is not visible to the user */
  Node root_node = {id, parent_id, dummy_val, children};

  root_id_ = root_node.id;

  /* Add the root node */
  nodes_.push_back(root_node);
}

template<typename T>
int SPIO_tree<T>::add(const T &val)
{
  int parent_id = root_id_;
  return add(val, parent_id);
}

template<typename T>
int SPIO_tree<T>::add(const T &val, int parent_id)
{
  int id = nodes_.size();
  std::vector<int> children;
  Node val_node = {id, parent_id, val, children};

  nodes_.push_back(val_node);
  nodes_[parent_id].children.push_back(id);  

  return id;
}

template<typename T>
void SPIO_tree<T>::dfs(SPIO_tree_visitor<T> &vis)
{
  if(nodes_.size() == 0){
    return;
  }

  vis.begin();
  dfs(nodes_[0], vis);
  vis.end();
}

/* Depth first search traversal on the tree */
template<typename T>
void SPIO_tree<T>::dfs(SPIO_tree::Node &node, SPIO_tree_visitor<T> &vis)
{
  if(node.id != root_id_){
    if(node.parent_id != root_id_){
      vis.enter_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
    }
    else{
      vis.enter_node(node.val, node.id);
    }
  }
  for(std::vector<int>::const_iterator cid_iter = node.children.cbegin();
      cid_iter != node.children.cend(); ++cid_iter){
    dfs(nodes_[*cid_iter], vis);
    /* Note that after traversing the last child we exit the node, we need to call
     * exit_node() on the visitor
     */
    if(node.id != root_id_){
      if(cid_iter + 1 != node.children.cend()){
        if(node.parent_id != root_id_){
          vis.on_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
        }
        else{
          vis.on_node(node.val, node.id);
        }
      }
    }
  }
  if(node.id != root_id_){
    if(node.parent_id != root_id_){
      vis.exit_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
    }
    else{
      vis.exit_node(node.val, node.id);
    }
  }
}

} // namespace PIO_Util

#endif /* __SPIO_TREE_HPP__ */
