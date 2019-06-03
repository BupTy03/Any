#ifndef ANY_HPP
#define ANY_HPP

#include <iostream>
#include <exception>
#include <type_traits>
#include <cassert>
#include <cstdlib>

struct any
{
	explicit any() noexcept {}

	template<class T, class = std::enable_if_t<!std::is_same_v<std::decay_t<T>, any>>>
	explicit any(T&& val)
	{
		constexpr auto st_type = (use_internal_storage<T>::value) ? storage_type::INTERNAL : storage_type::EXTERNAL;
		storage_operator_ = &(storage_operator<T, st_type>::do_things);
		storage_strategy<T, st_type>::construct(storage_, std::forward<T>(val));
	}

	~any() { reset(); }

	any(const any& other)
		: storage_operator_{other.storage_operator_}
	{
		storage_operator_(storage_operation::COPY, const_cast<any&>(other), this);
	}
	any& operator=(const any& other)
	{
		any(other).swap(*this);
		return *this;
	}

	any(any&& other) noexcept
		: storage_operator_{other.storage_operator_}
	{
		other.storage_operator_(storage_operation::MOVE, other, this);
		other.storage_operator_ = nullptr;
	}
	any& operator=(any&& other) noexcept
	{
		any(std::move(other)).swap(*this);
		return *this;
	}

	template<class T, class... Args>
	void emplace(Args&&... args)
	{
		reset();
		constexpr auto st_type = (use_internal_storage<T>::value) ? storage_type::INTERNAL : storage_type::EXTERNAL;
		storage_operator_ = &(storage_operator<T, st_type>::do_things);
		try {
			storage_operator_(storage_strategy<T, st_type>::construct(storage_, std::forward<Args>(args)...));
		}
		catch (const std::exception& exc) {
			reset();
			throw exc;
		}
	}

	void reset()
	{
		if (storage_operator_ != nullptr) {
			storage_operator_(storage_operation::DESTROY, *this, nullptr);
			storage_operator_ = nullptr;
		}
	}

	void swap(any& other)
	{
		if (this == &other) {
			return;
		}

		if (has_value() && other.has_value()){
			any tmp;
			tmp.storage_operator_ = other.storage_operator_;
			other.storage_operator_(storage_operation::MOVE, other, &tmp);

			other.storage_operator_ = storage_operator_;
			storage_operator_(storage_operation::MOVE, *this, &other);

			storage_operator_ = tmp.storage_operator_;
			tmp.storage_operator_(storage_operation::MOVE, tmp, this);
		}
		else if (!has_value() && other.has_value()) {
			std::swap(storage_operator_, other.storage_operator_);
			storage_operator_(storage_operation::MOVE, other, this);
		}
		else if (has_value() && !other.has_value()) {
			std::swap(storage_operator_, other.storage_operator_);
			storage_operator_(storage_operation::MOVE, *this, &other);
		}
	}

	bool has_value() const { return storage_operator_ != nullptr; }
	const std::type_info& type() const
 	{
		if (has_value()) {
			auto pTypeInfo = storage_operator_(storage_operation::TYPE_INFO, const_cast<any&>(*this), nullptr);
			return (*static_cast<const std::type_info*>(pTypeInfo));
		}
		else {
			return typeid(void);
		}
	}

	template<class T>
	T cast(bool* success = nullptr) const
	{
		const bool is_valid_cast = (has_value() && (type() == typeid(std::remove_reference_t<T>)));

		if (success != nullptr) {
			*success = is_valid_cast;

			return is_valid_cast
				? (*static_cast<T*>(storage_operator_(storage_operation::GET, const_cast<any&>(*this), nullptr)))
					: T();
		}

		if (!is_valid_cast) {
			throw std::exception("invalid any cast");
		}

		return (*static_cast<T*>(storage_operator_(storage_operation::GET, const_cast<any&>(*this), nullptr)));
	}

private:

	union storage
	{
		using internal_storage_t = std::aligned_storage_t<4 * sizeof(void*), alignof(void*)>;

		void* external_storage{ nullptr };
		internal_storage_t internal_storage;
	};

	template<class T>
	using use_internal_storage = std::bool_constant
	<
		(sizeof(T) <= sizeof(storage)) &&
		(alignof(storage) % alignof(T) == 0)
	>;

	enum class storage_type
	{
		INTERNAL, EXTERNAL
	};

	template<class T, storage_type ST>
	struct storage_strategy { };

	template<class T>
	struct storage_strategy<T, storage_type::INTERNAL>
	{
		static inline void construct(storage& s, const T& val)
		{
			::new(static_cast<void*>(&s.internal_storage)) T(val);
		}

		static inline void construct(storage& s, T&& val)
		{
			::new(static_cast<void*>(&s.internal_storage)) T(std::forward<T>(val));
		}

		template<class... Args>
		static inline void construct_inplace(storage& s, Args&&... args)
		{
			::new(static_cast<void*>(&s.internal_storage)) T(std::forward<Args>(args)...);
		}

		static inline void destroy(storage& s)
		{
			T& t = *(reinterpret_cast<T*>(&s.internal_storage));
			t.~T();
		}

		static inline T* get(storage& s)
		{
			return reinterpret_cast<T*>(&s.internal_storage);
		}

		static inline void copyStorage(storage& from, storage& to)
		{
			construct(to, *(reinterpret_cast<T*>(&from.internal_storage)));
		}

		static inline void moveStorage(storage& from, storage& to)
		{
			construct(to, std::move(*(reinterpret_cast<T*>(&from.internal_storage))));
			destroy(from);
		}
	};

	template<class T>
	struct storage_strategy<T, storage_type::EXTERNAL>
	{
		static inline void construct(storage& s, const T& val)
		{
			s.external_storage = new T(val);
		}

		static inline void construct(storage& s, T&& val)
		{
			s.external_storage = new T(std::forward<T>(val));
		}

		template<class... Args>
		static inline void construct_inplace(storage& s, Args&& ... args)
		{
			s.external_storage = new T(std::forward<Args>(args)...);
		}

		static inline void destroy(storage& s)
		{
			delete static_cast<T*>(s.external_storage);
		}

		static inline T* get(storage& s)
		{
			return static_cast<T*>(s.external_storage);
		}

		static inline void copyStorage(storage& from, storage& to)
		{
			construct(to, *(static_cast<T*>(from.external_storage)));
		}

		static inline void moveStorage(storage& from, storage& to)
		{
			construct(to, std::move(*(static_cast<T*>(from.external_storage))));
			destroy(from);
		}
	};

	enum class storage_operation
	{
		GET, DESTROY, COPY, MOVE, TYPE_INFO
	};

	template<class T, storage_type StorageType>
	struct storage_operator
	{
		static void* do_things(storage_operation operation, any& rThis, any* pOther)
		{
			switch (operation)
			{
			case storage_operation::GET:
			{
				return storage_strategy<T, StorageType>::get(rThis.storage_);
			}
			break;

			case storage_operation::DESTROY:
			{
				storage_strategy<T, StorageType>::destroy(rThis.storage_);
			}
			break;

			case storage_operation::COPY:
			{
				assert(pOther);
				storage_strategy<T, StorageType>::copyStorage(rThis.storage_, pOther->storage_);
			}
			break;

			case storage_operation::MOVE:
			{
				assert(pOther);
				storage_strategy<T, StorageType>::moveStorage(rThis.storage_, pOther->storage_);
			}
			break;

			case storage_operation::TYPE_INFO:
			{
				return (void*) & typeid(T);
			}
			break;

			default:
			{
				assert(false && "unknown storage operation");
			}
			break;
			};

			return nullptr;
		}
	};

	using storage_operator_ptr = void* (*) (storage_operation, any&, any*);

private:
	storage storage_;
	storage_operator_ptr storage_operator_{ nullptr };
};

#endif // !ANY_HPP
