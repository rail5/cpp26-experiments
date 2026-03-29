/**
 * Basic usage of contracts in C++26
 */

#include <iostream>
#include <contracts>

/**
 * Quote from P2900R14:
 *

The contract-violation handler is a function named ::handle_contract_violation that is attached
to the global module and has C++ language linkage. This function will be invoked when a contract
violation is identified at run time.

This function
• shall take a single argument of type const std::contracts::contract_violation&
• shall return void
• may be 

The implementation shall provide a definition of this function, which is called the default contract-
violation handler and has implementation-defined effects. The recommended practice is that the
default contract-violation handler will output diagnostic information describing the pertinent prop-
erties of the provided std::contracts::contract_violation object.

 * 
 * In other words, the particular contract-violation handler is implementation-defined
 *
 * More from P2900R14:

Whether ::handle_contract_violation is replaceable is implementation-defined. When it is replace-
able, that replacement is done in the same way it would be done for the global operator new and
operator delete, i.e., by defining a function that has the correct signature (function name and
argument types), has the correct return type, and satisfies the requirements listed above. Such a
function is called a user-defined contract-violation handler.

A user-provided contract-violation handler may have any exception specification; i.e., it is free to
be noexcept(true) or noexcept(false). Enabling this flexibility is a primary motivation for not
providing any declaration of ::handle_contract_violation in the Standard Library; whether that
declaration was noexcept would force that decision on user-provided contract-violation handlers, like
it does for the global operator new and operator delete, which have declarations that are noexcept
provided in the Standard Library.

 *
 * So: whether or not we're allowed to override it is also implementation-defined.
 * Well, in the meantime, we'll just override it and see if it works.
 */
void handle_contract_violation(const std::contracts::contract_violation& violation) {
	std::cerr << "Contract violation" << std::endl;
	std::terminate();
}



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
