#ifndef __SPIO_TREE_HPP__
#define __SPIO_TREE_HPP__

namespace PIO_Util{

template<typename T>
class SPIO_tree_visitor{
  public:
    /* Called the first time a traversal enters a tree node */
    virtual void enter_node(T &val, int val_id) {};
    virtual void enter_node(T &val, int val_id, T &parent_val, int parent_id) {};
    /* Called all times, except the first time, a traveral is on a tree node */
    virtual void on_node(T &val, int val_id, T &parent_val, int parent_id) {};
    /* Called when we exit the node */
    virtual void exit_node(T &val, int val_id) {};
    virtual void exit_node(T &val, int val_id, T &parent_val, int parent_id) {};
    virtual ~SPIO_tree_visitor() {};
};

template<typename T>
class SPIO_tree{
  public:
    SPIO_tree();
    int add(const T &val);
    int add(const T &val, int parent_id);
    void dfs(SPIO_tree_visitor<T> &vis);
  private:
    struct Node{
      int id;
      int parent_id;
      T val;
      std::vector<int> children;
    };
    int root_id_;
    std::vector<Node> nodes_;
    void dfs(Node &node, SPIO_tree_visitor<T> &vis);
};

template<typename T>
SPIO_tree<T>::SPIO_tree()
{
  int id = 0;
  int parent_id = 0;
  T dummy_val;
  std::vector<int> children;

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

  dfs(nodes_[0], vis);
}

template<typename T>
void SPIO_tree<T>::dfs(SPIO_tree::Node &node, SPIO_tree_visitor<T> &vis)
{
  if(node.id != root_id_){
    vis.enter_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
  }
  for(std::vector<int>::const_iterator cid_iter = node.children.cbegin();
      cid_iter != node.children.cend(); ++cid_iter){
    dfs(nodes_[*cid_iter], vis);
    /* Note that after traversing the last child we exit the node, we need to call
     * exit_node() on the visitor
     */
    if(node.id != root_id_){
      if(cid_iter + 1 != node.children.cend()){
        vis.on_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
      }
    }
  }
  if(node.id != root_id_){
    vis.exit_node(node.val, node.id, nodes_[node.parent_id].val, nodes_[node.parent_id].id);
  }
}

} // namespace PIO_Util

#endif /* __SPIO_TREE_HPP__ */
