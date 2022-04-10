#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    
    RawMemory(const RawMemory<T>&) = delete;
    
    RawMemory(RawMemory<T>&& other) noexcept {
        if(this != &other) {
            std::swap(buffer_, other.buffer_);
            std::swap(capacity_, other.capacity_);
            Deallocate(other.buffer_);
        }
    }
    
    RawMemory& operator=(const RawMemory<T>&) = delete;
    
    RawMemory& operator=(RawMemory<T>&& other) noexcept {
        if(this != &other) {
            std::swap(buffer_, other.buffer_);
            std::swap(capacity_, other.capacity_);
            Deallocate(other.buffer_);
        }
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator begin() const noexcept;
    
    iterator end() noexcept;
    const_iterator cend() const noexcept;
    const_iterator end() const noexcept;

    Vector() = default;
    explicit Vector(size_t size_);
    Vector(const Vector<T>& other);
    Vector(Vector<T>&& other) noexcept;
    
    Vector<T>& operator=(const Vector<T>& other);
    Vector<T>& operator=(Vector<T>&& other) noexcept;
    
    ~Vector();
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Reserve(size_t new_capacity);
    void Resize(size_t new_size);
    
    void Swap(Vector<T>& other) noexcept;
    
    void PushBack(const T& value);
    void PushBack(T&& value);
    void PopBack() noexcept;
    
    iterator Insert(const_iterator pos, const T& value);
    iterator Insert(const_iterator pos, T&& value);
    
    iterator Erase(const_iterator pos) noexcept;
    
    template<typename... Types>
    T& EmplaceBack(Types&&... args) {
        if(size_ < Capacity()) {
            try {
                new(data_ + size_)T(std::forward<Types>(args)...);
            }
            catch(...) {
                std::destroy_at(data_ + size_);
                throw;
            }
        }
        else {
            RawMemory<T> new_data{size_ == 0 ? 1 : 2 * size_};
            try{
                new(new_data + size_)T(std::forward<Types>(args)...);
                if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
                else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
            }
            catch(...) {
                std::destroy_n(new_data.GetAddress(), size_ + 1);
                throw;
            }       
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        return (*this)[size_++];
    }
    
    template<typename... Types>
    iterator Emplace(const_iterator pos, Types&&... value) {
        if(pos == end()) {
            EmplaceBack(std::forward<Types>(value)...);
            return end() - 1u;
        }
        size_t offset = pos - begin();
        if(size_ < Capacity()) {
            T temp_value(std::forward<Types>(value)...);
            try {
                std::uninitialized_move_n(end() - 1u, 1u, end());
                std::move_backward(begin() + offset, end() - 1u, end());
            }
            catch(...) {
                std::destroy_n(data_.GetAddress(), size_ - offset);
                throw;
            }
            (*this)[offset] = std::forward<T>(temp_value);
        }
        else {
            RawMemory<T> new_data{size_ == 0 ? 1 : 2 * size_};
            new(new_data + offset)T(std::forward<Types>(value)...);
            try {
                /*
                if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(begin(), offset, new_data.GetAddress());
                }
                else {
                    std::uninitialized_copy_n(begin(), offset, new_data.GetAddress());
                }
                */
                RealocateElements(begin(), offset, new_data.GetAddress());
            }
            catch(...) {
                std::destroy_at(new_data + offset);
                throw;
            }
            try {
                /*
                if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(begin() + offset, size_ - offset, new_data + offset + 1u);
                }
                else {
                    std::uninitialized_copy_n(begin() + offset, size_ - offset, new_data + offset + 1u);
                }
                */
                RealocateElements(begin() + offset, size_ - offset, new_data + offset + 1u);
            }
            catch(...) {
                std::destroy_n(new_data.GetAddress(), offset + 1u);
                throw;
            }
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        ++size_;
        return begin() + offset;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
    void RealocateElements(const T* from, size_t count, const T* to);
};

template<typename T>
typename Vector<T>::iterator Vector<T>::begin() noexcept {
    return data_ + 0u;
}

template<typename T>
typename Vector<T>::const_iterator Vector<T>::cbegin() const noexcept {
    return data_ + 0u;
}

template<typename T>
typename Vector<T>::const_iterator Vector<T>::begin() const noexcept {
    return data_ + 0u;
}

template<typename T>
typename Vector<T>::iterator Vector<T>::end() noexcept {
    return data_ + size_;
}

template<typename T>
typename Vector<T>::const_iterator Vector<T>::cend() const noexcept {
    return data_ + size_;
}

template<typename T>
typename Vector<T>::const_iterator Vector<T>::end() const noexcept {
    return data_ + size_;
}

template<typename T>
Vector<T>::Vector(size_t size) : data_(RawMemory<T>(size)),
                                 size_(size) {
    std::uninitialized_value_construct_n(data_.GetAddress(), size);
}

template<typename T>
Vector<T>::Vector(const Vector<T>& other) : data_(RawMemory<T>(other.size_)),
                                            size_(other.size_) {
    std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
}

template<typename T>
Vector<T>::Vector(Vector<T>&& other) noexcept : data_(RawMemory<T>(std::move(other.data_))),
                                                size_(other.size_) {
    other.size_ = 0;
}

template<typename T>
Vector<T>& Vector<T>::operator=(const Vector<T>& other) {
    if(this != &other) {
        if(other.size_ > Capacity()) {
            Vector<T> temp_vector{other};
            Swap(temp_vector);
        }
        else {
            std::copy_n(other.begin(), other.size_, begin());
            if(other.size_ < size_) {
                std::destroy_n(data_ + other.size_, size_ - other.size_);
            }
            else {
                std::uninitialized_copy_n(other.data_ + size_, other.size_ - size_, data_ + size_);
            }
            size_ = other.size_;
        }
    }
    return *this;
}

template<typename T>
Vector<T>& Vector<T>::operator=(Vector<T>&& other) noexcept {
    if(this != &other) {
        Swap(other);
        size_ = other.size_;
    }
    return *this;
}

template<typename T>
Vector<T>::~Vector() {
    std::destroy_n(data_.GetAddress(), size_);
}

template<typename T>
void Vector<T>::Reserve(size_t new_capacity) {
    if(new_capacity <= Capacity()) {
        return;
    }
    RawMemory<T> new_data_ptr{new_capacity};
    /*
    if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(data_.GetAddress(), size_, new_data_ptr.GetAddress());
    }
    else {
        std::uninitialized_copy_n(data_.GetAddress(), size_, new_data_ptr.GetAddress());
    }
    */
    RealocateElements(data_.GetAddress(), size_, new_data_ptr.GetAddress());
    new_data_ptr.Swap(data_);
    std::destroy_n(data_.GetAddress(), size_);
}

template<typename T>
void Vector<T>::Resize(size_t new_size) {
    if(new_size > size_) {
        Reserve(new_size);
        std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
    }
    else {
        std::destroy_n(data_ + new_size, size_ - new_size);
    }
    size_ = new_size;
}

template<typename T>
void Vector<T>::Swap(Vector<T>& other) noexcept {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
}

template<typename T>
void Vector<T>::PushBack(const T& value) {
    if(size_ < Capacity()) {
        try {
            new(data_ + size_)T{value};
        }
        catch(...) {
            std::destroy_at(data_ + size_);
            throw;
        }
    }
    else {
        RawMemory<T> new_data{size_ == 0 ? 1 : 2 * size_};
        try{
            new(new_data + size_)T{value};
            /*
            if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            */
            RealocateElements(data_.GetAddress(), size_, new_data.GetAddress());
        }
        catch(...) {
            std::destroy_n(new_data.GetAddress(), size_ + 1);
            throw;
        }
            
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
    }
    ++size_;
}

template<typename T>
void Vector<T>::PushBack(T&& value) {
    if(size_ < Capacity()) {
        try {
            new(data_ + size_)T{std::move(value)};
        }
        catch(...) {
            std::destroy_at(data_ + size_);
            throw;
        }
    }
    else {
        RawMemory<T> new_data{size_ == 0 ? 1 : 2 * size_};
        try {
            new(new_data + size_)T{std::move(value)};
            /*
            if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {  
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            */
            RealocateElements(data_.GetAddress(), size_, new_data.GetAddress());
        }
        catch(...) {
            std::destroy_n(new_data.GetAddress(), size_ + 1);
            throw;
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }
    ++size_;
}

template<typename T>
void Vector<T>::PopBack() noexcept {
    std::destroy_at(data_ + --size_);
}

template<typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, const T& value) {
    return Emplace(pos, value);
}

template<typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, T&& value) {
    return Emplace(pos, std::move(value));
}

template<typename T>
typename Vector<T>::iterator Vector<T>::Erase(const_iterator pos) noexcept {
    size_t offset = pos - begin();
    std::move(begin() + offset + 1u, end(), begin() + offset);
    std::destroy_at(end() - 1u);
    --size_;
    return begin() + offset;
}

template<typename T>
void Vector<T>::RealocateElements(const T* from, size_t count, const T* to) {
    if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(from, count, to);
    }
    else {
        std::uninitialized_copy_n(from, count, to);
    }
}
