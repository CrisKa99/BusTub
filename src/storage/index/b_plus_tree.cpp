#include <sstream>
#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"

namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->FetchPageWrite(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  return root_page_id_ == INVALID_PAGE_ID; 
}

INDEX_TEMPLATE_ARGUMENTS
Page *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, bool leftMost) {
    // 从根节点开始查找目标键所在的叶子页面
    throw Exception(ExceptionType::NOT_IMPLEMENTED, "Implement this for test");
    
    if (root_page_id_ == INVALID_PAGE_ID) {
        throw std::runtime_error("Unexpected. root_page_id is INVALID_PAGE_ID");
    }
    
    Page *page = buffer_pool_manager_->FetchPage(root_page_id_);  // 获取根页面（pin计数+1）
    BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
    
    while (!node->IsLeafPage()) {
        InternalPage *internal_node = reinterpret_cast<InternalPage *>(node);
        page_id_t next_page_id = leftMost ? internal_node->ValueAt(0) 
                                         : internal_node->Lookup(key, comparator_);
        
        Page *next_page = buffer_pool_manager_->FetchPage(next_page_id);  // 获取子节点
        BPlusTreePage *next_node = reinterpret_cast<BPlusTreePage *>(next_page->GetData());
        
        buffer_pool_manager_->UnpinPage(node->GetPageId(), false);  // 释放当前节点
        page = next_page;
        node = next_node;
    }
    
    return page;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
    // 创建一棵新的 B+ 树，并在根节点插入一个键值对
    Page *new_page = buffer_pool_manager_->NewPage(&root_page_id_);
    if (new_page == nullptr) {
        throw std::runtime_error("Out of memory: failed to allocate new page");
    }

    LeafPage *leaf_page = reinterpret_cast<LeafPage *>(new_page->GetData());
    leaf_page->Init(root_page_id_, INVALID_PAGE_ID, leaf_max_size_);

    UpdateRootPageId(1);  
    //插入键值对
    leaf_page->Insert(key, value, comparator_);
    
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *txn) -> bool {
    Context ctx;
    ctx.header_page_ = nullptr;
    ctx.root_page_id_ = INVALID_PAGE_ID;
    result->clear();
    // 获取根页面ID
    ctx.root_page_id_ = GetRootPageId();
    if (ctx.root_page_id_ == INVALID_PAGE_ID) {
        return false;
    }
    // 获取根页面
    ctx.header_page_ = buffer_pool_manager_->FetchPage(ctx.root_page_id_);
    if (ctx.header_page_ == nullptr) {
        throw Exception(EXCEPTION_TYPE_INDEX, "failed to fetch root page");
    }
    // 查找叶子页
    Page *leaf_page = FindLeafPage(key, Operation::READ, txn, &ctx);
    if (leaf_page == nullptr) {
        buffer_pool_manager_->UnpinPage(ctx.root_page_id_, false);
        return false;
    }
    // 转换为叶子节点
    LeafPage *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
    // 在叶子节点中查找键
    ValueType value;
    bool found = false;
    
    // 重复键
    for (int i = 0; i < leaf_node->GetSize(); i++) {
        if (comparator_(key, leaf_node->KeyAt(i)) == 0) {
            result->push_back(leaf_node->ValueAt(i));
            found = true;
        } else if (found) {
            break;
        }
    }
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    buffer_pool_manager_->UnpinPage(ctx.root_page_id_, false);
    return found;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
//当前为空树则创建一个叶子结点并且也是根节点
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *txn) -> bool {
  // Declaration of context instance.
  //边界检查
  if (GetSize() == GetMaxSize()) {
    std::cout << "leaf_page Insert: size=" << GetSize() 
              << ", max_size=" << GetMaxSize() << std::endl;
  }

  //二分查找确定key应该插入的有序位置
  int pos = KeyIndex(key, comparator);

  for (int i = GetSize() - 1; i >= pos; i--) {
    array_[i + 1] = array_[i];  
  }
  array_[pos] = MappingType{key, value};  

  IncreaseSize(1);  // 增加节点元素计数
  return GetSize(); 
}

INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *txn) {
  {
    // 如果树是空的，开始创建新树
    if (IsEmpty()) {
      StartNewTree(key, value);
      return true;
    }
    // 找到要插入的叶子节点
    Page *right_leaf = FindLeafPage(key, false);
    LeafPage *leaf_page = reinterpret_cast<LeafPage *>(right_leaf);

    // 检查插入的键是否已存在
    if (leaf_page->Lookup(key, nullptr, comparator_)) {
      buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
      return false;
    }
    leaf_page->Insert(key, value, comparator_);
    // 检查是否需要分裂
    if (leaf_page->GetSize() == maxSize(leaf_page) + 1) {
      LeafPage *new_leaf = Split(leaf_page);
      InsertIntoParent(leaf_page, new_leaf->KeyAt(0), new_leaf, transaction);
    }
    // 如果不需要分裂，解除对右叶子页面的锁定
    buffer_pool_manager_->UnpinPage(right_leaf->GetPageId(), true);
    return true;
  }
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
N *BPLUSTREE_TYPE::Split(N *node) {
  page_id_t new_page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);  // pinned
  if (new_page == nullptr) {
    throw std::string("out of memory");
  }
  N *new_node;
  if (node->IsLeafPage()) {
    LeafPage *leaf_node = reinterpret_cast<LeafPage *>(node);
    LeafPage *new_leaf_node = reinterpret_cast<LeafPage *>(new_page);
    new_leaf_node->Init(new_page_id, leaf_node->GetParentPageId(),leaf_max_size_);
    leaf_node->MoveHalfTo(new_leaf_node);
    // 叶子节点更新双向链表
    new_leaf_node->SetNextPageId(leaf_node->GetNextPageId());
    leaf_node->SetNextPageId(new_leaf_node->GetPageId());
    new_node = reinterpret_cast<N *>(new_leaf_node);
  } else {
    // 内部节点不需要设置双向链表
    InternalPage *internal_node = reinterpret_cast<InternalPage *>(node);
    InternalPage *new_internal_node = reinterpret_cast<InternalPage *>(new_page);
    new_internal_node->Init(new_page_id, internal_node->GetParentPageId(),internal_max_size_);
    internal_node->MoveHalfTo(new_internal_node,buffer_pool_manager_);
    new_node = reinterpret_cast<N *>(new_internal_node);
  }
  return new_node;
}

//将当前叶子节点的一半元素移动到另一个接收的叶子节点（
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveHalfTo(BPlusTreeLeafPage *recipient) {
  int moved_num = GetSize() - GetSize() / 2;
  int start = GetSize() - moved_num;
  CopyNFrom(array_ + start, moved_num);
  IncreaseSize(-1 * moved_num);
  recipient->IncreaseSize(moved_num);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node, 
                                      const KeyType &key,
                                      BPlusTreePage *new_node,
                                      Transaction *transaction) {
    // 原节点是根节点，创建新根
    if (old_node->IsRootPage()) {
        page_id_t new_root_id;
        Page *new_root_page = buffer_pool_manager_->NewPage(&new_root_id);
        auto *root_node = reinterpret_cast<InternalPage *>(new_root_page->GetData());
        
        root_node->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);
        root_node->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
        
        old_node->SetParentPageId(new_root_id);
        new_node->SetParentPageId(new_root_id);
        
        root_page_id_ = new_root_id;
        UpdateRootPageId(new_root_id);
        
        buffer_pool_manager_->UnpinPage(new_root_id, true);
        return;
    }

    // 非根节点，获取父节点
    page_id_t parent_id = old_node->GetParentPageId();
    Page *parent_page = buffer_pool_manager_->FetchPage(parent_id);
    auto *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());

    parent_node->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    new_node->SetParentPageId(parent_id);

    // 父节点需要分裂
    if (parent_node->GetSize() > parent_node->GetMaxSize()) {
        Page *new_parent_page = Split(parent_node);
        auto *new_parent_node = reinterpret_cast<InternalPage *>(new_parent_page->GetData());
        
        InsertIntoParent(parent_node, new_parent_node->KeyAt(0), new_parent_node, transaction);
        
        buffer_pool_manager_->UnpinPage(new_parent_node->GetPageId(), true);
    }

    buffer_pool_manager_->UnpinPage(parent_id, true);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */

INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_LEAF_PAGE_TYPE::RemoveAndDeleteRecord(const KeyType &key, const KeyComparator &comparator) {
  int pos = KeyIndex(key, comparator);
  if (pos < GetSize() && comparator(array_[pos].first, key) == 0) {
    for (int j = pos + 1; j < GetSize(); j++) {
      array_[j - 1] = array_[j];
    }
    IncreaseSize(-1);
  }
  return GetSize();
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *txn) {
  // Declaration of context instance.
  Page *page = FindLeafPage(key, false);
    LeafPage *leafPage = reinterpret_cast<LeafPage*>(page->GetData()); //pined leafPage

    leafPage->RemoveAndDeleteRecord(key,comparator_);
}

//调整B+树的根节点，以确保根节点满足B+树的性质
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  // 内部结点且大小为1，表示内部结点已经没有key,要把它的孩子更新成新的根节点
  if (!old_root_node->IsLeafPage() && old_root_node->GetSize() == 1) {
    InternalPage *old_root_page = reinterpret_cast<InternalPage *>(old_root_node);
    page_id_t new_root_page_id = old_root_page->adjustRootForInternal();
    root_page_id_ = new_root_page_id;
    UpdateRootPageId(0);

    Page *new_root_page = buffer_pool_manager_->FetchPage(new_root_page_id);
    BPlusTreePage *new_root = reinterpret_cast<BPlusTreePage *>(new_root_page->GetData());
    new_root->SetParentPageId(INVALID_PAGE_ID);
    buffer_pool_manager_->UnpinPage(new_root_page_id, true);

    return true;
  }
  // 叶子结点且大小为0
  if (old_root_node->IsLeafPage() && old_root_node->GetSize() == 0) {
    root_page_id_ = INVALID_PAGE_ID;
    UpdateRootPageId(0);
    return true;
  }
  return false;
}

// 判断是否满足合并要求
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::IsCoalesce(N *nodeL, N *nodeR) {
  // maxSize consider InternalPage and LeafPage
  return nodeL->GetSize() + nodeR->GetSize() <= maxSize(nodeL);
}

//合并
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::Coalesce(N **neighbor_node, N **node,
                              BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> **parent, int index,
                              Transaction *transaction) {
  // Assume that *neighbor_node is the left sibling of *node

  // Move entries from node to neighbor_node
  if ((*node)->IsLeafPage()) {
    LeafPage *leaf_node = reinterpret_cast<LeafPage *>(*node);
    LeafPage *leaf_neighbor = reinterpret_cast<LeafPage *>(*neighbor_node);

    leaf_node->MoveAllTo(leaf_neighbor);
    leaf_neighbor->SetNextPageId(leaf_node->GetNextPageId());
  } else {
    InternalPage *internal_node = reinterpret_cast<InternalPage *>(*node);
    InternalPage *internal_neighbor = reinterpret_cast<InternalPage *>(*neighbor_node);

    internal_node->MoveAllTo(internal_neighbor, (*parent)->KeyAt(index), buffer_pool_manager_);
  }

  buffer_pool_manager_->UnpinPage((*node)->GetPageId(), true);
  buffer_pool_manager_->DeletePage((*node)->GetPageId());

  (*parent)->Remove(index);

  // If parent is underfull, recursive operation
  if ((*parent)->GetSize() < minSize(*parent)) {
    return CoalesceOrRedistribute(*parent, transaction);
  }
  return false;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveAllTo(BPlusTreeLeafPage *r,int, BufferPoolManager *) {
  if(r == nullptr) assert(0);
 
  //copy last half
  int st = r->GetSize();//7 is 4,5,6,7; 8 is 4,5,6,7,8
  for (int i = 0; i < GetSize(); i++) {
    r->array[st + i].first = array[i].first;
    r->array[st + i].second = array[i].second;
  }
  //设置指针
  r->SetNextPageId(GetNextPageId());
  //增加存储大小
  r->IncreaseSize(GetSize());
  SetSize(0);
 
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::CoalesceAndRedistribute(BPlusTreePage *node, 
                                              int left_sib_index, 
                                              int right_sib_index, 
                                              InternalPage *parent, 
                                              int cur_index, 
                                              Transaction *transaction) {
  // 调用 AdjustRoot 检查根节点情况
  if (AdjustRoot(node)) {
    return;  
  }
  // 判断是否需要进行合并
  if (left_sib_index >= 0) {  // 左边
    page_id_t sibling_pid = parent->ValueAt(left_sib_index);
    Page *sibling_page = buffer_pool_manager_->FetchPage(sibling_pid);
    N *sib_node = reinterpret_cast<N *>(sibling_page->GetData());
    
    if (IsCoalesce(node, sib_node)) {
      bool del_parent = Coalesce(&sib_node, &node, &parent, cur_index, transaction);
      buffer_pool_manager_->UnpinPage(sibling_pid, true);
      buffer_pool_manager_->UnpinPage(parent_pid, true);
      if (del_parent) {
        buffer_pool_manager_->DeletePage(parent_pid);
      }
      return false; 
    }
    buffer_pool_manager_->UnpinPage(sibling_pid, false);  // 释放兄弟节点
  }
  // 检查右边
  if (right_sib_index < parent->GetSize()) {
    page_id_t sibling_pid = parent->ValueAt(right_sib_index);
    Page *sibling_page = buffer_pool_manager_->FetchPage(sibling_pid);
    N *sib_node = reinterpret_cast<N *>(sibling_page->GetData());
    
    if (IsCoalesce(node, sib_node)) {
      // 合并
      bool del_parent = Coalesce(&node, &sib_node, &parent, cur_index, transaction);
      buffer_pool_manager_->UnpinPage(node->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(parent_pid, true);
      if (del_parent) {
        buffer_pool_manager_->DeletePage(parent_pid);
      }
      return false; 
    }
    buffer_pool_manager_->UnpinPage(sibling_pid, false);  // 释放兄弟节点
  }
  if (left_sib_index >= 0) {
    // 从左边兄弟节点借
    page_id_t sibling_pid = parent->ValueAt(left_sib_index);
    Page *sibling_page = buffer_pool_manager_->FetchPage(sibling_pid);
    N *sib_node = reinterpret_cast<N *>(sibling_page->GetData());
    Redistribute(sib_node, node, parent, left_sib_index, transaction);
    buffer_pool_manager_->UnpinPage(sibling_pid, true);  // 释放兄弟节点
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);  // 释放父节点
    return true;  
  }

  if (right_sib_index < parent->GetSize()) {
    // 从右边兄弟节点借
    page_id_t sibling_pid = parent->ValueAt(right_sib_index);
    Page *sibling_page = buffer_pool_manager_->FetchPage(sibling_pid);
    N *sib_node = reinterpret_cast<N *>(sibling_page->GetData());
    Redistribute(node, sib_node, parent, right_sib_index, transaction);
    buffer_pool_manager_->UnpinPage(sibling_pid, true);  // 释放兄弟节点
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);  // 释放父节点
    return true;  
  }
}
/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE { 
  KeyType _;
  Page *page = FindLeafPage(_, true);
  B_PLUS_TREE_LEAF_PAGE_TYPE *start_leaf = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page->GetData());
  return INDEXITERATOR_TYPE(start_leaf, 0, buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  Page *page = FindLeafPage(key, false);
  B_PLUS_TREE_LEAF_PAGE_TYPE *start_leaf = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page->GetData());
  if (page == nullptr) {
    return INDEXITERATOR_TYPE(start_leaf, 0, buffer_pool_manager_);
  }
  int idx = start_leaf->KeyIndex(key, comparator_);
  return INDEXITERATOR_TYPE(start_leaf, idx, buffer_pool_manager_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(nullptr, 0, buffer_pool_manager_); }

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return root_page_id_; }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *txn) {
  int64_t key;
  std::ifstream input(file_name);
  while (input >> key) {
    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, txn);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *txn) {
  int64_t key;
  std::ifstream input(file_name);
  while (input >> key) {
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, txn);
  }
}

/*
 * This method is used for test only
 * Read data from file and insert/remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::BatchOpsFromFile(const std::string &file_name, Transaction *txn) {
  int64_t key;
  char instruction;
  std::ifstream input(file_name);
  while (input) {
    input >> instruction >> key;
    RID rid(key);
    KeyType index_key;
    index_key.SetFromInteger(key);
    switch (instruction) {
      case 'i':
        Insert(index_key, rid, txn);
        break;
      case 'd':
        Remove(index_key, txn);
        break;
      default:
        break;
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  auto root_page_id = GetRootPageId();
  auto guard = bpm->FetchPageBasic(root_page_id);
  PrintTree(guard.PageId(), guard.template As<BPlusTreePage>());
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::PrintTree(page_id_t page_id, const BPlusTreePage *page) {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<const LeafPage *>(page);
    std::cout << "Leaf Page: " << page_id << "\tNext: " << leaf->GetNextPageId() << std::endl;

    // Print the contents of the leaf page.
    std::cout << "Contents: ";
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i);
      if ((i + 1) < leaf->GetSize()) {
        std::cout << ", ";
      }
    }
    std::cout << std::endl;
    std::cout << std::endl;

  } else {
    auto *internal = reinterpret_cast<const InternalPage *>(page);
    std::cout << "Internal Page: " << page_id << std::endl;

    // Print the contents of the internal page.
    std::cout << "Contents: ";
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i);
      if ((i + 1) < internal->GetSize()) {
        std::cout << ", ";
      }
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      auto guard = bpm_->FetchPageBasic(internal->ValueAt(i));
      PrintTree(guard.PageId(), guard.template As<BPlusTreePage>());
    }
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Drawing an empty tree");
    return;
  }

  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  auto root_page_id = GetRootPageId();
  auto guard = bpm->FetchPageBasic(root_page_id);
  ToGraph(guard.PageId(), guard.template As<BPlusTreePage>(), out);
  out << "}" << std::endl;
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(page_id_t page_id, const BPlusTreePage *page, std::ofstream &out) {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<const LeafPage *>(page);
    // Print node name
    out << leaf_prefix << page_id;
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << page_id << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << page_id << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << page_id << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }
  } else {
    auto *inner = reinterpret_cast<const InternalPage *>(page);
    // Print node name
    out << internal_prefix << page_id;
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << page_id << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_guard = bpm_->FetchPageBasic(inner->ValueAt(i));
      auto child_page = child_guard.template As<BPlusTreePage>();
      ToGraph(child_guard.PageId(), child_page, out);
      if (i > 0) {
        auto sibling_guard = bpm_->FetchPageBasic(inner->ValueAt(i - 1));
        auto sibling_page = sibling_guard.template As<BPlusTreePage>();
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_guard.PageId() << " " << internal_prefix
              << child_guard.PageId() << "};\n";
        }
      }
      out << internal_prefix << page_id << ":p" << child_guard.PageId() << " -> ";
      if (child_page->IsLeafPage()) {
        out << leaf_prefix << child_guard.PageId() << ";\n";
      } else {
        out << internal_prefix << child_guard.PageId() << ";\n";
      }
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::DrawBPlusTree() -> std::string {
  if (IsEmpty()) {
    return "()";
  }

  PrintableBPlusTree p_root = ToPrintableBPlusTree(GetRootPageId());
  std::ostringstream out_buf;
  p_root.Print(out_buf);

  return out_buf.str();
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::ToPrintableBPlusTree(page_id_t root_id) -> PrintableBPlusTree {
  auto root_page_guard = bpm_->FetchPageBasic(root_id);
  auto root_page = root_page_guard.template As<BPlusTreePage>();
  PrintableBPlusTree proot;

  if (root_page->IsLeafPage()) {
    auto leaf_page = root_page_guard.template As<LeafPage>();
    proot.keys_ = leaf_page->ToString();
    proot.size_ = proot.keys_.size() + 4;  // 4 more spaces for indent

    return proot;
  }

  // draw internal page
  auto internal_page = root_page_guard.template As<InternalPage>();
  proot.keys_ = internal_page->ToString();
  proot.size_ = 0;
  for (int i = 0; i < internal_page->GetSize(); i++) {
    page_id_t child_id = internal_page->ValueAt(i);
    PrintableBPlusTree child_node = ToPrintableBPlusTree(child_id);
    proot.size_ += child_node.size_;
    proot.children_.push_back(child_node);
  }

  return proot;
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub