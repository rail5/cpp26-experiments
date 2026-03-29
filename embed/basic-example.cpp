/**
 * Basic usage of the #embed preprocessor directive in C++26
 */

#include <iostream>

int main() {
	const unsigned char embedded_string[] = {
		#embed "hello.txt"
	};
	std::cout << embedded_string << std::endl;

	return 0;
}
