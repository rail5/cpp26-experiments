/**
 * Basic usage of contracts in C++26
 */

#include <iostream>
#include <contracts>

void print_positive(int x)
	pre (x > 0) // Precondition: x must be greater than 0
{
	std::cout << "The number is: " << x << std::endl;
}

int main() {
	print_positive(5); // This will work fine
	print_positive(-3); // This will trigger the contract violation

	return 0;
}
