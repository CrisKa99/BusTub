//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/b_plus_tree_page.h"

namespace bustub {

/*
 * Helper methods to get/set page type
 * Page type enum class is defined in b_plus_tree_page.h
 */
auto BPlusTreePage::IsLeafPage() const -> bool { 
    if (page_type_ == IndexPageType::LEAF_PAGE)
        return true; 
    return false;
}
void BPlusTreePage::SetPageType(IndexPageType page_type) {
    page_type_ = page_type;
}
bool BPlusTreePage::IsRootPage() const{
    if (parent_page_id_ == INVALID_PAGE_ID) 
        return true; 
    return false;
}

/*
 * Helper methods to get/set size (number of key/value pairs stored in that
 * page)
 */
auto BPlusTreePage::GetSize() const -> int { 
    return size_;
}
void BPlusTreePage::SetSize(int size) {
    size_ = size;
}
void BPlusTreePage::IncreaseSize(int amount) {
    size_ += amount;
}

/*
 * Helper methods to get/set max size (capacity) of the page
 */
auto BPlusTreePage::GetMaxSize() const -> int {
    return max_size_;
}
void BPlusTreePage::SetMaxSize(int size) {
    max_size_ = size;
}

/*
 * Helper method to get min page size
 * Generally, min page size == max page size / 2
 */
auto BPlusTreePage::GetMinSize() const -> int { return 0; 
    if (IsRootPage()) {//1表示只有一个指针
        if (IsLeafPage()) return 1;//如果一个页既是根节点又是叶子节点，那么说明是空树，返回1
        else return 2;//至少有一个叶子节点
    }
    return (max_size_) / 2;//不然就返回最大规格的一半。
    }
}  // namespace bustub
