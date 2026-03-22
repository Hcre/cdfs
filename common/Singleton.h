
#ifndef SINGLETON_H
#define SINGLETON_H

#include "const.h"

template<typename T>
class singleton {
protected:
    singleton() = default;
    singleton(const singleton&) = delete;
    singleton& operator=(const singleton&) = delete;
    static std::shared_ptr<T> _instance;

public:
    static std::shared_ptr<T> getInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() { //多线程初始化， 只会初始化一次
            _instance = std::shared_ptr<T>(new T);
            });
        return _instance;
    }

    void getAddr() {
        std::cout << _instance.get() << std::endl;
    }

    ~singleton() {
        std::cout << "singleton destruct" << std::endl;
    }
};

template<typename T>
std::shared_ptr<T> singleton<T>::_instance = nullptr;

#endif // SINGLETON_H