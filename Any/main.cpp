#include <iostream>
#include <string>
#include <vector>

#include "any.hpp"

template<class T>
void print_as(const any& val)
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

void print(const any& val)
{
	auto&& typeId = val.type();
	if (typeId == typeid(int)) {
		print_as<int>(val);
	}
	else if (typeId == typeid(char)) {
		print_as<char>(val);
	}
	else if (typeId == typeid(bool)) {
		print_as<bool>(val);
	}
	else if (typeId == typeid(std::string)) {
		print_as<std::string>(val);
	}
	else {
		std::cout << "Unknown type!\n";
	}
}

int main()
{
	std::vector<any> vec;
	vec.emplace_back(1);
	vec.emplace_back(true);
	vec.emplace_back('c');
	vec.emplace_back(std::string("string"));

	std::cout << std::boolalpha;

	for (auto&& val : vec) {
		print(val);
	}

	return 0;
}