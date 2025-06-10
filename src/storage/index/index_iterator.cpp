/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator(){
    if (leaf_ != nullptr) {
    bufferPoolManager_->UnpinPage(leaf_->GetPageId(),false);
  }
} // NOLINT

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool {
    //有可能处于末尾，也有可能是空
  return (nullptr == leaf_ || index_ >= leaf_->GetSize());
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> const MappingType & { 
    return leaf_->GetItem(index_);
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
    if((index_+1) >= leaf_->GetSize()) {
        page_id_t next_page_id = leaf_->GetNextPageId();
        if(next_page_id == INVALID_PAGE_ID) {
          bufferPoolManager_->UnpinPage(leaf_->GetPageId(), false);
          leaf_ = nullptr;
          index_ = 0;
        }
        else {
          bufferPoolManager_->UnpinPage(leaf_->GetPageId(), false);
          Page *page = bufferPoolManager_->FetchPage(next_page_id);
          leaf_ = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page);
          index_ = 0;
        }
      }
      else {
        index_++;
      }
      return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
