// Copyright 2019 Andrew Karasyov
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

#pragma once

#include "cloudstorageapi/status_or_val.h"
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace csa {
namespace internal {

/** 
 *  Single pass input iterator over given items from owner.
 *  It asks owner for next item and if next item is on
 *  the next page owner is responsible to load it first.
 */
template <typename T, typename Range>
class PaginationIterator
{
public:
    //@{
    /// @name Iterator traits
    using iterator_category = std::input_iterator_tag;
    using value_type = StatusOrVal<T>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    //@}

    PaginationIterator() : m_owner(nullptr) {}

    PaginationIterator& operator++()
    {
        *this = m_owner->GetNext();
        return *this;
    }

    PaginationIterator const operator++(int)
    {
        PaginationIterator tmp(*this);
        operator++();
        return tmp;
    }

    value_type const* operator->() const { return &m_value; }
    value_type* operator->() { return &m_value; }

    value_type const& operator*() const& { return m_value; }
    value_type& operator*()& { return m_value; }
    value_type const&& operator*() const&& { return std::move(m_value); }
    value_type&& operator*()&& { return std::move(m_value); }

private:
    friend Range;

    friend bool operator==(PaginationIterator const& lhs,
        PaginationIterator const& rhs)
    {
        // Iterators on different streams are always different.
        if (lhs.m_owner != rhs.m_owner)
        {
            return false;
        }
        // All end iterators are equal.
        if (lhs.m_owner == nullptr)
        {
            return true;
        }
        // Iterators on the same stream are equal if they point to the same object.
        if (lhs.m_value.Ok() && rhs.m_value.Ok()) {
            return *lhs.m_value == *rhs.m_value;
        }
        // If one is an error and the other is not then they must be different,
        // because only one iterator per range can have an error status. For the
        // same reason, if both have an error they both are pointing to the same
        // element.
        return lhs.m_value.Ok() == rhs.m_value.Ok();
    }

    friend bool operator!=(PaginationIterator const& lhs,
        PaginationIterator const& rhs)
    {
        return std::rel_ops::operator!=(lhs, rhs);
    }

    explicit PaginationIterator(Range* owner, value_type value)
        : m_owner(owner), m_value(std::move(value)) {}

    Range* m_owner;
    value_type m_value;
};

template <typename T, typename Request, typename Response>
class PaginationRange {
public:
    explicit PaginationRange(
        Request request,
        std::function<StatusOrVal<Response>(Request const& r)> loader)
        : m_request(std::move(request)),
        m_nextPageLoader(std::move(loader)),
        m_nextPageToken(),
        m_onLastPage(false)
    {
        m_current = m_currentPage.begin();
    }

    /// The iterator type for this Range.
    using iterator = PaginationIterator<T, PaginationRange>;

    /**
     * Return an iterator over the list of items.
     *
     * The returned iterator is a single-pass input iterator.
     *
     * Creating, and particularly incrementing, multiple iterators on the same
     * ListHmacKeysReader is unsupported and can produce incorrect results.
     */
    iterator begin() { return GetNext(); }

    /// Return an iterator pointing to the end of the stream.
    iterator end() { return PaginationIterator<T, PaginationRange>{}; }

protected:
    friend class PaginationIterator<T, PaginationRange>;

    /**
     * Fetches (or returns if already fetched) the next object from the stream.
     *
     * @return An iterator pointing to the next element in the stream. On error,
     *   it returns an iterator that is different from `.end()`, but has an error
     *   status. If the stream is exhausted, it returns the `.end()` iterator.
     */
    iterator GetNext()
    {
        static Status const pastTheEndError(StatusCode::FailedPrecondition,
            "Cannot iterating past the end of pagination range.");
        
        if (m_currentPage.end() == m_current)
        {
            if (m_onLastPage)
            {
                return iterator(nullptr, pastTheEndError);
            }
            m_request.SetPageToken(std::move(m_nextPageToken));
            auto response = m_nextPageLoader(m_request);
            if (!response.Ok())
            {
                m_nextPageToken.clear();
                m_currentPage.clear();
                m_onLastPage = true;
                m_current = m_currentPage.begin();
                return iterator(this, std::move(response).GetStatus());
            }
            m_nextPageToken = std::move(response->m_nextPageToken);
            m_currentPage = std::move(response->m_items);
            m_current = m_currentPage.begin();
            if (m_nextPageToken.empty())
            {
                m_onLastPage = true;
            }
            if (m_currentPage.end() == m_current)
            {
                return iterator(nullptr, pastTheEndError);
            }
        }
        return iterator(this, std::move(*m_current++));
    }

private:
    Request m_request;
    std::function<StatusOrVal<Response>(Request const& r)> m_nextPageLoader;
    std::vector<T> m_currentPage;
    typename std::vector<T>::iterator m_current;
    std::string m_nextPageToken;
    bool m_onLastPage;
};

}  // namespace internal
}  // namespace csa
