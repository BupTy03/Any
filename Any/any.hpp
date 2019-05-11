#ifndef ANY_HPP
#define ANY_HPP

#include <iostream>
#include <exception>
#include <type_traits>

struct any
{
	explicit any() noexcept {}

	template<class T, class = std::enable_if_t<!std::is_same_v<std::decay_t<T>, any>>>
	explicit any(T&& val)
	{
		holder* tmp_value{ nullptr };
		try { // if constructor of T class throws exception
			tmp_value = new value_holder<T>{ std::forward<T>(val) };
		}
		catch (const std::exception & exc) {
			// freeing memory and rethrow exception outwards
			if (tmp_value != nullptr) {
				delete tmp_value;
				tmp_value = nullptr;
			}
			throw exc;
		}
		value_ = tmp_value;
	}

	template<class T, class... Args>
	static any make(Args&&... args)
	{
		any result;
		holder* tmp_value{ nullptr };
		try { // if constructor of T class throws exception
			tmp_value = new value_holder<T>{ std::forward<Args>(args)... };
		}
		catch (const std::exception& exc) {
			// freeing memory and rethrow exception outwards
			if (tmp_value != nullptr) {
				delete tmp_value;
				tmp_value = nullptr;
			}
			throw exc;
		}
		result.value_ = tmp_value;
		return result;
	}

	~any() { reset(); }

	any(const any& other) : value_{ (other.value_)->clone() } { }
	any& operator=(const any& other)
	{
		if (this == &other) {
			return *this;
		}
		auto tmp_value = (other.value_)->clone();
		reset();
		value_ = tmp_value;
		return *this;
	}

	any(any&& other) noexcept { std::swap(value_, other.value_); }
	any& operator=(any&& other) noexcept
	{
		if (this == &other) {
			return *this;
		}
		std::swap(value_, other.value_);
		return *this;
	}

	template<class T, class... Args>
	void emplace(Args&&... val)
	{
		holder* tmp{ nullptr };
		try {
			tmp = new value_holder<T>(std::forward<Args>(val)...);
		}
		catch (const std::exception& exc) {
			delete tmp;
			throw exc;
		}
		reset();
		value_ = tmp;
	}
	void reset()
	{
		if (has_value()) {
			delete value_;
		}
		value_ = nullptr;
	}

	void swap(any& other)
	{
		if (this == &other) {
			return;
		}
		std::swap(value_, other.value_);
	}

	bool has_value() const { return value_ != nullptr; }
	const int type() const { return value_->get_type_id(); }

	template<class T>
	T cast(bool* success = nullptr) const
	{
		if (!has_value() || typeid(T).hash_code() != value_->get_type_id()) {
			if (success == nullptr) {
				throw std::exception{ "bad any cast" };
			}
			else {
				*success = false;
				return T{};
			}
		}
		if (success != nullptr) {
			*success = true;
		}
		return (static_cast<value_holder<T>*>(value_))->get_value();
	}

private:
	struct holder
	{
		holder(const std::size_t type_v) : type_val_id_{ type_v } {}
		virtual ~holder() {}

		virtual holder* clone() const = 0;

		std::size_t get_type_id() const { return type_val_id_; }
	private:
		const std::size_t type_val_id_;
	};

	template<class T>
	struct value_holder : holder
	{
		template<class... Args>
		value_holder(Args&&... args)
			: holder{ typeid(T).hash_code() }
			, val_{ std::forward<Args>(args)... } {}

		T get_value() const { return val_; }

		virtual holder* clone() const override
		{
			holder* result{ nullptr };
			try { // if constructor of T class throws exception
				result = new value_holder(val_);
			}
			catch (const std::exception & exc) {
				// freeing memory and rethrow exception outwards
				if (result != nullptr) {
					delete result;
					result = nullptr;
				}
				throw exc;
			}
			return result;
		}

	private:
		T val_;
	};

	holder* value_{nullptr};
};

#endif // !ANY_HPP
