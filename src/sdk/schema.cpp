#include "schema.hpp"
#include "../core/memory.hpp"
#include "../sdk/interfaces.hpp"
#include <fstream>

namespace lacz::schema {

static schema_system_t* system = nullptr;
static std::unordered_map<std::string, int> cache;

bool initialize() {
    system = reinterpret_cast<schema_system_t*>(iface::schema_system);
    if (!system)
        return false;
    if (!mem::is_readable(reinterpret_cast<uptr>(system), sizeof(void*)))
        return false;
    return true;
}

static int resolve_class(schema_type_scope_t* scope, const char* class_name,
                         const char* field) {
    if (!scope || !mem::is_readable(reinterpret_cast<uptr>(scope), sizeof(void*)))
        return 0;

    // Use the vtable call if present; otherwise fall back to the flat vector at
    // the standard offset (older/newer builds differ).
    utl_vector* classes = nullptr;
    if (scope->get_declared_classes) {
        // Some builds return a pointer to a generated vector by value; ours is
        // by-pointer. Guard with SEH.
        __try {
            classes = reinterpret_cast<utl_vector*>(scope->get_declared_classes(scope));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            classes = nullptr;
        }
    }
    if (!classes) {
        // Fallback: TypeScope + 0x2B0 holds the DeclaredClasses vector.
        classes = reinterpret_cast<utl_vector*>(
            reinterpret_cast<uptr>(scope) + 0x2B0);
    }
    if (!classes || !mem::is_readable(reinterpret_cast<uptr>(classes), sizeof(utl_vector)))
        return 0;

    for (int i = 0; i < classes->count; ++i) {
        if (!mem::is_readable(reinterpret_cast<uptr>(classes->memory) + i * sizeof(void*), sizeof(void*)))
            break;
        auto* declared = reinterpret_cast<schema_declared_class_t*>(classes->memory[i]);
        if (!declared || !mem::is_readable(reinterpret_cast<uptr>(declared), sizeof(schema_declared_class_t)))
            continue;

        if (!declared->name || std::strcmp(declared->name, class_name) != 0)
            continue;

        // Found exact class - search fields FIRST before base classes
        auto* fields = reinterpret_cast<schema_class_fields_t*>(declared->fields);
        if (fields && mem::is_readable(reinterpret_cast<uptr>(fields), sizeof(schema_class_fields_t))) {
            int count = fields->count > 100000 ? fields->count / 1000 : fields->count;
            for (int f = 0; f < count; ++f) {
                auto& fld = fields->fields[f];
                if (fld.name && std::strcmp(fld.name, field) == 0)
                    return fld.offset;
            }
        }

        // Only if not found in target class, recurse into base classes
        if (declared->base_classes) {
            auto* bases = reinterpret_cast<utl_vector*>(declared->base_classes);
            if (mem::is_readable(reinterpret_cast<uptr>(bases), sizeof(utl_vector))) {
                for (int b = 0; b < bases->count; ++b) {
                    auto* base = reinterpret_cast<schema_declared_class_t*>(bases->memory[b]);
                    if (base && mem::is_readable(reinterpret_cast<uptr>(base), sizeof(schema_declared_class_t))) {
                        int off = resolve_class(scope, base->name, field);
                        if (off)
                            return off;
                    }
                }
            }
        }
    }
    return 0;
}

int find_offset(const char* class_name, const char* field) {
    if (!system)
        return 0;
    auto* scopes = reinterpret_cast<utl_vector*>(
        reinterpret_cast<uptr>(system) + 0x190);
    if (!mem::is_readable(reinterpret_cast<uptr>(scopes), sizeof(utl_vector)))
        return 0;

    for (int i = 0; i < scopes->count; ++i) {
        auto* scope = reinterpret_cast<schema_type_scope_t*>(scopes->memory[i]);
        if (!scope)
            continue;
        int off = resolve_class(scope, class_name, field);
        if (off)
            return off;
    }
    return 0;
}

int field(const char* class_name, const char* field) {
    std::string key = std::string(class_name) + "::" + field;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    int off = find_offset(class_name, field);
    cache[key] = off;
    return off;
}

void dump(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open())
        return;
    auto* scopes = reinterpret_cast<utl_vector*>(
        reinterpret_cast<uptr>(system) + 0x190);
    if (!scopes || !mem::is_readable(reinterpret_cast<uptr>(scopes), sizeof(utl_vector)))
        return;
    for (int i = 0; i < scopes->count; ++i) {
        auto* scope = reinterpret_cast<schema_type_scope_t*>(scopes->memory[i]);
        if (!scope)
            continue;
        utl_vector* classes = reinterpret_cast<utl_vector*>(
            reinterpret_cast<uptr>(scope) + 0x2B0);
        if (!classes || !mem::is_readable(reinterpret_cast<uptr>(classes), sizeof(utl_vector)))
            continue;
        for (int c = 0; c < classes->count; ++c) {
            auto* declared = reinterpret_cast<schema_declared_class_t*>(classes->memory[c]);
            if (!declared || !declared->name)
                continue;
            out << declared->name << "\n";
            auto* fields = reinterpret_cast<schema_class_fields_t*>(declared->fields);
            if (!fields || !mem::is_readable(reinterpret_cast<uptr>(fields), sizeof(schema_class_fields_t)))
                continue;
            int count = fields->count > 100000 ? fields->count / 1000 : fields->count;
            for (int f = 0; f < count; ++f)
                out << "  +0x" << std::hex << fields->fields[f].offset
                    << " " << (fields->fields[f].name ? fields->fields[f].name : "?") << std::dec << "\n";
        }
    }
}

} // namespace lacz::schema