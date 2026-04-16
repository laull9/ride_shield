#pragma once

#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef RIDESHIELD_HAS_EMBEDDED_RESOURCES
#include "battery/embed.hpp"
#endif

namespace RideShield::resources {

using ResourceSpan = std::span<const unsigned char>;

namespace detail {

#ifdef RIDESHIELD_HAS_EMBEDDED_RESOURCES
inline auto make_span(const b::EmbedInternal::EmbeddedFile& file) noexcept -> ResourceSpan {
	return {
		reinterpret_cast<const unsigned char*>(file.data()),
		file.size(),
	};
}
#endif

}  // namespace detail

inline auto find(std::string_view resource_path) noexcept -> ResourceSpan {
#ifdef RIDESHIELD_HAS_EMBEDDED_RESOURCES
#include "rideshield_resources_find.inc"
#else
	(void)resource_path;
#endif
	return {};
}

inline auto get(std::string_view resource_path) -> ResourceSpan {
	const auto resource = find(resource_path);
	if (!resource.empty()) {
		return resource;
	}

	throw std::runtime_error("Embedded resource not found: " + std::string(resource_path));
}

#ifdef RIDESHIELD_HAS_EMBEDDED_RESOURCES
#include "rideshield_resources_accessors.inc"
#endif

}  // namespace RideShield::resources
