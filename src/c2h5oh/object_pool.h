#pragma once
#include <list>
#include <stack>

template <class TYPE>
class object_pool
{
public:
  TYPE * object_new() {          // returns new object
    if(free_objects.empty()) {
      if (pool.size() >= max_size) {
        return nullptr;
      } else {
        pool.emplace_back();
        TYPE & res = pool.back();
        return &res;
      }
    } else {
      TYPE * res = free_objects.top(); 
      free_objects.pop();
      return res;
    }
  }
  void object_delete(TYPE * o) { // returns object - mark as unused
    free_objects.push(o);
  }

  void set_max_size(size_t size) { max_size = size; }

  object_pool() : max_size(10) {} // default constructor
  virtual ~object_pool() {}       // virtual destructor

private: 
  object_pool(const object_pool&) = delete;    // disallow assign
  void operator=(const object_pool&) = delete; // disallow copy
  size_t max_size;
  std::list<TYPE>    pool;           // objects list
  std::stack<TYPE *> free_objects;   // free objects stack
};

