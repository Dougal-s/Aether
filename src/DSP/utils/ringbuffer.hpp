#ifndef RINGBUFFER_HPP
#define RINGBUFFER_HPP

#include <algorithm>
#include <cstddef>

template<class T>
struct Ringbuffer {

	Ringbuffer() : Ringbuffer(0) {}
	explicit Ringbuffer(size_t sz) : size{sz}, buf{new T[sz]} { clear(); }

	Ringbuffer(const Ringbuffer&) = delete;
	Ringbuffer& operator=(const Ringbuffer&) = delete;

	~Ringbuffer() { delete[] buf; }

	void push(T value) noexcept {
		++end;
		end -= (end >= size ? size : 0);
		buf[end] = value;
	}

	void clear() noexcept { std::fill_n(buf, size, T()); }

	void swap(Ringbuffer& other) noexcept {
		std::swap(end, other.end);
		std::swap(size, other.size);
		std::swap(buf, other.buf);
	}

	size_t end = 0;
	size_t size;
	T* buf;
};

namespace std {
	template <class T>
	inline void swap(Ringbuffer<T>& lhs, Ringbuffer<T>& rhs) noexcept {
		lhs.swap(rhs);
	}
}

/*
template <class T>
class Ringbuffer {
public:
	class Iterator {
	public:

	private:
		Ringbuffer& m_rb;
		size_t m_pos;
	};

	Ringbuffer& operator=(const Ringbuffer& other) {
		m_sz = other.m_sz;
		delete[] m_buf;
		m_buf = new T[m_sz];
		std::copy_n(other.m_buf, m_sz, m_buf);
	}

	Ringbuffer& operator=(Ringbuffer&& other) {
		this->swap(other);
	}


	size_t size() const noexcept { return size; }

	T& operator[](size_t idx) noexcept { return m_buf[idx]; }

	T* data() { return m_buf; }

	size_t end = 0;
private:
	size_t m_sz;
	T* m_buf;
};
*/

#endif
