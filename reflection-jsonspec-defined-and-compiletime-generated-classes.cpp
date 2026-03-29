/**
 * Experiment: Putting together Reflection, #embed, and Contracts
 *
 * The goal:
 *  1. Read a JSON "spec" file (res/spec.json) that describes some class definitions (#embed)
 *  1.5. Compile-time parsing of the JSON file to extract the class definitions
 *  2. Use static reflection to generate class definitions based on the spec
 *  3. Use contracts to enforce some invariants on the generated classes
 *
 * This one is a **much** bigger deal. We're doing some really powerful things here.
 * One of my major projects is the Bash++ compiler and language server, which is C++23.
 *  -- that language server generates classes based on the LSP metaModel specification
 *     as a separate compilation phase, before '#include'ing them in the language server
 *     code to use as a message types, etc.
 * With C++26 however, we should be able to embed the LSP metaModel spec directly,
 * and use it to generate the message classes at compile time, without needing a separate codegen phase.
 * This experiment is a smaller-scale proof of concept for that.
 */

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <contracts>

constexpr const char spec_json[] = {
	#embed "res/spec.json"
};

namespace spec
{
	using std::string_view;

	enum class value_kind {
		null,
		boolean,
		number,
		string,
	};

	struct value {
		value_kind kind{};
		string_view raw{}; // for number/string, this is the raw token content (no surrounding quotes for strings)
		bool boolean_value{};
	};

	struct member {
		string_view name;
		string_view type;
		value default_value;
		string_view access;
	};

	struct klass {
		string_view name;
		std::size_t members_begin{};
		std::size_t members_count{};
	};

	template <std::size_t MaxClasses, std::size_t MaxMembers>
	struct parsed_spec {
		std::array<klass, MaxClasses> classes{};
		std::size_t classes_count{};

		std::array<member, MaxMembers> members{};
		std::size_t members_count{};

		consteval const klass* find_class(string_view class_name) const {
			for (std::size_t i = 0; i < classes_count; ++i) {
				if (classes[i].name == class_name)
					return &classes[i];
			}
			return nullptr;
		}

		consteval std::size_t find_class_index(string_view class_name) const {
			for (std::size_t i = 0; i < classes_count; ++i) {
				if (classes[i].name == class_name)
					return i;
			}
			return static_cast<std::size_t>(-1);
		}

		consteval string_view class_name_by_index(std::size_t idx) const {
			return classes[idx].name;
		}

		consteval const member& member_at(std::size_t idx) const {
			return members[idx];
		}
	};

	struct cursor {
		string_view text;
		std::size_t i{};

		consteval bool eof() const { return i >= text.size(); }
		consteval char peek() const { return eof() ? '\0' : text[i]; }
		consteval void bump() { if (!eof()) ++i; }

		consteval void skip_ws() {
			while (!eof()) {
				char c = peek();
				if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
					bump();
				else
					break;
			}
		}

		consteval bool consume(char c) {
			skip_ws();
			if (peek() == c) {
				bump();
				return true;
			}
			return false;
		}

		consteval void expect(char c) {
			skip_ws();
			if (peek() != c)
				throw std::meta::exception(u8"JSON parse error: expected character", ^^expect);
			bump();
		}

		consteval bool starts_with(string_view s) const {
			return text.substr(i).starts_with(s);
		}

		consteval void expect_keyword(string_view kw) {
			skip_ws();
			if (!starts_with(kw))
				throw std::meta::exception(u8"JSON parse error: expected keyword", ^^expect_keyword);
			i += kw.size();
		}

		// Parses a JSON string and returns a view of the raw contents (no surrounding quotes),
		// i.e. escape sequences are NOT unescaped here.
		consteval string_view parse_string_raw() {
			skip_ws();
			expect('"');
			std::size_t start = i;
			while (!eof()) {
				char c = peek();
				if (c == '"') {
					std::size_t end = i;
					bump();
					return text.substr(start, end - start);
				}
				if (c == '\\') {
					bump();
					if (eof())
						throw std::meta::exception(u8"JSON parse error: dangling escape", ^^parse_string_raw);
					bump();
					continue;
				}
				bump();
			}
			throw std::meta::exception(u8"JSON parse error: unterminated string", ^^parse_string_raw);
		}

		consteval value parse_value() {
			skip_ws();
			char c = peek();
			if (c == '"') {
				return value{ value_kind::string, parse_string_raw(), false };
			}
			if (c == 'n') {
				expect_keyword("null");
				return value{ value_kind::null, {}, false };
			}
			if (c == 't') {
				expect_keyword("true");
				return value{ value_kind::boolean, {}, true };
			}
			if (c == 'f') {
				expect_keyword("false");
				return value{ value_kind::boolean, {}, false };
			}
			// number (very permissive; just slice it)
			if (c == '-' || (c >= '0' && c <= '9')) {
				std::size_t start = i;
				bump();
				while (!eof()) {
					char d = peek();
					if ((d >= '0' && d <= '9') || d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-')
						bump();
					else
						break;
				}
				return value{ value_kind::number, text.substr(start, i - start), false };
			}
			throw std::meta::exception(u8"JSON parse error: unsupported value", ^^parse_value);
		}

		// Skip over any JSON value without building an AST.
		consteval void skip_any_value() {
			skip_ws();
			char c = peek();
			if (c == '{') {
				expect('{');
				skip_ws();
				if (consume('}'))
					return;
				for (;;) {
					(void)parse_string_raw();
					expect(':');
					skip_any_value();
					skip_ws();
					if (consume('}'))
						return;
					expect(',');
				}
			}
			if (c == '[') {
				expect('[');
				skip_ws();
				if (consume(']'))
					return;
				for (;;) {
					skip_any_value();
					skip_ws();
					if (consume(']'))
						return;
					expect(',');
				}
			}
			(void)parse_value();
		}
	};

	consteval bool is_identifier_char(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
	}

	// Minimal JSON-string unescape (handles the escapes used in this repo's specs).
	// Total function: on malformed input it just stops unescaping.
	inline std::string unescape_json_string(string_view raw) {
		std::string out;
		out.reserve(raw.size());
		for (std::size_t j = 0; j < raw.size(); ++j) {
			char c = raw[j];
			if (c != '\\') {
				out.push_back(c);
				continue;
			}
			if (j + 1 >= raw.size())
				break;
			char e = raw[++j];
			switch (e) {
				case '"': out.push_back('"'); break;
				case '\\': out.push_back('\\'); break;
				case '/': out.push_back('/'); break;
				case 'b': out.push_back('\b'); break;
				case 'f': out.push_back('\f'); break;
				case 'n': out.push_back('\n'); break;
				case 'r': out.push_back('\r'); break;
				case 't': out.push_back('\t'); break;
				default:
					// Keep the escaped character verbatim.
					out.push_back(e);
			}
		}
		return out;
	}

	consteval parsed_spec<8, 64> parse(string_view json) {
		parsed_spec<8, 64> out{};
		cursor cur{ json, 0 };

		cur.expect('{');
		cur.skip_ws();
		if (cur.consume('}'))
			return out;

		for (;;) {
			string_view key = cur.parse_string_raw();
			cur.expect(':');
			if (key == "classes") {
				cur.expect('[');
				cur.skip_ws();
				if (!cur.consume(']')) {
					for (;;) {
						cur.expect('{');
						klass k{};
						k.members_begin = out.members_count;
						k.members_count = 0;

						cur.skip_ws();
						if (!cur.consume('}')) {
							for (;;) {
								string_view ckey = cur.parse_string_raw();
								cur.expect(':');
								if (ckey == "name") {
									k.name = cur.parse_string_raw();
								} else if (ckey == "members") {
									cur.expect('[');
									cur.skip_ws();
									if (!cur.consume(']')) {
										for (;;) {
											cur.expect('{');
											member m{};
											cur.skip_ws();
											if (!cur.consume('}')) {
												for (;;) {
													string_view mkey = cur.parse_string_raw();
													cur.expect(':');
													if (mkey == "name")
														m.name = cur.parse_string_raw();
													else if (mkey == "type")
														m.type = cur.parse_string_raw();
													else if (mkey == "default_value")
														m.default_value = cur.parse_value();
													else if (mkey == "access")
														m.access = cur.parse_string_raw();
													else
														cur.skip_any_value();

													cur.skip_ws();
													if (cur.consume('}'))
														break;
													cur.expect(',');
												}
											}

											if (out.members_count >= out.members.size())
												throw std::meta::exception(u8"Spec too large: too many members", ^^parse);
											out.members[out.members_count++] = m;
											++k.members_count;

											cur.skip_ws();
											if (cur.consume(']'))
												break;
											cur.expect(',');
										}
									}
								} else {
									cur.skip_any_value();
								}

								cur.skip_ws();
								if (cur.consume('}'))
									break;
								cur.expect(',');
							}
						}

						if (out.classes_count >= out.classes.size())
							throw std::meta::exception(u8"Spec too large: too many classes", ^^parse);
						out.classes[out.classes_count++] = k;

						cur.skip_ws();
						if (cur.consume(']'))
							break;
						cur.expect(',');
					}
				}
			} else {
				cur.skip_any_value();
			}

			cur.skip_ws();
			if (cur.consume('}'))
				break;
			cur.expect(',');
		}

		return out;
	}
} // namespace spec

// These are the names the spec is expected to generate.
// Note: `std::meta::define_aggregate` needs an incomplete class type to complete.
struct MyClass1;
struct MyClass2;

namespace generated
{
	consteval std::meta::info resolve_type(std::string_view type_name) {
		if (type_name == "int") return ^^int;
		if (type_name == "double") return ^^double;
		if (type_name == "std::string") return ^^std::string;
		if (type_name == "std::vector<int>") return ^^std::vector<int>;
		if (type_name == "std::map<std::string, int>") return ^^std::map<std::string, int>;
		throw std::meta::exception(u8"Unsupported type string in spec", ^^resolve_type);
	}

	consteval const spec::klass& require_class(const spec::parsed_spec<8, 64>& s, std::string_view name) {
		if (auto* k = s.find_class(name))
			return *k;
		throw std::meta::exception(u8"Spec missing required class", ^^require_class);
	}

	consteval std::vector<std::meta::info> build_member_specs(
		const spec::parsed_spec<8, 64>& s,
		const spec::klass& k
	) {
		std::vector<std::meta::info> out;
		out.reserve(k.members_count);
		for (std::size_t idx = 0; idx < k.members_count; ++idx) {
			const auto& m = s.member_at(k.members_begin + idx);
			auto type = resolve_type(m.type);

			std::meta::data_member_options opts{};
			opts.name = std::meta::data_member_options::_Name{std::string(m.name)};

			out.push_back(std::meta::data_member_spec(type, opts));
		}
		return out;
	}

	template <class T>
	consteval const spec::member& find_member_spec(
		const spec::parsed_spec<8, 64>& s,
		const spec::klass& k,
		std::string_view member_name
	) {
		for (std::size_t idx = 0; idx < k.members_count; ++idx) {
			const auto& m = s.member_at(k.members_begin + idx);
			if (m.name == member_name)
				return m;
		}
		throw std::meta::exception(u8"Spec missing required member", ^^find_member_spec<T>);
	}

	constexpr long long parse_int_or(std::string_view sv, long long fallback = 0) {
		bool neg = false;
		std::size_t i = 0;
		if (i < sv.size() && sv[i] == '-') { neg = true; ++i; }
		if (i >= sv.size() || sv[i] < '0' || sv[i] > '9')
			return fallback;
		long long v = 0;
		for (; i < sv.size(); ++i) {
			char c = sv[i];
			if (c < '0' || c > '9')
				return fallback;
			v = v * 10 + (c - '0');
		}
		return neg ? -v : v;
	}

	constexpr double parse_double_or(std::string_view sv, double fallback = 0.0) {
		// `from_chars` for floating point isn't constexpr everywhere yet, so we keep this very small
		// and use a simple manual parser for the forms in `res/spec.json` (e.g. 3.14).
		bool neg = false;
		std::size_t i = 0;
		if (i < sv.size() && sv[i] == '-') { neg = true; ++i; }
		double int_part = 0.0;
		bool saw_digit = false;
		while (i < sv.size() && sv[i] >= '0' && sv[i] <= '9') {
			saw_digit = true;
			int_part = int_part * 10.0 + (sv[i] - '0');
			++i;
		}
		double frac_part = 0.0;
		double frac_scale = 1.0;
		if (i < sv.size() && sv[i] == '.') {
			++i;
			while (i < sv.size() && sv[i] >= '0' && sv[i] <= '9') {
				saw_digit = true;
				frac_part = frac_part * 10.0 + (sv[i] - '0');
				frac_scale *= 10.0;
				++i;
			}
		}
		if (!saw_digit || i != sv.size())
			return fallback;
		double v = int_part + (frac_part / frac_scale);
		return neg ? -v : v;
	}

	template <class T>
	T parse_default_value(const spec::value& v) {
		using U = std::remove_cvref_t<T>;
		if constexpr (std::is_same_v<U, int>) {
			if (v.kind == spec::value_kind::number)
				return static_cast<int>(parse_int_or(v.raw, 0));
			if (v.kind == spec::value_kind::string)
				return static_cast<int>(parse_int_or(spec::unescape_json_string(v.raw), 0));
			return 0;
		} else if constexpr (std::is_same_v<U, double>) {
			if (v.kind == spec::value_kind::number)
				return parse_double_or(v.raw, 0.0);
			if (v.kind == spec::value_kind::string)
				return parse_double_or(spec::unescape_json_string(v.raw), 0.0);
			return 0.0;
		} else if constexpr (std::is_same_v<U, std::string>) {
			if (v.kind != spec::value_kind::string)
				return {};
			std::string s = spec::unescape_json_string(v.raw);
			// The spec stores a C++-ish string literal for string defaults (e.g. "\"hello\"").
			if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
				return s.substr(1, s.size() - 2);
			return s;
		} else {
			// For containers/etc we accept "{}" (or any string) as "default-construct".
			return U{};
		}
	}

	template <class T>
	T make_default_from_spec() {
		static constexpr std::string_view json_text{ spec_json, sizeof(spec_json) };
		static constexpr auto parsed = spec::parse(json_text);
		constexpr std::size_t kidx = parsed.find_class_index(std::meta::identifier_of(^^T));
		static_assert(kidx != static_cast<std::size_t>(-1), "No class in spec for this type");
		constexpr spec::klass k = parsed.classes[kidx];

		T out{};
		static constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())
		);

		template for (constexpr auto mem : members) {
			constexpr auto mem_name = std::meta::identifier_of(mem);
			constexpr auto& mem_spec = find_member_spec<T>(parsed, k, mem_name);
			using member_t = std::remove_cvref_t<decltype(out.[:mem:])>;
			out.[:mem:] = parse_default_value<member_t>(mem_spec.default_value);
		}

		return out;
	}

	template <class T>
	bool equals_defaults(const T& obj) {
		T def = make_default_from_spec<T>();
		bool ok = true;
		static constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())
		);
		template for (constexpr auto mem : members) {
			ok = ok && (obj.[:mem:] == def.[:mem:]);
		}
		return ok;
	}

	template <class T>
	bool invariants(const T& obj) {
		bool ok = true;
		static constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())
		);
		template for (constexpr auto mem : members) {
			using member_t = std::remove_cvref_t<decltype(obj.[:mem:])>;
			if constexpr (std::is_arithmetic_v<member_t>) {
				if constexpr (std::is_signed_v<member_t>)
					ok = ok && (obj.[:mem:] >= 0);
			} else if constexpr (std::is_same_v<member_t, std::string>) {
				ok = ok && !obj.[:mem:].empty();
			} else if constexpr (requires (const member_t& x) { x.size(); }) {
				ok = ok && (obj.[:mem:].size() <= 1024);
			}
		}
		return ok;
	}

	template <class T, auto MemberPtr>
	void copy_one_member(T& dst, const T& src) {
		dst.*MemberPtr = src.*MemberPtr;
	}

	template <class T>
	consteval auto reset_plan() {
		using action_t = void (*)(T&, const T&);
		constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())
		);
		return [&]<std::size_t... I>(std::index_sequence<I...>) {
			return std::array<action_t, sizeof...(I)>{
				&copy_one_member<T, &[:members[I]:] >...
			};
		}(std::make_index_sequence<members.size()>{});
	}

	template <class T>
	void reset_to_defaults(T& obj)
		post (equals_defaults(obj))
	{
		T def = make_default_from_spec<T>();
		using action_t = void (*)(T&, const T&);
		constexpr std::array<action_t, reset_plan<T>().size()> plan = reset_plan<T>();
		for (auto action : plan)
			action(obj, def);
	}

	template <class T>
	void use(const T& obj)
		pre (invariants(obj))
	{
		// pretend to do something meaningful
		(void)obj;
	}

	template <class T>
	void dump(const T& obj, std::string_view header) {
		auto print_value = [](const auto& v) {
			using V = std::remove_cvref_t<decltype(v)>;
			if constexpr (requires (std::ostream& os, const V& x) { os << x; }) {
				std::cout << v;
			} else if constexpr (requires (const V& x) { x.size(); }) {
				std::cout << "<size=" << v.size() << ">";
			} else {
				std::cout << "<unprintable>";
			}
		};

		std::cout << header << "\n";
		static constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())
		);
		template for (constexpr auto mem : members) {
			std::cout << "  " << std::meta::identifier_of(mem) << " = ";
			print_value(obj.[:mem:]);
			std::cout << "\n";
		}
	}
} // namespace generated

// The generation itself: parse at compile time and define the aggregates.
consteval {
	constexpr std::string_view json_text{ spec_json, sizeof(spec_json) };
	constexpr auto parsed = spec::parse(json_text);

	const auto& c1 = generated::require_class(parsed, "MyClass1");
	const auto& c2 = generated::require_class(parsed, "MyClass2");

	std::meta::define_aggregate(^^MyClass1, generated::build_member_specs(parsed, c1));
	std::meta::define_aggregate(^^MyClass2, generated::build_member_specs(parsed, c2));
}

int main() {
	MyClass1 a{};
	MyClass2 b{};

	generated::dump(a, "Initial MyClass1 (aggregate default init)");
	generated::dump(b, "Initial MyClass2 (aggregate default init)");

	generated::reset_to_defaults(a);
	generated::reset_to_defaults(b);

	generated::dump(a, "After reset_to_defaults(MyClass1)");
	generated::dump(b, "After reset_to_defaults(MyClass2)");

	std::cout << "We have now compile-time generated a type based on a JSON specification."
		<< std::endl
		<< "We will now read a data member from that type without the source code having explicitly"
		<< " defined the class header:" << std::endl;
	std::cout << "a.x: " << a.x << std::endl << std::endl;

	std::cout << "We are now going to intentionally trigger a contract violation." << std::endl;

	// Now put them into a "bad" state and intentionally trigger a contract violation.
	a.x = -1;
	a.y = "";
	b.a = std::vector<int>(2000, 1);
	generated::use(a);

	return 0;
}

