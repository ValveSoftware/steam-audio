//
// Copyright 2017-2023 Valve Corporation.
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
//

#pragma once

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>

#include "memory_allocator.h"

namespace ipl {

using string = std::basic_string<char, std::char_traits<char>, allocator<char>>;

template <typename T>
using vector = std::vector<T, allocator<T>>;

template <typename T>
using deque = std::deque<T, allocator<T>>;

template <typename T>
using forward_list = std::forward_list<T, allocator<T>>;

template <typename T>
using list = std::list<T, allocator<T>>;

template <typename T, typename C = std::less<T>>
using set = std::set<T, C, allocator<T>>;

template <typename K, typename V, typename C = std::less<K>>
using map = std::map<K, V, C, allocator<std::pair<const K, V>>>;

template <typename T, typename H = std::hash<T>, typename E = std::equal_to<T>>
using unordered_set = std::unordered_set<T, H, E, allocator<T>>;

template <typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using unordered_map = std::unordered_map<K, V, H, E, allocator<std::pair<const K, V>>>;

template <typename T, typename C = vector<T>>
using stack = std::stack<T, C>;

template <typename T, typename C = list<T>>
using queue = std::queue<T, C>;

template <typename T, typename C = std::less<T>>
using priority_queue = std::priority_queue<T, vector<T>, C>;

}
