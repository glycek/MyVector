#ifndef MyVector_H
#define MyVector_H

#include <initializer_list>
#include <type_traits>

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

template< class T, std::size_t N >
class MyVector;
template< class ET, std::size_t N >
class MyVectorView;

template< class T, std::size_t N >
using MyVectorConstView = MyVectorView< const T, N >;

namespace nd_detail
{
	template< std::size_t N >
	constexpr std::size_t total_elems(const std::array< std::size_t, N >& dims) noexcept
	{
		if constexpr (N == 0)
			return 0;
		return std::accumulate(dims.begin(), dims.end(), std::size_t{ 1 }, std::multiplies());
	}

	template< class T, std::size_t N >
	struct nested_init
	{
		using type = std::initializer_list< typename nested_init< T, N - 1 >::type >;
	};
	template< class T >
	struct nested_init< T, 1 >
	{
		using type = std::initializer_list< T >;
	};
	template< class T >
	struct nested_init< T, 0 >
	{
		using type = T;
	};

	template< std::size_t N >
	constexpr std::array< std::size_t, N > make_strides(const std::array< std::size_t, N >& d) noexcept
	{
		std::array< std::size_t, N > s{};
		if constexpr (N > 0)
		{
			s[N - 1] = 1;
			for (std::size_t i = N - 1; i-- > 0;)
			{
				s[i] = s[i + 1] * d[i + 1];
			}
		}
		return s;
	}

	template< class Parent >
	class NDIterator;
}	 // namespace nd_detail

template< class ET, std::size_t N >
class MyVectorViewBase
{
	static_assert(N > 0, "MyVectorView intended to only for N > 0");

  public:
	using value_type = std::remove_const_t< ET >;
	using size_type = std::size_t;
	using T_ptr = ET*;
	using T_ref = ET&;
	using iterator = nd_detail::NDIterator< MyVectorViewBase >;
	using const_iterator = nd_detail::NDIterator< const MyVectorViewBase >;
	static constexpr size_type order = N;

  protected:
	friend class nd_detail::NDIterator< MyVectorViewBase >;
	friend class nd_detail::NDIterator< const MyVectorViewBase >;
	friend class MyVector< value_type, N + 1 >;
	friend class MyVector< value_type, N >;

	T_ptr data_{};
	std::array< size_type, N > dims_{};
	std::array< size_type, N > strides_{};

	using ArrayParameterType =
		std::conditional_t< std::is_const_v< ET >, const MyVector< value_type, N >, MyVector< value_type, N > >;

  public:
	MyVectorViewBase() = default;
	MyVectorViewBase(T_ptr data, const std::array< size_type, N >& dims, const std::array< size_type, N >& strides) :
		data_(data), dims_(dims), strides_(strides)
	{
	}

	explicit MyVectorViewBase(ArrayParameterType& arr) : data_(arr.data()), dims_(arr.dims()), strides_(arr.strides()) {}

	template< class It >
	MyVectorViewBase(It first, It last) noexcept
	{
		if (first == last)
		{
			return;
		}

		auto p_view = static_cast< MyVectorViewBase >(first);

		this->strides_ = p_view.strides();
		this->data_ = (*first).data();
		this->dims_[0] = std::distance(first, last);

		if constexpr (N > 1)
		{
			const std::array< size_type, N - 1 >& sub_dims = (*first).dims();
			std::copy(sub_dims.begin(), sub_dims.end(), this->dims_.begin() + 1);
		}
	}

	const MyVectorViewBase* operator->() const noexcept { return this; }
	MyVectorViewBase* operator->() noexcept { return this; }

	T_ptr data() const noexcept { return data_; }
	const std::array< size_type, N >& dims() const noexcept { return dims_; }
	const std::array< size_type, N >& strides() const noexcept { return strides_; }
	[[nodiscard]] size_type count() const noexcept { return dims_.empty() ? 0 : dims_[0]; }
	[[nodiscard]] size_type total_count() const noexcept { return nd_detail::total_elems(dims_); }
	static constexpr size_type dim() noexcept { return N; }

	iterator begin() noexcept { return iterator(this, 0); }
	iterator end() noexcept { return iterator(this, count()); }
	const_iterator begin() const noexcept { return const_iterator(this, 0); }
	const_iterator end() const noexcept { return const_iterator(this, count()); }
	const_iterator cbegin() const noexcept { return begin(); }
	const_iterator cend() const noexcept { return end(); }
};

template< class ET, std::size_t N >
class MyVectorView : public MyVectorViewBase< ET, N >
{
  public:
	using MyVectorViewBase< ET, N >::MyVectorViewBase;
	using typename MyVectorViewBase< ET, N >::size_type;
	MyVectorView< ET, N - 1 > operator[](size_type i) const
	{
		if (i >= this->dims_[0])
			throw std::out_of_range(
				"MyVector::at(i=" + std::to_string(i) + ") - index is out of bound for size=" + std::to_string(this->dims_[0]));

		std::array< size_type, N - 1 > sub_dims;
		std::array< size_type, N - 1 > sub_strides;

		std::copy(this->dims_.begin() + 1, this->dims_.end(), sub_dims.begin());
		std::copy(this->strides_.begin() + 1, this->strides_.end(), sub_strides.begin());

		return MyVectorView< ET, N - 1 >(this->data_ + i * this->strides_[0], sub_dims, sub_strides);
	}
};

template< class ET >
class MyVectorView< ET, 1 > : public MyVectorViewBase< ET, 1 >
{
  public:
	using MyVectorViewBase< ET, 1 >::MyVectorViewBase;

	MyVectorViewBase< ET, 1 >::T_ref operator[](MyVectorViewBase< ET, 1 >::size_type i) const
	{
		if (i >= this->dims_[0])
			throw std::out_of_range("MyVectorView::operator[](i=" + std::to_string(i) +
									") - index is out of bound for size=" + std::to_string(this->dims_[0]));

		return static_cast< MyVectorViewBase< ET, 1 >::T_ref >(*(this->data_ + i * this->strides_[0]));
	}
};

template< class T, std::size_t N >
class MyVectorBase
{
  public:
	using value_type = T;
	using size_type = std::size_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using init_list = nd_detail::nested_init< T, N >::type;
	static constexpr size_type order = N;

  protected:
	friend class nd_detail::NDIterator< MyVectorBase >;
	friend class nd_detail::NDIterator< const MyVectorBase >;

	pointer data_ = nullptr;
	std::array< size_type, N > dims_{};
	std::array< size_type, N > strides_{};

	void allocate(size_type n) { data_ = static_cast< pointer >(::operator new(n * sizeof(T))); }

	void deallocate() noexcept
	{
		if (data_)
			::operator delete(data_);
		data_ = nullptr;
	}

	void destroy_elements() noexcept(std::is_nothrow_destructible_v< T >)
	{
		if (data_ && total_count() > 0)
		{
			std::destroy(data_, data_ + total_count());
		}
	}

	void init_shape(const std::array< size_type, N >& d) noexcept
	{
		dims_ = d;
		strides_ = nd_detail::make_strides(dims_);
	}

	template< class It >
	void copy_from_iterator_range(It first, It last)
	{
		pointer current_pos = data_;
		for (auto it = first; it != last; ++it)
		{
			std::construct_at(current_pos++, *it);
		}
	}

  public:
	~MyVectorBase()
	{
		destroy_elements();
		deallocate();
	}

	MyVectorBase() = default;

	MyVectorBase(const MyVectorBase& other)
	{
		init_shape(other.dims_);
		const size_type n = other.total_count();
		if (n == 0)
		{
			data_ = nullptr;
			return;
		}

		allocate(n);

		pointer last = data_;
		try
		{
			last = std::uninitialized_copy_n(other.data(), n, data_);
		} catch (...)
		{
			std::destroy(data_, last);
			deallocate();
			throw;
		}
	}

	MyVectorBase(MyVectorBase&& other) noexcept :
		data_(std::exchange(other.data_, nullptr)), dims_(std::move(other.dims_)), strides_(std::move(other.strides_))
	{
	}

	MyVectorBase& operator=(const MyVectorBase& other)
	{
		if (this == &other)
		{
			return *this;
		}

		MyVectorBase temp(other);
		swap(temp);
		return *this;
	}

	MyVectorBase& operator=(MyVectorBase&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		destroy_elements();
		deallocate();

		data_ = std::exchange(other.data_, nullptr);
		dims_ = std::move(other.dims_);
		strides_ = std::move(other.strides_);

		return *this;
	}

	template< std::integral... Dims >
		requires(sizeof...(Dims) == N)
	explicit MyVectorBase(Dims... dims)
	{
		init_shape({ static_cast< size_type >(dims)... });
		allocate(total_count());
		if (data_ && total_count() > 0)
		{
			std::uninitialized_value_construct_n(data_, total_count());
		}
	}

	MyVectorBase(const std::array< size_type, N >& d, const T& fill_val)
	{
		init_shape(d);
		allocate(total_count());
		if (data_ && total_count() > 0)
		{
			std::uninitialized_fill(data_, data_ + total_count(), fill_val);
		}
	}
	MyVectorBase(const size_type (&d)[N], const T& fill_val) : MyVectorBase(std::to_array(d), fill_val) {}

	MyVectorBase(const std::array< size_type, N >& d, const_pointer src)
	{
		init_shape(d);
		allocate(total_count());
		if (data_ && total_count() > 0)
		{
			std::uninitialized_copy(src, src + total_count(), data_);
		}
	}

	template< class It >
	MyVectorBase(const std::array< size_type, N >& d, It first, It last)
	{
		init_shape(d);
		allocate(total_count());
		if (data_ && total_count() > 0)
		{
			try
			{
				copy_from_iterator_range(first, last);
			} catch (...)
			{
				destroy_elements();
				deallocate();
				throw;
			}
		}
	}

	explicit MyVectorBase(const MyVectorView< T, N >& v)
	{
		init_shape(v.dims());
		allocate(total_count());
		if (data_ && total_count() > 0)
		{
			pointer current_pos = data_;
			copy_from_view_recursive(v, current_pos);
		}
	}

	[[nodiscard]] size_type count() const noexcept { return dims_.empty() ? 0 : dims_[0]; }
	[[nodiscard]] size_type total_count() const noexcept { return nd_detail::total_elems(dims_); }
	static constexpr inline size_type dim() { return N; }

	void swap(MyVectorBase& other) noexcept
	{
		std::swap(data_, other.data_);
		std::swap(dims_, other.dims_);
		std::swap(strides_, other.strides_);
	}

	bool is_equal(const MyVectorBase& other) const
		requires std::equality_comparable< T >
	{
		if (dims_ != other.dims_)
			return false;
		if (total_count() == 0 && other.total_count() == 0)
			return true;
		return std::equal(data_, data_ + total_count(), other.data_);
	}

	pointer data() noexcept { return data_; }
	const_pointer data() const noexcept { return data_; }
	const std::array< size_type, N >& dims() const noexcept { return dims_; }
	const std::array< size_type, N >& strides() const noexcept { return strides_; }
};

template< class T, std::size_t N >
class MyVector : public MyVectorBase< T, N >
{
  public:
	using MyVectorBase< T, N >::MyVectorBase;

	using iterator = nd_detail::NDIterator< MyVector< T, N > >;
	using const_iterator = nd_detail::NDIterator< const MyVector< T, N > >;

	using typename MyVectorBase< T, N >::size_type;
	using typename MyVectorBase< T, N >::pointer;
	using typename MyVectorBase< T, N >::const_pointer;
	using typename MyVectorBase< T, N >::init_list;

  private:
	friend class nd_detail::NDIterator< MyVector >;
	friend class nd_detail::NDIterator< const MyVector >;

	template< class U >
	void find_max_dims_recursive(const std::initializer_list< U >& list, size_type depth)
	{
		if (depth >= N)
			return;
		this->dims_[depth] = std::max(this->dims_[depth], list.size());
		if constexpr (!std::is_same_v< U, T >)
		{
			for (const U& sublist : list)
			{
				find_max_dims_recursive(sublist, depth + 1);
			}
		}
	}

	template< class U >
	void flatten_recursive(const std::initializer_list< U >& list, std::array< size_type, N >& indices, size_type depth)
	{
		size_type i = 0;
		for (const U& item : list)
		{
			indices[depth] = i++;
			if constexpr (std::is_same_v< U, T >)
			{
				size_type flat_index = 0;
				for (size_type k = 0; k < N; ++k)
				{
					flat_index += indices[k] * this->strides_[k];
				}
				new (&this->data_[flat_index]) T(std::move(item));
			}
			else
			{
				flatten_recursive(item, indices, depth + 1);
			}
		}
	}

	template< bool IsConst >
	MyVectorView< T, N > reshape_impl(std::initializer_list< size_type > new_dims_list) const
	{
		std::array< size_type, N > new_dims_arr;
		std::copy(new_dims_list.begin(), new_dims_list.end(), new_dims_arr.begin());
		if (this->total_count() != nd_detail::total_elems(new_dims_arr))
		{
			throw std::invalid_argument("reshape: numbers of elements are different");
		}

		std::conditional_t< IsConst, const_pointer, pointer > data_ptr = this->data_;

		return MyVectorView< T, N >(data_ptr, new_dims_arr, nd_detail::make_strides< N >(new_dims_arr));
	}

	template< size_type M >
		requires(M > 1)
	void copy_from_view_recursive(const MyVectorView< T, M >& view, pointer& dest) const
	{
		for (size_type i = 0; i < view.count(); ++i)
		{
			copy_from_view_recursive(view[i], dest);
		}
	}

	template< size_type M >
		requires(M == 1)
	void copy_from_view_recursive(const MyVectorView< T, M >& view, pointer& dest) const
	{
		for (size_type i = 0; i < view.count(); ++i)
		{
			std::construct_at(dest, view[i]);
			++dest;
		}
	}

	template< class E >
	MyVectorView< E, N - 1 > view(T* base, size_type i) const noexcept
	{
		std::array< size_type, N - 1 > sub_dims;
		std::array< size_type, N - 1 > sub_strides;
		std::copy(this->dims_.begin() + 1, this->dims_.end(), sub_dims.begin());
		std::copy(this->strides_.begin() + 1, this->strides_.end(), sub_strides.begin());
		return MyVectorView< E, N - 1 >(base + i * this->strides_[0], sub_dims, sub_strides);
	}

  public:
	explicit MyVector(init_list list)
	{
		std::fill(this->dims_.begin(), this->dims_.end(), 0);
		find_max_dims_recursive(list, 0);

		this->init_shape(this->dims_);
		this->allocate(this->total_count());

		if (this->data_ && this->total_count() > 0)
		{
			std::array< size_type, N > current_indices{};
			flatten_recursive(list, current_indices, 0);
		}
	}

	MyVectorView< T, N - 1 > at(size_type i)
	{
		if (i >= this->dims_[0])
			throw std::out_of_range("MyVectorView::operator[](i=" + std::to_string(i) +
									") - index is out of bound for size=" + std::to_string(this->dims_[0]));
		return (*this)[i];
	}

	MyVectorConstView< T, N - 1 > at(size_type i) const
	{
		if (i >= this->dims_[0])
			throw std::out_of_range(
				"MyVector::at(i=" + std::to_string(i) + ") - index is out of bound for size=" + std::to_string(this->dims_[0]));
		return (*this)[i];
	}

	MyVectorView< T, N - 1 > operator[](size_type i) noexcept { return view< T >(this->data_, i); }

	MyVectorConstView< T, N - 1 > operator[](size_type i) const noexcept { return view< const T >(this->data_, i); }

	auto reshape(std::initializer_list< size_type > new_dims_list) { return reshape_impl< false >(new_dims_list); }

	auto reshape(std::initializer_list< size_type > new_dims_list) const { return reshape_impl< true >(new_dims_list); }

	pointer data() noexcept { return this->data_; }
	const_pointer data() const noexcept { return this->data_; }
	const std::array< size_type, N >& dims() const noexcept { return this->dims_; }
	const std::array< size_type, N >& strides() const noexcept { return this->strides_; }

	iterator begin() noexcept { return iterator(this, 0); }
	iterator end() noexcept { return iterator(this, this->count()); }
	const_iterator begin() const noexcept { return const_iterator(this, 0); }
	const_iterator end() const noexcept { return const_iterator(this, this->count()); }
	const_iterator cbegin() const noexcept { return begin(); }
	const_iterator cend() const noexcept { return end(); }
};

template< class T >
class MyVector< T, 1 > : public MyVectorBase< T, 1 >
{
  public:
	using typename MyVectorBase< T, 1 >::size_type;
	using typename MyVectorBase< T, 1 >::pointer;
	using typename MyVectorBase< T, 1 >::const_pointer;
	using typename MyVectorBase< T, 1 >::reference;
	using typename MyVectorBase< T, 1 >::const_reference;

	using iterator = pointer;
	using const_iterator = const_pointer;

	using init_list = std::initializer_list< T >;
	static constexpr size_type order = 1;

	MyVector(init_list list)
	{
		const size_type size = list.size();
		this->dims_[0] = size;

		if (size == 0)
		{
			this->data_ = nullptr;
			return;
		}

		this->allocate(this->total_count());
		std::uninitialized_copy(list.begin(), list.end(), this->data_);
	}

	reference operator[](size_type i) noexcept { return this->data_[i]; }
	const_reference operator[](size_type i) const noexcept { return this->data_[i]; }

	reference at(size_type i)
	{
		if (i >= this->dims_[0])
			throw std::out_of_range("at(): index is out of bound");
		return this->data_[i];
	}
	const_reference at(size_type i) const
	{
		if (i >= this->dims_[0])
			throw std::out_of_range("at(): index is out of bound");
		return this->data_[i];
	}

	pointer data() noexcept { return this->data_; }
	const_pointer data() const noexcept { return this->data_; }

	iterator begin() noexcept { return this->data_; }
	iterator end() noexcept { return this->data_ + count(); }
	const_iterator begin() const noexcept { return this->data_; }
	const_iterator end() const noexcept { return this->data_ + count(); }
	const_iterator cbegin() const noexcept { return begin(); }
	const_iterator cend() const noexcept { return end(); }

	size_type size() const noexcept { return this->dims_.empty() ? 0 : this->dims_[0]; }
	size_type count() const noexcept { return size(); }
	[[nodiscard]] bool empty() const noexcept { return size() == 0; }
};

template< class T >
class MyVector< T, 0 >
{
  public:
	using size_type = std::size_t;

	MyVector()
		requires std::default_initializable< T >
		: data_(new T())
	{
	}
	explicit MyVector(const T& v) : data_(new T(v)) {}
	~MyVector() { delete data_; }
	MyVector(const MyVector& other) : data_(new T(*other.data_)) {}
	MyVector(MyVector&& other) noexcept : data_(other.data_) { other.data_ = nullptr; }

	MyVector& operator=(MyVector other) noexcept
	{
		swap(other);
		return *this;
	}

	MyVector& operator=(MyVector&& other) noexcept
	{
		if (this != &other)
		{
			delete data_;
			data_ = std::exchange(other.data_, nullptr);
		}
		return *this;
	}

	T& operator*() { return *data_; }
	const T& operator*() const { return *data_; }
	T* data() noexcept { return data_; }
	const T* data() const noexcept { return data_; }
	T* begin() noexcept { return data_; }
	T* end() noexcept { return data_ ? data_ + 1 : nullptr; }
	const T* cbegin() const noexcept { return begin(); }
	const T* cend() const noexcept { return end(); }

	void swap(MyVector& other) noexcept { std::swap(data_, other.data_); }

	[[nodiscard]] static size_type count() { return 1; }

  private:
	T* data_ = nullptr;
};

namespace nd_detail
{
	template< class Parent >
	class NDIterator
	{
	  public:
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		static constexpr std::size_t N = Parent::order;
		using view_type = MyVectorView< typename Parent::value_type, N - 1 >;
		using iterator_category = std::random_access_iterator_tag;
		using value_type = view_type;
		using reference = view_type;
		using pointer = view_type*;

	  private:
		Parent* p_{};
		size_type idx_{};
		mutable view_type buf_{};

	  public:
		NDIterator() noexcept : p_(nullptr), buf_() {}
		NDIterator(Parent* p, size_type i) noexcept : p_(p), idx_(i), buf_() {}

		reference operator*() const { return (*p_)[idx_]; }

		pointer operator->() const
		{
			buf_ = **this;
			return &buf_;
		}
		reference operator[](difference_type d) const { return *(*this + d); }

		NDIterator& operator++() noexcept
		{
			++idx_;
			return *this;
		}
		NDIterator operator++(int) noexcept
		{
			NDIterator tmp = *this;
			++*this;
			return tmp;
		}
		NDIterator& operator--() noexcept
		{
			--idx_;
			return *this;
		}
		NDIterator operator--(int) noexcept
		{
			NDIterator tmp = *this;
			--*this;
			return tmp;
		}
		NDIterator& operator+=(difference_type d) noexcept
		{
			idx_ += d;
			return *this;
		}
		NDIterator& operator-=(difference_type d) noexcept
		{
			idx_ -= d;
			return *this;
		}
		friend NDIterator operator+(const NDIterator& it, difference_type d) noexcept
		{
			return NDIterator(it.p_, it.idx_ + d);
		}
		friend NDIterator operator+(difference_type d, const NDIterator& it) noexcept { return it + d; }
		NDIterator operator-(difference_type d) const noexcept { return NDIterator(p_, idx_ - d); }
		difference_type operator-(const NDIterator& other) const noexcept
		{
			return static_cast< difference_type >(idx_ - other.idx_);
		}

		bool operator==(const NDIterator& other) const noexcept { return p_ == other.p_ && idx_ == other.idx_; }
		auto operator<=>(const NDIterator& other) const noexcept
		{
			if (p_ != other.p_)
				return std::strong_ordering::less;
			return idx_ <=> other.idx_;
		}
	};
}	 // namespace nd_detail

#endif	  // MyVector_H
