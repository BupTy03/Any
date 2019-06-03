#include <iostream>
#include <string>

#include "any.hpp"

template<class T>
void print(const any& val)
{
	using std::cout;
	using std::endl;

	bool is_valid_cast{ false };
	auto result = val.cast<T>(&is_valid_cast);
	if (is_valid_cast) {
		cout << result << endl;
	}
	else {
		cout << "ERROR!" << endl;
	}
}

int main()
{
	any a(23);

	any b{ std::move(a) };
	print<int>(b);

	any c = b;
	print<int>(c);

	any d{ std::string{"string"} };
	print<std::string>(d);

	any e{ std::move(d) };
	print<std::string>(e);

	return 0;
}