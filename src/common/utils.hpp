#include <array>
#include <cstddef>
#include <string_view>

// concatenate multiple string views
template <const std::string_view&... strings>
struct join {
	static constexpr auto join_strings() {
		constexpr size_t length = (strings.size() + ... + 0);

		std::array<char, length+1> buffer = {};
		auto append = [i = 0u, &buffer](const auto& s) mutable {
			for (auto c : s) buffer[i++] = c;
		};
		(append(strings), ...);
		buffer.back() = 0;
		return buffer;
	}

	static constexpr auto buf = join_strings();
	static constexpr const char* value = buf.data();
};

template <const std::string_view&... strings>
static constexpr auto join_v = join<strings...>::value;
