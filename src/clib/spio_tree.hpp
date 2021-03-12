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
    SPIO_tree() {};
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
    /* Id of the imaginary node of the tree that is
     * the parent of all root nodes */
    static const int ROOT_ID = -1;
    /* The nodes of the tree is stored in this vector */
    std::vector<Node> nodes_;
    /* Id of the root nodes of the trees in the forest */
    std::vector<std::size_t> root_node_ids_;
    /* Internal function that implements DFS */
    void dfs(Node &node, SPIO_tree_visitor<T> &vis);
};

/* Add a value to the tree. This value has no parent.
 * A unique id for this value is returned
 */
template<typename T>
int SPIO_tree<T>::add(const T &val)
{
  int parent_id = ROOT_ID;
  int id = static_cast<int>(nodes_.size());
  std::vector<int> children;
  Node val_node = {id, parent_id, val, children};

  nodes_.push_back(val_node);
  /* Since this value has no parent, its a root node */
  root_node_ids_.push_back(id);

  return id;
}

/* Add value as a child to the value previously added
 * to the tree, whose id is parent_id
 * The id of this value is returned
 */
template<typename T>
int SPIO_tree<T>::add(const T &val, int parent_id)
{
  int id = static_cast<int>(nodes_.size());
  std::vector<int> children;
  Node val_node = {id, parent_id, val, children};

  nodes_.push_back(val_node);
  /* Update the parent with reference to this node */
  nodes_[parent_id].children.push_back(id);  

  return id;
}

/* Perform depth first traversal on all nodes in the tree */
template<typename T>
void SPIO_tree<T>::dfs(SPIO_tree_visitor<T> &vis)
{
  if(root_node_ids_.size() == 0){
    return;
  }

  vis.begin();
  /* Perform DFS on all the trees in the forest */
  for(std::vector<std::size_t>::const_iterator citer = root_node_ids_.cbegin();
        citer != root_node_ids_.cend(); ++citer){
    dfs(nodes_[*citer], vis);
  }
  vis.end();
}

/* Perform depth first search traversal on node, node, in the tree */
template<typename T>
void SPIO_tree<T>::dfs(SPIO_tree::Node &node, SPIO_tree_visitor<T> &vis)
{
  if(node.parent_id != ROOT_ID){
    vis.enter_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
  }
  else{
    vis.enter_node(node.val, node.id);
  }
  for(std::vector<int>::const_iterator cid_iter = node.children.cbegin();
      cid_iter != node.children.cend(); ++cid_iter){
    dfs(nodes_[*cid_iter], vis);
    /* Note that after traversing the last child we exit the node, we need to call
     * exit_node() on the visitor
     */
    if(cid_iter + 1 != node.children.cend()){
      if(node.parent_id != ROOT_ID){
        vis.on_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
      }
      else{
        vis.on_node(node.val, node.id);
      }
    }
  }
  if(node.parent_id != ROOT_ID){
    vis.exit_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
  }
  else{
    vis.exit_node(node.val, node.id);
  }
}

} // namespace PIO_Util

#endif /* __SPIO_TREE_HPP__ */
