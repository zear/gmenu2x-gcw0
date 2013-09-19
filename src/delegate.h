#ifndef __DELEGATE_H__
#define __DELEGATE_H__

#include <functional>

typedef std::function<void(void)> function_t;

#define BIND(function) std::bind(function, this)

#endif /* __DELEGATE_H__ */
