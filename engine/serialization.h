#pragma once

#include <variant>
#include <string>
#include <unordered_map>



namespace criogenio {
	using Variant = std::variant<int, float, bool, std::string>;

	struct SerializedComponent {
		std::string type;
		std::unordered_map<std::string, Variant> fields;
	};

	struct SerializedEntity {
		int id;
		std::string name;
		std::vector<SerializedComponent> components;
	};

	struct SerializedScene {
		std::string name;
		std::vector<SerializedEntity> entities;
	};
}

