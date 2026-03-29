/**
 * Basic example of reflection in C++26
 *
 * This simply takes a given struct/class and outputs the names of all non-static data members as strings.
 */

#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <meta>

template<typename T>
struct Reflect {
	static consteval auto member_name_views() {
		constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(
				^^T,
				std::meta::access_context::unchecked()
			)
		);

		std::array<std::string_view, members.size()> out{};
		for (std::size_t i = 0; i < members.size(); ++i) {
			auto id = std::meta::identifier_of(members[i]);
			auto ptr = std::define_static_string(id);
			out[i] = std::string_view(ptr, id.size());
		}
		return out;
	}

	static std::vector<std::string> get_member_names() {
		static constexpr auto views = member_name_views();

		std::vector<std::string> names;
		names.reserve(views.size());
		for (std::string_view sv : views) {
			names.emplace_back(sv);
		}
		return names;
	}
};

struct Person {
	std::string name;
	int age;
};

class Object {
	private:
		int property1;
		std::string property2;
};

int main() {
	std::vector<std::string> member_names = Reflect<Person>::get_member_names();
	std::cout << "Members of struct 'Person':" << std::endl;
	for (const auto& name : member_names) {
		std::cout << name << std::endl;
	}

	member_names = Reflect<Object>::get_member_names();
	std::cout << "Members of class 'Object':" << std::endl;
	for (const auto& name : member_names) {
		std::cout << name << std::endl;
	}
	return 0;
}
