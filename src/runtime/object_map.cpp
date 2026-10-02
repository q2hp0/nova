#include "object_map.h"

#include "../errors/errors.h"

std::string mapKeyOf(const Value& key) {
    if (key.isString()) {
        return key.asString();
    }

    if (key.isNumber()) {
        return formatNumber(key.asNumber());
    }

    if (key.isBool()) {
        return key.asBool() ? "true" : "false";
    }

    throw RuntimeError(
        "map keys must be string, number or bool"
    );
}

static std::string renderEntries(
    const std::vector<std::pair<std::string, Value>>& items,
    bool inspect
) {
    std::string out = "{";

    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }

        out += "\"" +
            items[i].first +
            "\": ";

        out += inspect
            ? items[i].second.inspect()
            : items[i].second.toString();
    }

    return out + "}";
}

MapObject::MapObject(
    std::vector<std::pair<std::string, Value>> entries
)
    : Object(ObjectType::Map),
      items(std::move(entries)) {
    for (size_t i = 0; i < items.size(); ++i) {
        position[items[i].first] = i;
    }
}

size_t MapObject::size() const {
    return items.size();
}

bool MapObject::contains(
    const std::string& key
) const {
    return position.find(key) != position.end();
}

Value MapObject::get(
    const std::string& key
) const {
    auto it = position.find(key);

    if (it == position.end()) {
        return Value();
    }

    return items[it->second].second;
}

void MapObject::set(
    const std::string& key,
    const Value& value
) {
    auto it = position.find(key);

    if (it == position.end()) {
        position[key] = items.size();
        items.emplace_back(key, value);
        return;
    }

    items[it->second].second = value;
}

bool MapObject::remove(const std::string& key) {
    auto it = position.find(key);

    if (it == position.end()) {
        return false;
    }

    size_t index = it->second;

    items.erase(items.begin() + static_cast<long>(index));
    position.erase(it);

    for (auto& entry : position) {
        if (entry.second > index) {
            entry.second--;
        }
    }

    return true;
}

void MapObject::clear() {
    items.clear();
    position.clear();
}

std::vector<std::string> MapObject::keys() const {
    std::vector<std::string> out;
    out.reserve(items.size());

    for (const auto& entry : items) {
        out.push_back(entry.first);
    }

    return out;
}

std::vector<Value> MapObject::values() const {
    std::vector<Value> out;
    out.reserve(items.size());

    for (const auto& entry : items) {
        out.push_back(entry.second);
    }

    return out;
}

const std::vector<std::pair<std::string, Value>>&
MapObject::entries() const {
    return items;
}

std::string MapObject::toString() const {
    return renderEntries(items, false);
}

std::string MapObject::inspect() const {
    return renderEntries(items, true);
}

std::string MapObject::typeName() const {
    return "map";
}
