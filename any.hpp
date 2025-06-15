
#include <vector>

struct Any {
    std::vector<char> storage;

    template<typename T>
    T* asA() {
        if (storage.size() < sizeof(T)) {
            storage.resize(sizeof(T));
        }
        return reinterpret_cast<T*>(storage.data());
    }
    
};