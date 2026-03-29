/**
 * This one provides a "universal reset" function that can be used
 * to reset all non-static (public) data members of a struct/class to their default values.
 *
 * The default values are the same as given in the class definition
 *		(e.g., if the class defines `int x = 6;` as a member, the "reset" sets `x = 6`, not `x = 0`)
 *
 * Problem: if the class contains *private* data members, this will not compile,
 * because the generated code for resetting those members will not have access to them.
 */

#include <iostream>
#include <meta>
#include <array>
#include <string>
#include <utility>

template <class T, auto MemberPtr>
void reset_one_member_ptr(T& dst, const T& src) {
	dst.*MemberPtr = src.*MemberPtr;
}

template <class T>
consteval auto reset_info() {
	using action_t = void (*)(T&, const T&);
	constexpr auto members = std::define_static_array(
		std::meta::nonstatic_data_members_of(
			^^T,
			std::meta::access_context::unchecked()
		)
	);

	return [&]<std::size_t... I>(std::index_sequence<I...>) {
		return std::array<action_t, sizeof...(I)>{
			&reset_one_member_ptr<T, &[:members[I]:] >...
		};
	}(std::make_index_sequence<members.size()>{});
}

template <class T>
void reset(T& obj) {
	T def{};
	using action_t = void (*)(T&, const T&);
	constexpr std::array<action_t, reset_info<T>().size()> plan = reset_info<T>();
	for (auto action : plan)
		action(obj, def);
}

struct Object {
	int x = 7;
	std::string name = "Default";
};

int main() {
	Object obj;

	std::cout << "Initial state: x = " << obj.x << ", name = " << obj.name << std::endl;

	obj.x = 42;
	obj.name = "Hello";

	std::cout << "Before reset: x = " << obj.x << ", name = " << obj.name << std::endl;

	reset(obj);

	std::cout << "After reset: x = " << obj.x << ", name = " << obj.name << std::endl;

	return 0;
}
