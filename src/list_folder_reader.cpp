// Copyright 2021 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/list_folder_reader.h"

namespace csa {
// ListFolderReader::iterator must satisfy the requirements of an
// InputIterator.
static_assert(
    std::is_same<std::iterator_traits<ListFolderReader::iterator>::iterator_category, std::input_iterator_tag>::value,
    "ListFolderReader::iterator should be an InputIterator");
static_assert(std::is_same<std::iterator_traits<ListFolderReader::iterator>::value_type,
                           StatusOrVal<internal::ListFolderResponse::MetadataItem>>::value,
              "ListFolderReader::iterator should be an InputIterator of internal::ListFolderResponse::MetadataItem");
static_assert(std::is_same<std::iterator_traits<ListFolderReader::iterator>::pointer,
                           StatusOrVal<internal::ListFolderResponse::MetadataItem>*>::value,
              "ListFolderReader::iterator should be an InputIterator of internal::ListFolderResponse::MetadataItem");
static_assert(std::is_same<std::iterator_traits<ListFolderReader::iterator>::reference,
                           StatusOrVal<internal::ListFolderResponse::MetadataItem>&>::value,
              "ListFolderReader::iterator should be an InputIterator of internal::ListFolderResponse::MetadataItem");
static_assert(std::is_copy_constructible<ListFolderReader::iterator>::value,
              "ListFolderReader::iterator must be CopyConstructible");
static_assert(std::is_move_constructible<ListFolderReader::iterator>::value,
              "ListFolderReader::iterator must be MoveConstructible");
static_assert(std::is_copy_assignable<ListFolderReader::iterator>::value,
              "ListFolderReader::iterator must be CopyAssignable");
static_assert(std::is_move_assignable<ListFolderReader::iterator>::value,
              "ListFolderReader::iterator must be MoveAssignable");
static_assert(std::is_destructible<ListFolderReader::iterator>::value,
              "ListFolderReader::iterator must be Destructible");
static_assert(std::is_convertible<decltype(*std::declval<ListFolderReader::iterator>()),
                                  ListFolderReader::iterator::value_type>::value,
              "*it when it is of ListFolderReader::iterator type must be convertible to "
              "ListFolderReader::iterator::value_type>");
static_assert(std::is_same<decltype(++std::declval<ListFolderReader::iterator>()), ListFolderReader::iterator&>::value,
              "++it when it is of ListFolderReader::iterator type must be a "
              "ListFolderReader::iterator &>");

}  // namespace csa
