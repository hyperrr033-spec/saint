#pragma once
#include "../core/common.hpp"
#include "math.hpp"

namespace lacz::schema {

// ---------------------------------------------------------------------------
// Minimal CS2 schema walker.
//
// CS2 exposes a CSchemaSystem through which every networked field is described
// (name, type, offset). Rather than hardcoding netvar offsets for one build, we
// enumerate the declared classes once and cache `Class::field -> offset`.
//
// The structures below mirror the Source 2 schema layout. They are version
// sensitive; the walker validates each level before descending so a bad layout
// degrades to "not found" instead of crashing.
// ---------------------------------------------------------------------------

struct schema_class_info_t {
    void* (*get_name)();
};

// CUtlTSHash bucket helpers are abstracted; we walk the flat vector form.
struct schema_declared_class_t {
    const char* name;
    const char* module_name;
    int         size;
    void*       fields;                 // SchemaClassFields*
    void*       base_classes;
    void*       type_scope;
};

struct schema_class_field_t {
    const char* name;
    const char* type;
    int         offset;
    int         metadata_size;
    void*       metadata;
};

struct schema_class_fields_t {
    char pad[0x8];
    schema_class_field_t* fields;
    int count;                          // *1000 in some builds
};

struct schema_type_scope_t {
    char pad[0x8];
    const char* name;
    char pad2[0x50];
    void* (*get_declared_classes)(void*);   // returns CUtlVector<DeclaredClass*>
    char pad3[0x8];
    void* (*get_declared_enums)(void*);
    // ...
};

struct utl_vector {
    int    count;
    int    capacity;
    void** memory;
    void*  grow_fn;
};

struct schema_system_t {
    char pad[0x190];
    void* type_scopes;                  // CUtlVector<TypeScope*>
};

bool initialize();

// Field lookup. `class_name` e.g. "C_CSPlayerPawn", `field` e.g. "m_iHealth".
int find_offset(const char* class_name, const char* field);
// Convenience wrapper that caches by combined key.
int field(const char* class_name, const char* field);

// Dump every known class/field to disk (debug aid).
void dump(const std::string& path);

} // namespace lacz::schema